/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam and QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "FeatureExtractionModelTransformer.h"

#include "plugin/FeatureExtractionPluginFactory.h"
#include "plugin/PluginXml.h"
#include "vamp-sdk/Plugin.h"

#include "data/model/Model.h"
#include "base/Window.h"
#include "data/model/SparseOneDimensionalModel.h"
#include "data/model/SparseTimeValueModel.h"
#include "data/model/EditableDenseThreeDimensionalModel.h"
#include "data/model/DenseTimeValueModel.h"
#include "data/model/NoteModel.h"
#include "data/model/FFTModel.h"
#include "data/model/WaveFileModel.h"

#include "TransformFactory.h"

#include <QMessageBox>

#include <iostream>

FeatureExtractionModelTransformer::FeatureExtractionModelTransformer(Input in,
                                                                     const Transform &transform) :
    ModelTransformer(in, transform),
    m_plugin(0),
    m_descriptor(0),
    m_outputFeatureNo(0)
{
//    std::cerr << "FeatureExtractionModelTransformer::FeatureExtractionModelTransformer: plugin " << pluginId.toStdString() << ", outputName " << m_transform.getOutput().toStdString() << std::endl;

    QString pluginId = transform.getPluginIdentifier();

    FeatureExtractionPluginFactory *factory =
	FeatureExtractionPluginFactory::instanceFor(pluginId);

    if (!factory) {
        m_message = tr("No factory available for feature extraction plugin id \"%1\" (unknown plugin type, or internal error?)").arg(pluginId);
	return;
    }

    DenseTimeValueModel *input = getConformingInput();
    if (!input) {
        m_message = tr("Input model for feature extraction plugin \"%1\" is of wrong type (internal error?)").arg(pluginId);
        return;
    }

    m_plugin = factory->instantiatePlugin(pluginId, input->getSampleRate());
    if (!m_plugin) {
        m_message = tr("Failed to instantiate plugin \"%1\"").arg(pluginId);
	return;
    }

    TransformFactory::getInstance()->makeContextConsistentWithPlugin
        (m_transform, m_plugin);

    TransformFactory::getInstance()->setPluginParameters
        (m_transform, m_plugin);

    size_t channelCount = input->getChannelCount();
    if (m_plugin->getMaxChannelCount() < channelCount) {
	channelCount = 1;
    }
    if (m_plugin->getMinChannelCount() > channelCount) {
        m_message = tr("Cannot provide enough channels to feature extraction plugin \"%1\" (plugin min is %2, max %3; input model has %4)")
            .arg(pluginId)
            .arg(m_plugin->getMinChannelCount())
            .arg(m_plugin->getMaxChannelCount())
            .arg(input->getChannelCount());
	return;
    }

    std::cerr << "Initialising feature extraction plugin with channels = "
              << channelCount << ", step = " << m_transform.getStepSize()
              << ", block = " << m_transform.getBlockSize() << std::endl;

    if (!m_plugin->initialise(channelCount,
                              m_transform.getStepSize(),
                              m_transform.getBlockSize())) {

        size_t pstep = m_transform.getStepSize();
        size_t pblock = m_transform.getBlockSize();

        m_transform.setStepSize(0);
        m_transform.setBlockSize(0);
        TransformFactory::getInstance()->makeContextConsistentWithPlugin
            (m_transform, m_plugin);

        if (m_transform.getStepSize() != pstep ||
            m_transform.getBlockSize() != pblock) {
            
            if (!m_plugin->initialise(channelCount,
                                      m_transform.getStepSize(),
                                      m_transform.getBlockSize())) {

                m_message = tr("Failed to initialise feature extraction plugin \"%1\"").arg(pluginId);
                return;

            } else {

                m_message = tr("Feature extraction plugin \"%1\" rejected the given step and block sizes (%2 and %3); using plugin defaults (%4 and %5) instead")
                    .arg(pluginId)
                    .arg(pstep)
                    .arg(pblock)
                    .arg(m_transform.getStepSize())
                    .arg(m_transform.getBlockSize());
            }

        } else {

            m_message = tr("Failed to initialise feature extraction plugin \"%1\"").arg(pluginId);
            return;
        }
    }

    Vamp::Plugin::OutputList outputs = m_plugin->getOutputDescriptors();

    if (outputs.empty()) {
        m_message = tr("Plugin \"%1\" has no outputs").arg(pluginId);
	return;
    }
    
    for (size_t i = 0; i < outputs.size(); ++i) {
	if (m_transform.getOutput() == "" ||
            outputs[i].identifier == m_transform.getOutput().toStdString()) {
	    m_outputFeatureNo = i;
	    m_descriptor = new Vamp::Plugin::OutputDescriptor
		(outputs[i]);
	    break;
	}
    }

    if (!m_descriptor) {
        m_message = tr("Plugin \"%1\" has no output named \"%2\"")
            .arg(pluginId)
            .arg(m_transform.getOutput());
	return;
    }

//    std::cerr << "FeatureExtractionModelTransformer: output sample type "
//	      << m_descriptor->sampleType << std::endl;

    int binCount = 1;
    float minValue = 0.0, maxValue = 0.0;
    bool haveExtents = false;
    
    if (m_descriptor->hasFixedBinCount) {
	binCount = m_descriptor->binCount;
    }

//    std::cerr << "FeatureExtractionModelTransformer: output bin count "
//	      << binCount << std::endl;

    if (binCount > 0 && m_descriptor->hasKnownExtents) {
	minValue = m_descriptor->minValue;
	maxValue = m_descriptor->maxValue;
        haveExtents = true;
    }

    size_t modelRate = input->getSampleRate();
    size_t modelResolution = 1;
    
    switch (m_descriptor->sampleType) {

    case Vamp::Plugin::OutputDescriptor::VariableSampleRate:
	if (m_descriptor->sampleRate != 0.0) {
	    modelResolution = size_t(modelRate / m_descriptor->sampleRate + 0.001);
	}
	break;

    case Vamp::Plugin::OutputDescriptor::OneSamplePerStep:
	modelResolution = m_transform.getStepSize();
	break;

    case Vamp::Plugin::OutputDescriptor::FixedSampleRate:
	modelRate = size_t(m_descriptor->sampleRate + 0.001);
	break;
    }

    if (binCount == 0) {

	m_output = new SparseOneDimensionalModel(modelRate, modelResolution,
						 false);

    } else if (binCount == 1) {

        SparseTimeValueModel *model;
        if (haveExtents) {
            model = new SparseTimeValueModel
                (modelRate, modelResolution, minValue, maxValue, false);
        } else {
            model = new SparseTimeValueModel
                (modelRate, modelResolution, false);
        }
        model->setScaleUnits(outputs[m_outputFeatureNo].unit.c_str());

        m_output = model;

    } else if (m_descriptor->sampleType ==
	       Vamp::Plugin::OutputDescriptor::VariableSampleRate) {

        // We don't have a sparse 3D model, so interpret this as a
        // note model.  There's nothing to define which values to use
        // as which parameters of the note -- for the moment let's
        // treat the first as pitch, second as duration in frames,
        // third (if present) as velocity. (Our note model doesn't
        // yet store velocity.)
        //!!! todo: ask the user!
	
        NoteModel *model;
        if (haveExtents) {
            model = new NoteModel
                (modelRate, modelResolution, minValue, maxValue, false);
        } else {
            model = new NoteModel
                (modelRate, modelResolution, false);
        }            
        model->setScaleUnits(outputs[m_outputFeatureNo].unit.c_str());

        m_output = model;

    } else {

        EditableDenseThreeDimensionalModel *model =
            new EditableDenseThreeDimensionalModel
            (modelRate, modelResolution, binCount, false);

	if (!m_descriptor->binNames.empty()) {
	    std::vector<QString> names;
	    for (size_t i = 0; i < m_descriptor->binNames.size(); ++i) {
		names.push_back(m_descriptor->binNames[i].c_str());
	    }
	    model->setBinNames(names);
	}
        
        m_output = model;
    }

    if (m_output) m_output->setSourceModel(input);
}

FeatureExtractionModelTransformer::~FeatureExtractionModelTransformer()
{
    std::cerr << "FeatureExtractionModelTransformer::~FeatureExtractionModelTransformer()" << std::endl;
    delete m_plugin;
    delete m_descriptor;
}

DenseTimeValueModel *
FeatureExtractionModelTransformer::getConformingInput()
{
    DenseTimeValueModel *dtvm =
	dynamic_cast<DenseTimeValueModel *>(getInputModel());
    if (!dtvm) {
	std::cerr << "FeatureExtractionModelTransformer::getConformingInput: WARNING: Input model is not conformable to DenseTimeValueModel" << std::endl;
    }
    return dtvm;
}

void
FeatureExtractionModelTransformer::run()
{
    DenseTimeValueModel *input = getConformingInput();
    if (!input) return;

    if (!m_output) return;

    while (!input->isReady()) {
/*
        if (dynamic_cast<WaveFileModel *>(input)) {
            std::cerr << "FeatureExtractionModelTransformer::run: Model is not ready, but it's not a WaveFileModel (it's a " << typeid(input).name() << "), so that's OK" << std::endl;
            sleep(2);
            break; // no need to wait
        }
*/
        std::cerr << "FeatureExtractionModelTransformer::run: Waiting for input model to be ready..." << std::endl;
        sleep(1);
    }

    size_t sampleRate = input->getSampleRate();

    size_t channelCount = input->getChannelCount();
    if (m_plugin->getMaxChannelCount() < channelCount) {
	channelCount = 1;
    }

    float **buffers = new float*[channelCount];
    for (size_t ch = 0; ch < channelCount; ++ch) {
	buffers[ch] = new float[m_transform.getBlockSize() + 2];
    }

    size_t stepSize = m_transform.getStepSize();
    size_t blockSize = m_transform.getBlockSize();

    bool frequencyDomain = (m_plugin->getInputDomain() ==
                            Vamp::Plugin::FrequencyDomain);
    std::vector<FFTModel *> fftModels;

    if (frequencyDomain) {
        for (size_t ch = 0; ch < channelCount; ++ch) {
            FFTModel *model = new FFTModel
                                  (getConformingInput(),
                                   channelCount == 1 ? m_input.getChannel() : ch,
                                   m_transform.getWindowType(),
                                   blockSize,
                                   stepSize,
                                   blockSize,
                                   false,
                                   StorageAdviser::PrecisionCritical);
            if (!model->isOK()) {
                QMessageBox::critical
                    (0, tr("FFT cache failed"),
                     tr("Failed to create the FFT model for this transform.\n"
                        "There may be insufficient memory or disc space to continue."));
                delete model;
                setCompletion(100);
                return;
            }
            model->resume();
            fftModels.push_back(model);
        }
    }

    long startFrame = m_input.getModel()->getStartFrame();
    long   endFrame = m_input.getModel()->getEndFrame();

    RealTime contextStartRT = m_transform.getStartTime();
    RealTime contextDurationRT = m_transform.getDuration();

    long contextStart =
        RealTime::realTime2Frame(contextStartRT, sampleRate);

    long contextDuration =
        RealTime::realTime2Frame(contextDurationRT, sampleRate);

    if (contextStart == 0 || contextStart < startFrame) {
        contextStart = startFrame;
    }

    if (contextDuration == 0) {
        contextDuration = endFrame - contextStart;
    }
    if (contextStart + contextDuration > endFrame) {
        contextDuration = endFrame - contextStart;
    }

    long blockFrame = contextStart;

    long prevCompletion = 0;

    setCompletion(0);

    while (!m_abandoned) {

        if (frequencyDomain) {
            if (blockFrame - int(blockSize)/2 >
                contextStart + contextDuration) break;
        } else {
            if (blockFrame >= 
                contextStart + contextDuration) break;
        }

//	std::cerr << "FeatureExtractionModelTransformer::run: blockFrame "
//		  << blockFrame << ", endFrame " << endFrame << ", blockSize "
//                  << blockSize << std::endl;

	long completion =
	    (((blockFrame - contextStart) / stepSize) * 99) /
	    (contextDuration / stepSize);

	// channelCount is either m_input.getModel()->channelCount or 1

        for (size_t ch = 0; ch < channelCount; ++ch) {
            if (frequencyDomain) {
                int column = (blockFrame - startFrame) / stepSize;
                for (size_t i = 0; i <= blockSize/2; ++i) {
                    fftModels[ch]->getValuesAt
                        (column, i, buffers[ch][i*2], buffers[ch][i*2+1]);
                }
            } else {
                getFrames(ch, channelCount, 
                          blockFrame, blockSize, buffers[ch]);
            }                
        }

	Vamp::Plugin::FeatureSet features = m_plugin->process
	    (buffers, Vamp::RealTime::frame2RealTime(blockFrame, sampleRate));

	for (size_t fi = 0; fi < features[m_outputFeatureNo].size(); ++fi) {
	    Vamp::Plugin::Feature feature =
		features[m_outputFeatureNo][fi];
	    addFeature(blockFrame, feature);
	}

	if (blockFrame == contextStart || completion > prevCompletion) {
	    setCompletion(completion);
	    prevCompletion = completion;
	}

	blockFrame += stepSize;
    }

    if (m_abandoned) return;

    Vamp::Plugin::FeatureSet features = m_plugin->getRemainingFeatures();

    for (size_t fi = 0; fi < features[m_outputFeatureNo].size(); ++fi) {
	Vamp::Plugin::Feature feature =
	    features[m_outputFeatureNo][fi];
	addFeature(blockFrame, feature);
    }

    if (frequencyDomain) {
        for (size_t ch = 0; ch < channelCount; ++ch) {
            delete fftModels[ch];
        }
    }

    setCompletion(100);
}

void
FeatureExtractionModelTransformer::getFrames(int channel, int channelCount,
                                            long startFrame, long size,
                                            float *buffer)
{
    long offset = 0;

    if (startFrame < 0) {
        for (int i = 0; i < size && startFrame + i < 0; ++i) {
            buffer[i] = 0.0f;
        }
        offset = -startFrame;
        size -= offset;
        if (size <= 0) return;
        startFrame = 0;
    }

    DenseTimeValueModel *input = getConformingInput();
    if (!input) return;

    long got = input->getData
        ((channelCount == 1 ? m_input.getChannel() : channel),
         startFrame, size, buffer + offset);

    while (got < size) {
        buffer[offset + got] = 0.0;
        ++got;
    }

    if (m_input.getChannel() == -1 && channelCount == 1 &&
        input->getChannelCount() > 1) {
        // use mean instead of sum, as plugin input
        int cc = input->getChannelCount();
        for (long i = 0; i < size; ++i) {
            buffer[i] /= cc;
        }
    }
}

void
FeatureExtractionModelTransformer::addFeature(size_t blockFrame,
					     const Vamp::Plugin::Feature &feature)
{
    size_t inputRate = m_input.getModel()->getSampleRate();

//    std::cerr << "FeatureExtractionModelTransformer::addFeature("
//	      << blockFrame << ")" << std::endl;

    int binCount = 1;
    if (m_descriptor->hasFixedBinCount) {
	binCount = m_descriptor->binCount;
    }

    size_t frame = blockFrame;

    if (m_descriptor->sampleType ==
	Vamp::Plugin::OutputDescriptor::VariableSampleRate) {

	if (!feature.hasTimestamp) {
	    std::cerr
		<< "WARNING: FeatureExtractionModelTransformer::addFeature: "
		<< "Feature has variable sample rate but no timestamp!"
		<< std::endl;
	    return;
	} else {
	    frame = Vamp::RealTime::realTime2Frame(feature.timestamp, inputRate);
	}

    } else if (m_descriptor->sampleType ==
	       Vamp::Plugin::OutputDescriptor::FixedSampleRate) {

	if (feature.hasTimestamp) {
	    //!!! warning: sampleRate may be non-integral
	    frame = Vamp::RealTime::realTime2Frame(feature.timestamp,
                                                   lrintf(m_descriptor->sampleRate));
	} else {
	    frame = m_output->getEndFrame();
	}
    }
	
    if (binCount == 0) {

	SparseOneDimensionalModel *model =
            getConformingOutput<SparseOneDimensionalModel>();
	if (!model) return;

	model->addPoint(SparseOneDimensionalModel::Point(frame, feature.label.c_str()));
	
    } else if (binCount == 1) {

	float value = 0.0;
	if (feature.values.size() > 0) value = feature.values[0];

	SparseTimeValueModel *model =
            getConformingOutput<SparseTimeValueModel>();
	if (!model) return;

	model->addPoint(SparseTimeValueModel::Point(frame, value, feature.label.c_str()));
//        std::cerr << "SparseTimeValueModel::addPoint(" << frame << ", " << value << "), " << feature.label.c_str() << std::endl;

    } else if (m_descriptor->sampleType == 
	       Vamp::Plugin::OutputDescriptor::VariableSampleRate) {

        float pitch = 0.0;
        if (feature.values.size() > 0) pitch = feature.values[0];

        float duration = 1;
        if (feature.values.size() > 1) duration = feature.values[1];
        
        float velocity = 100;
        if (feature.values.size() > 2) velocity = feature.values[2];
        if (velocity < 0) velocity = 127;
        if (velocity > 127) velocity = 127;

        NoteModel *model = getConformingOutput<NoteModel>();
        if (!model) return;

        model->addPoint(NoteModel::Point(frame, pitch,
                                         lrintf(duration),
                                         velocity / 127.f,
                                         feature.label.c_str()));
	
    } else {
	
	DenseThreeDimensionalModel::Column values = feature.values;
	
	EditableDenseThreeDimensionalModel *model =
            getConformingOutput<EditableDenseThreeDimensionalModel>();
	if (!model) return;

	model->setColumn(frame / model->getResolution(), values);
    }
}

void
FeatureExtractionModelTransformer::setCompletion(int completion)
{
    int binCount = 1;
    if (m_descriptor->hasFixedBinCount) {
	binCount = m_descriptor->binCount;
    }

//    std::cerr << "FeatureExtractionModelTransformer::setCompletion("
//              << completion << ")" << std::endl;

    if (binCount == 0) {

	SparseOneDimensionalModel *model =
            getConformingOutput<SparseOneDimensionalModel>();
	if (!model) return;
	model->setCompletion(completion, true); //!!!m_context.updates);

    } else if (binCount == 1) {

	SparseTimeValueModel *model =
            getConformingOutput<SparseTimeValueModel>();
	if (!model) return;
	model->setCompletion(completion, true); //!!!m_context.updates);

    } else if (m_descriptor->sampleType ==
	       Vamp::Plugin::OutputDescriptor::VariableSampleRate) {

	NoteModel *model =
            getConformingOutput<NoteModel>();
	if (!model) return;
	model->setCompletion(completion, true); //!!!m_context.updates);

    } else {

	EditableDenseThreeDimensionalModel *model =
            getConformingOutput<EditableDenseThreeDimensionalModel>();
	if (!model) return;
	model->setCompletion(completion, true); //!!!m_context.updates);
    }
}

