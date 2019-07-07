/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "Model.h"
#include "AlignmentModel.h"

#include <QTextStream>

#include <iostream>

//#define DEBUG_COMPLETION 1

Model::~Model()
{
    SVDEBUG << "Model::~Model: " << this << " with id " << getId() << endl;
    //!!! see notes in header - sort this out
    /*
    if (!m_alignmentModel.isNone()) {
        ModelById::release(m_alignmentModel);
    }
    */
}

void
Model::setSourceModel(ModelId modelId)
{
    m_sourceModel = modelId;

    auto model = ModelById::get(m_sourceModel);
    if (model) {
        connect(model.get(), SIGNAL(alignmentCompletionChanged(ModelId)),
                this, SIGNAL(alignmentCompletionChanged(ModelId)));
    }
}

void
Model::setAlignment(ModelId alignmentModel)
{
    SVDEBUG << "Model(" << this << "): accepting alignment model "
            << alignmentModel << endl;
    
    if (!m_alignmentModel.isNone()) {
        ModelById::release(m_alignmentModel);
    }
    
    m_alignmentModel = alignmentModel;

    auto model = ModelById::get(m_alignmentModel);
    if (model) {
        connect(model.get(), SIGNAL(completionChanged(ModelId)),
                this, SIGNAL(alignmentCompletionChanged(ModelId)));
    }
}

const ModelId
Model::getAlignment() const
{
    return m_alignmentModel;
}

const ModelId
Model::getAlignmentReference() const
{
    auto model = ModelById::getAs<AlignmentModel>(m_alignmentModel);
    if (model) return model->getReferenceModel();
    else return {};
}

sv_frame_t
Model::alignToReference(sv_frame_t frame) const
{
    auto alignmentModel = ModelById::getAs<AlignmentModel>(m_alignmentModel);
    
    if (!alignmentModel) {
        auto sourceModel = ModelById::get(m_sourceModel);
        if (sourceModel) {
            return sourceModel->alignToReference(frame);
        }
        return frame;
    }
    
    sv_frame_t refFrame = alignmentModel->toReference(frame);
    auto refModel = ModelById::get(alignmentModel->getReferenceModel());
    if (refModel && refFrame > refModel->getEndFrame()) {
        refFrame = refModel->getEndFrame();
    }
    return refFrame;
}

sv_frame_t
Model::alignFromReference(sv_frame_t refFrame) const
{
    auto alignmentModel = ModelById::getAs<AlignmentModel>(m_alignmentModel);
    
    if (!alignmentModel) {
        auto sourceModel = ModelById::get(m_sourceModel);
        if (sourceModel) {
            return sourceModel->alignFromReference(refFrame);
        }
        return refFrame;
    }
    
    sv_frame_t frame = alignmentModel->fromReference(refFrame);
    if (frame > getEndFrame()) frame = getEndFrame();
    return frame;
}

int
Model::getAlignmentCompletion() const
{
#ifdef DEBUG_COMPLETION
    SVCERR << "Model(" << this
           << ")::getAlignmentCompletion: m_alignmentModel = "
           << m_alignmentModel << endl;
#endif

    auto alignmentModel = ModelById::getAs<AlignmentModel>(m_alignmentModel);
    
    if (!alignmentModel) {
        auto sourceModel = ModelById::get(m_sourceModel);
        if (sourceModel) {
            return sourceModel->getAlignmentCompletion();
        }
        return 100;
    }
    
    int completion = 0;
    (void)alignmentModel->isReady(&completion);
#ifdef DEBUG_COMPLETION
    SVCERR << "Model(" << this
           << ")::getAlignmentCompletion: completion = " << completion
           << endl;
#endif
    return completion;
}

QString
Model::getTitle() const
{
    auto source = ModelById::get(m_sourceModel);
    if (source) return source->getTitle();
    else return "";
}

QString
Model::getMaker() const
{
    auto source = ModelById::get(m_sourceModel);
    if (source) return source->getMaker();
    else return "";
}

QString
Model::getLocation() const
{
    auto source = ModelById::get(m_sourceModel);
    if (source) return source->getLocation();
    else return "";
}

void
Model::toXml(QTextStream &stream, QString indent,
             QString extraAttributes) const
{
    stream << indent;
    stream << QString("<model id=\"%1\" name=\"%2\" sampleRate=\"%3\" start=\"%4\" end=\"%5\" %6/>\n")
        .arg(getExportId())
        .arg(encodeEntities(objectName()))
        .arg(getSampleRate())
        .arg(getStartFrame())
        .arg(getEndFrame())
        .arg(extraAttributes);
}


