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

#include "WaveFileModel.h"

#include "fileio/AudioFileReader.h"
#include "fileio/AudioFileReaderFactory.h"

#include "system/System.h"

#include <QMessageBox>
#include <QFileInfo>

#include <iostream>
#include <unistd.h>
#include <math.h>
#include <sndfile.h>

#include <cassert>

using std::cerr;
using std::endl;

PowerOfSqrtTwoZoomConstraint
WaveFileModel::m_zoomConstraint;

WaveFileModel::WaveFileModel(QString path) :
    m_path(path),
    m_myReader(true),
    m_fillThread(0),
    m_updateTimer(0),
    m_lastFillExtent(0),
    m_exiting(false)
{
    m_reader = AudioFileReaderFactory::createReader(path);
    setObjectName(QFileInfo(path).fileName());
    if (isOK()) fillCache();
}

WaveFileModel::WaveFileModel(QString path, AudioFileReader *reader) :
    m_path(path),
    m_myReader(false),
    m_fillThread(0),
    m_updateTimer(0),
    m_lastFillExtent(0),
    m_exiting(false)
{
    m_reader = reader;
    setObjectName(QFileInfo(path).fileName());
    fillCache();
}

WaveFileModel::~WaveFileModel()
{
    m_exiting = true;
    if (m_fillThread) m_fillThread->wait();
    if (m_myReader) delete m_reader;
    m_reader = 0;
}

bool
WaveFileModel::isOK() const
{
    return m_reader && m_reader->isOK();
}

bool
WaveFileModel::isReady(int *completion) const
{
    bool ready = (isOK() && (m_fillThread == 0));
    double c = double(m_lastFillExtent) / double(getEndFrame() - getStartFrame());
    if (completion) *completion = int(c * 100.0 + 0.01);
//    std::cerr << "WaveFileModel::isReady(): ready = " << ready << ", completion = " << (completion ? *completion : -1) << std::endl;
    return ready;
}

Model *
WaveFileModel::clone() const
{
    WaveFileModel *model = new WaveFileModel(m_path);
    return model;
}

size_t
WaveFileModel::getFrameCount() const
{
    if (!m_reader) return 0;
    return m_reader->getFrameCount();
}

size_t
WaveFileModel::getChannelCount() const
{
    if (!m_reader) return 0;
    return m_reader->getChannelCount();
}

size_t
WaveFileModel::getSampleRate() const 
{
    if (!m_reader) return 0;
    return m_reader->getSampleRate();
}

size_t
WaveFileModel::getValues(int channel, size_t start, size_t end,
			 float *buffer) const
{
    // Always read these directly from the file. 
    // This is used for e.g. audio playback.
    // Could be much more efficient (although compiler optimisation will help)

    if (end < start) {
	std::cerr << "ERROR: WaveFileModel::getValues[float]: end < start ("
		  << end << " < " << start << ")" << std::endl;
	assert(end >= start);
    }

    if (!m_reader || !m_reader->isOK()) return 0;

//    std::cerr << "WaveFileModel::getValues(" << channel << ", "
//              << start << ", " << end << "): calling reader" << std::endl;

    SampleBlock frames;
    m_reader->getInterleavedFrames(start, end - start, frames);

    size_t i = 0;

    int ch0 = channel, ch1 = channel, channels = getChannelCount();
    if (channel == -1) {
	ch0 = 0;
	ch1 = channels - 1;
    }
    
    while (i < end - start) {

	buffer[i] = 0.0;

	for (int ch = ch0; ch <= ch1; ++ch) {

	    size_t index = i * channels + ch;
	    if (index >= frames.size()) break;
            
	    float sample = frames[index];
	    buffer[i] += sample;
	}

	++i;
    }

    return i;
}

size_t
WaveFileModel::getValues(int channel, size_t start, size_t end,
			 double *buffer) const
{
    if (end < start) {
	std::cerr << "ERROR: WaveFileModel::getValues[double]: end < start ("
		  << end << " < " << start << ")" << std::endl;
	assert(end >= start);
    }

    if (!m_reader || !m_reader->isOK()) return 0;

    SampleBlock frames;
    m_reader->getInterleavedFrames(start, end - start, frames);

    size_t i = 0;

    int ch0 = channel, ch1 = channel, channels = getChannelCount();
    if (channel == -1) {
	ch0 = 0;
	ch1 = channels - 1;
    }

    while (i < end - start) {

	buffer[i] = 0.0;

	for (int ch = ch0; ch <= ch1; ++ch) {

	    size_t index = i * channels + ch;
	    if (index >= frames.size()) break;
            
	    float sample = frames[index];
	    buffer[i] += sample;
	}

	++i;
    }

    return i;
}

WaveFileModel::RangeBlock
WaveFileModel::getRanges(size_t channel, size_t start, size_t end,
			 size_t &blockSize) const
{
    RangeBlock ranges;
    if (!isOK()) return ranges;

    if (end <= start) {
	std::cerr << "WARNING: Internal error: end <= start in WaveFileModel::getRanges (end = " << end << ", start = " << start << ", blocksize = " << blockSize << ")" << std::endl;
	return ranges;
    }

    int cacheType = 0;
    int power = m_zoomConstraint.getMinCachePower();
    blockSize = m_zoomConstraint.getNearestBlockSize
        (blockSize, cacheType, power, ZoomConstraint::RoundUp);

    size_t channels = getChannelCount();

    if (cacheType != 0 && cacheType != 1) {

	// We need to read directly from the file.  We haven't got
	// this cached.  Hope the requested area is small.  This is
	// not optimal -- we'll end up reading the same frames twice
	// for stereo files, in two separate calls to this method.
	// We could fairly trivially handle this for most cases that
	// matter by putting a single cache in getInterleavedFrames
	// for short queries.

	SampleBlock frames;
	m_reader->getInterleavedFrames(start, end - start, frames);
	float max = 0.0, min = 0.0, total = 0.0;
	size_t i = 0, count = 0;

	while (i < end - start) {

	    size_t index = i * channels + channel;
	    if (index >= frames.size()) break;
            
	    float sample = frames[index];
            if (sample > max || count == 0) max = sample;
	    if (sample < min || count == 0) min = sample;
            total += fabsf(sample);
	    
	    ++i;
            ++count;
            
            if (count == blockSize) {
                ranges.push_back(Range(min, max, total / count));
                min = max = total = 0.0f;
                count = 0;
	    }
	}

	if (count > 0) {
            ranges.push_back(Range(min, max, total / count));
	}

	return ranges;

    } else {

	QMutexLocker locker(&m_mutex);
    
	const RangeBlock &cache = m_cache[cacheType];

	size_t cacheBlock, div;
        
	if (cacheType == 0) {
	    cacheBlock = (1 << m_zoomConstraint.getMinCachePower());
            div = (1 << power) / cacheBlock;
	} else {
	    cacheBlock = ((unsigned int)((1 << m_zoomConstraint.getMinCachePower()) * sqrt(2) + 0.01));
            div = ((unsigned int)((1 << power) * sqrt(2) + 0.01)) / cacheBlock;
	}

	size_t startIndex = start / cacheBlock;
	size_t endIndex = end / cacheBlock;

	float max = 0.0, min = 0.0, total = 0.0;
	size_t i = 0, count = 0;

	//cerr << "blockSize is " << blockSize << ", cacheBlock " << cacheBlock << ", start " << start << ", end " << end << ", power is " << power << ", div is " << div << ", startIndex " << startIndex << ", endIndex " << endIndex << endl;

	for (i = 0; i < endIndex - startIndex; ) {
        
	    size_t index = (i + startIndex) * channels + channel;
	    if (index >= cache.size()) break;
            
            const Range &range = cache[index];
            if (range.max > max || count == 0) max = range.max;
            if (range.min < min || count == 0) min = range.min;
            total += range.absmean;
            
	    ++i;
            ++count;
            
	    if (count == div) {
		ranges.push_back(Range(min, max, total / count));
                min = max = total = 0.0f;
                count = 0;
	    }
	}
		
	if (count > 0) {
            ranges.push_back(Range(min, max, total / count));
	}
    }

    //cerr << "returning " << ranges.size() << " ranges" << endl;
    return ranges;
}

WaveFileModel::Range
WaveFileModel::getRange(size_t channel, size_t start, size_t end) const
{
    Range range;
    if (!isOK()) return range;

    if (end <= start) {
	std::cerr << "WARNING: Internal error: end <= start in WaveFileModel::getRange (end = " << end << ", start = " << start << ")" << std::endl;
	return range;
    }

    size_t blockSize;
    for (blockSize = 1; blockSize <= end - start; blockSize *= 2);
    blockSize /= 2;

    bool first = false;

    size_t blockStart = (start / blockSize) * blockSize;
    size_t blockEnd = (end / blockSize) * blockSize;

    if (blockStart < start) blockStart += blockSize;
        
    if (blockEnd > blockStart) {
        RangeBlock ranges = getRanges(channel, blockStart, blockEnd, blockSize);
        for (size_t i = 0; i < ranges.size(); ++i) {
            if (first || ranges[i].min < range.min) range.min = ranges[i].min;
            if (first || ranges[i].max > range.max) range.max = ranges[i].max;
            if (first || ranges[i].absmean < range.absmean) range.absmean = ranges[i].absmean;
            first = false;
        }
    }

    if (blockStart > start) {
        Range startRange = getRange(channel, start, blockStart);
        range.min = std::min(range.min, startRange.min);
        range.max = std::max(range.max, startRange.max);
        range.absmean = std::min(range.absmean, startRange.absmean);
    }

    if (blockEnd < end) {
        Range endRange = getRange(channel, blockEnd, end);
        range.min = std::min(range.min, endRange.min);
        range.max = std::max(range.max, endRange.max);
        range.absmean = std::min(range.absmean, endRange.absmean);
    }

    return range;
}

void
WaveFileModel::fillCache()
{
    m_mutex.lock();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, SIGNAL(timeout()), this, SLOT(fillTimerTimedOut()));
    m_updateTimer->start(100);

    m_fillThread = new RangeCacheFillThread(*this);
    connect(m_fillThread, SIGNAL(finished()), this, SLOT(cacheFilled()));

    m_mutex.unlock();
    m_fillThread->start();

//    std::cerr << "WaveFileModel::fillCache: started fill thread" << std::endl;
}   

void
WaveFileModel::fillTimerTimedOut()
{
    if (m_fillThread) {
	size_t fillExtent = m_fillThread->getFillExtent();
//        cerr << "WaveFileModel::fillTimerTimedOut: extent = " << fillExtent << endl;
	if (fillExtent > m_lastFillExtent) {
	    emit modelChanged(m_lastFillExtent, fillExtent);
	    m_lastFillExtent = fillExtent;
	}
    } else {
//        cerr << "WaveFileModel::fillTimerTimedOut: no thread" << std::endl;
	emit modelChanged();
    }
}

void
WaveFileModel::cacheFilled()
{
    m_mutex.lock();
    delete m_fillThread;
    m_fillThread = 0;
    delete m_updateTimer;
    m_updateTimer = 0;
    m_mutex.unlock();
    emit modelChanged();
//    cerr << "WaveFileModel::cacheFilled" << endl;
}

void
WaveFileModel::RangeCacheFillThread::run()
{
    size_t cacheBlockSize[2];
    cacheBlockSize[0] = (1 << m_model.m_zoomConstraint.getMinCachePower());
    cacheBlockSize[1] = ((unsigned int)((1 << m_model.m_zoomConstraint.getMinCachePower()) *
                                        sqrt(2) + 0.01));
    
    size_t frame = 0;
    size_t readBlockSize = 16384;
    SampleBlock block;

    if (!m_model.isOK()) return;
    
    size_t channels = m_model.getChannelCount();
    bool updating = m_model.m_reader->isUpdating();

    if (updating) {
        while (channels == 0 && !m_model.m_exiting) {
//            std::cerr << "WaveFileModel::fill: Waiting for channels..." << std::endl;
            sleep(1);
            channels = m_model.getChannelCount();
        }
    }

    Range *range = new Range[2 * channels];
    size_t count[2];
    count[0] = count[1] = 0;

    bool first = true;

    while (first || updating) {

        updating = m_model.m_reader->isUpdating();
        m_frameCount = m_model.getFrameCount();

//        std::cerr << "WaveFileModel::fill: frame = " << frame << ", count = " << m_frameCount << std::endl;

        while (frame < m_frameCount) {

            if (updating && (frame + readBlockSize > m_frameCount)) break;

            m_model.m_reader->getInterleavedFrames(frame, readBlockSize, block);

            for (size_t i = 0; i < readBlockSize; ++i) {
		
                for (size_t ch = 0; ch < size_t(channels); ++ch) {

                    size_t index = channels * i + ch;
                    if (index >= block.size()) continue;
                    float sample = block[index];
                    
                    for (size_t ct = 0; ct < 2; ++ct) {
                        
                        size_t rangeIndex = ch * 2 + ct;
                        
                        if (sample > range[rangeIndex].max || count[ct] == 0) {
                            range[rangeIndex].max = sample;
                        }
                        if (sample < range[rangeIndex].min || count[ct] == 0) {
                            range[rangeIndex].min = sample;
                        }
                        range[rangeIndex].absmean += fabsf(sample);
                    }
                }
                
                QMutexLocker locker(&m_model.m_mutex);
                for (size_t ct = 0; ct < 2; ++ct) {
                    if (++count[ct] == cacheBlockSize[ct]) {
                        for (size_t ch = 0; ch < size_t(channels); ++ch) {
                            size_t rangeIndex = ch * 2 + ct;
                            range[rangeIndex].absmean /= count[ct];
                            m_model.m_cache[ct].push_back(range[rangeIndex]);
                            range[rangeIndex] = Range();
                        }
                        count[ct] = 0;
                    }
                }
                
                ++frame;
            }
            
            if (m_model.m_exiting) break;
            
            m_fillExtent = frame;
        }

        first = false;
        if (m_model.m_exiting) break;
        if (updating) {
            sleep(1);
        }
    }

    if (!m_model.m_exiting) {

        QMutexLocker locker(&m_model.m_mutex);
        for (size_t ct = 0; ct < 2; ++ct) {
            if (count[ct] > 0) {
                for (size_t ch = 0; ch < size_t(channels); ++ch) {
                    size_t rangeIndex = ch * 2 + ct;
                    range[rangeIndex].absmean /= count[ct];
                    m_model.m_cache[ct].push_back(range[rangeIndex]);
                    range[rangeIndex] = Range();
                }
                count[ct] = 0;
            }
            
            const Range &rr = *m_model.m_cache[ct].begin();
            MUNLOCK(&rr, m_model.m_cache[ct].capacity() * sizeof(Range));
        }
    }
    
    delete[] range;

    m_fillExtent = m_frameCount;
        
//    for (size_t ct = 0; ct < 2; ++ct) {
//        cerr << "Cache type " << ct << " now contains " << m_model.m_cache[ct].size() << " ranges" << endl;
//    }
}

void
WaveFileModel::toXml(QTextStream &out,
                     QString indent,
                     QString extraAttributes) const
{
    Model::toXml(out, indent,
                 QString("type=\"wavefile\" file=\"%1\" %2")
                 .arg(m_path).arg(extraAttributes));
}

QString
WaveFileModel::toXmlString(QString indent,
			   QString extraAttributes) const
{
    return Model::toXmlString(indent,
			      QString("type=\"wavefile\" file=\"%1\" %2")
			      .arg(m_path).arg(extraAttributes));
}
    

#ifdef INCLUDE_MOCFILES
#ifdef INCLUDE_MOCFILES
#include "WaveFileModel.moc.cpp"
#endif
#endif
