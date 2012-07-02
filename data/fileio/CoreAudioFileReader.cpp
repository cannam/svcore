/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.

    Based on QTAudioFile.cpp from SoundBite, copyright 2006
    Chris Sutton and Mark Levy.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
 */

#ifdef HAVE_COREAUDIO

#include "CoreAudioFileReader.h"
#include "base/Profiler.h"
#include "base/ProgressReporter.h"
#include "system/System.h"

#include <QFileInfo>

#if !defined(__COREAUDIO_USE_FLAT_INCLUDES__)
#include <AudioToolbox/AudioToolbox.h>
#include <AudioToolbox/ExtendedAudioFile.h>
#else
#include "AudioToolbox.h"
#include "ExtendedAudioFile.h"
#endif

class CoreAudioFileReader::D
{
public:
    D() : blockSize(1024) { }

    ExtAudioFileRef file;
    AudioBufferList buffer;
    OSStatus err;
    AudioStreamBasicDescription asbd;
    int blockSize;
};

static QString
codestr(OSStatus err)
{
    char text[5];
    UInt32 uerr = err;
    text[0] = (uerr >> 24) & 0xff;
    text[1] = (uerr >> 16) & 0xff;
    text[2] = (uerr >> 8) & 0xff;
    text[3] = (uerr) & 0xff;
    text[4] = '\0';
    return QString("%1 (%2)").arg(err).arg(QString::fromAscii(text));
}

CoreAudioFileReader::CoreAudioFileReader(FileSource source,
                                         DecodeMode decodeMode,
                                         CacheMode mode,
                                         size_t targetRate,
                                         ProgressReporter *reporter) :
    CodedAudioFileReader(mode, targetRate),
    m_source(source),
    m_path(source.getLocalFilename()),
    m_d(new D),
    m_reporter(reporter),
    m_cancelled(false),
    m_completion(0),
    m_decodeThread(0)
{
    m_channelCount = 0;
    m_fileRate = 0;

    Profiler profiler("CoreAudioFileReader::CoreAudioFileReader", true);

    std::cerr << "CoreAudioFileReader: path is \"" << m_path.toStdString() << "\"" << std::endl;

    QByteArray ba = m_path.toLocal8Bit();

    CFURLRef url = CFURLCreateFromFileSystemRepresentation
        (kCFAllocatorDefault,
         (const UInt8 *)ba.data(),
         (CFIndex)ba.length(),
         false);

    //!!! how do we find out if the file open fails because of DRM protection?

#if (MACOSX_DEPLOYMENT_TARGET <= 1040 && MAC_OS_X_VERSION_MIN_REQUIRED <= 1040)
    FSRef fsref;
    if (!CFURLGetFSRef(url, &fsref)) { // returns Boolean, not error code
        m_error = "CoreAudioReadStream: Error looking up FS ref (file not found?)";
        return;
    }
    m_d->err = ExtAudioFileOpen(&fsref, &m_d->file);
#else
    m_d->err = ExtAudioFileOpenURL(url, &m_d->file);
#endif

    CFRelease(url);

    if (m_d->err) { 
        m_error = "CoreAudioReadStream: Error opening file: code " + codestr(m_d->err);
        return;
    }
    if (!m_d->file) { 
        m_error = "CoreAudioReadStream: Failed to open file, but no error reported!";
        return;
    }
    
    UInt32 propsize = sizeof(AudioStreamBasicDescription);
    m_d->err = ExtAudioFileGetProperty
	(m_d->file, kExtAudioFileProperty_FileDataFormat, &propsize, &m_d->asbd);
    
    if (m_d->err) {
        m_error = "CoreAudioReadStream: Error in getting basic description: code " + codestr(m_d->err);
        return;
    }
	
    m_channelCount = m_d->asbd.mChannelsPerFrame;
    m_sampleRate = m_d->asbd.mSampleRate;

    std::cerr << "CoreAudioReadStream: " << m_channelCount << " channels, " << m_sampleRate << " Hz" << std::endl;

    m_d->asbd.mSampleRate = getSampleRate();

    m_d->asbd.mFormatID = kAudioFormatLinearPCM;
    m_d->asbd.mFormatFlags =
        kAudioFormatFlagIsFloat |
        kAudioFormatFlagIsPacked |
        kAudioFormatFlagsNativeEndian;
    m_d->asbd.mBitsPerChannel = sizeof(float) * 8;
    m_d->asbd.mBytesPerFrame = sizeof(float) * m_channelCount;
    m_d->asbd.mBytesPerPacket = sizeof(float) * m_channelCount;
    m_d->asbd.mFramesPerPacket = 1;
    m_d->asbd.mReserved = 0;
	
    m_d->err = ExtAudioFileSetProperty
	(m_d->file, kExtAudioFileProperty_ClientDataFormat, propsize, &m_d->asbd);
    
    if (m_d->err) {
        m_error = "CoreAudioReadStream: Error in setting client format: code " + codestr(m_d->err);
        return;
    }

    m_d->buffer.mNumberBuffers = 1;
    m_d->buffer.mBuffers[0].mNumberChannels = m_channelCount;
    m_d->buffer.mBuffers[0].mDataByteSize = sizeof(float) * m_channelCount * m_d->blockSize;
    m_d->buffer.mBuffers[0].mData = new float[m_channelCount * m_d->blockSize];

    initialiseDecodeCache();

    if (m_reporter) {
        connect(m_reporter, SIGNAL(cancelled()), this, SLOT(cancelled()));
        m_reporter->setMessage
            (tr("Decoding %1...").arg(QFileInfo(m_path).fileName()));
    }

    while (1) {

        UInt32 framesRead = m_d->blockSize;
        m_d->err = ExtAudioFileRead(m_d->file, &framesRead, &m_d->buffer);

        if (m_d->err) {
            m_error = QString("Error in CoreAudio decoding: code %1")
                .arg(m_d->err);
            break;
        }

        //!!! progress?

        //    std::cerr << "Read " << framesRead << " frames (block size " << m_d->blockSize << ")" << std::endl;

        // buffers are interleaved unless specified otherwise
        addSamplesToDecodeCache((float *)m_d->buffer.mBuffers[0].mData, framesRead);

        if (framesRead < m_d->blockSize) break;
    }

    finishDecodeCache();
    endSerialised();

    m_completion = 100;

    ExtAudioFileDispose(m_d->file);
}


CoreAudioFileReader::~CoreAudioFileReader()
{
    std::cerr << "CoreAudioFileReader::~CoreAudioFileReader" << std::endl;
    delete[] m_d->buffer.mBuffers[0].mData;
    delete m_d;
}

void
CoreAudioFileReader::cancelled()
{
  m_cancelled = true;
}

/*
void
CoreAudioFileReader::DecodeThread::run()
{
  if (m_reader->m_cacheMode == CacheInTemporaryFile) {
      m_reader->m_completion = 1;
      m_reader->startSerialised("QuickTimeFileReader::Decode");
  }

  while (1) {

      UInt32 framesRead = m_reader->m_d->blockSize;
      UInt32 extractionFlags = 0;
      m_reader->m_d->err = MovieAudioExtractionFillBuffer
          (m_reader->m_d->extractionSessionRef, &framesRead,
              &m_reader->m_d->buffer, &extractionFlags);
      if (m_reader->m_d->err) {
          m_reader->m_error = QString("Error in QuickTime decoding: code %1")
                                    .arg(m_reader->m_d->err);
          break;
      }

      // QuickTime buffers are interleaved unless specified otherwise
      m_reader->addSamplesToDecodeCache(m_reader->m_d->data, framesRead);

      if (framesRead < m_reader->m_d->blockSize) break;
  }

  m_reader->finishDecodeCache();

  m_reader->m_d->err = MovieAudioExtractionEnd(m_reader->m_d->extractionSessionRef);
  if (m_reader->m_d->err) {
      m_reader->m_error = QString("Error ending QuickTime extraction session: code %1").arg(m_reader->m_d->err);
  }

  m_reader->m_completion = 100;
  m_reader->endSerialised();
} 
*/


void
CoreAudioFileReader::getSupportedExtensions(std::set<QString> &extensions)
{
    extensions.insert("aiff");
    extensions.insert("aif");
    extensions.insert("au");
    extensions.insert("avi");
    extensions.insert("m4a");
    extensions.insert("m4b");
    extensions.insert("m4p");
    extensions.insert("m4v");
    extensions.insert("mov");
    extensions.insert("mp3");
    extensions.insert("mp4");
    extensions.insert("wav");
}

bool
CoreAudioFileReader::supportsExtension(QString extension)
{
    std::set<QString> extensions;
    getSupportedExtensions(extensions);
    return (extensions.find(extension.toLower()) != extensions.end());
}

bool
CoreAudioFileReader::supportsContentType(QString type)
{
    return (type == "audio/x-aiff" ||
            type == "audio/x-wav" ||
            type == "audio/mpeg" ||
            type == "audio/basic" ||
            type == "audio/x-aac" ||
            type == "video/mp4" ||
            type == "video/quicktime");
}

bool
CoreAudioFileReader::supports(FileSource &source)
{
    return (supportsExtension(source.getExtension()) ||
            supportsContentType(source.getContentType()));
}

#endif

