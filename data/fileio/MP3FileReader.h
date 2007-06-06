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

#ifndef _MP3_FILE_READER_H_
#define _MP3_FILE_READER_H_

#ifdef HAVE_MAD

#include "CodedAudioFileReader.h"

#include "base/Thread.h"
#include <mad.h>

#include <set>

class QProgressDialog;

class MP3FileReader : public CodedAudioFileReader
{
public:
    enum DecodeMode {
        DecodeAtOnce, // decode the file on construction, with progress dialog
        DecodeThreaded // decode in a background thread after construction
    };

    MP3FileReader(QString path, DecodeMode decodeMode, CacheMode cacheMode);
    virtual ~MP3FileReader();

    virtual QString getError() const { return m_error; }

    static void getSupportedExtensions(std::set<QString> &extensions);
    
    virtual int getDecodeCompletion() const { return m_completion; }

    virtual bool isUpdating() const {
        return m_decodeThread && m_decodeThread->isRunning();
    }

protected:
    QString m_path;
    QString m_error;
    size_t m_fileSize;
    double m_bitrateNum;
    size_t m_bitrateDenom;
    int m_completion;
    bool m_done;

    unsigned char *m_filebuffer;

    QProgressDialog *m_progress;
    bool m_cancelled;

    struct DecoderData
    {
	unsigned char const *start;
	unsigned long length;
	MP3FileReader *reader;
    };

    bool decode(void *mm, size_t sz);
    enum mad_flow accept(struct mad_header const *, struct mad_pcm *);

    static enum mad_flow input(void *, struct mad_stream *);
    static enum mad_flow output(void *, struct mad_header const *, struct mad_pcm *);
    static enum mad_flow error(void *, struct mad_stream *, struct mad_frame *);

    class DecodeThread : public Thread
    {
    public:
        DecodeThread(MP3FileReader *reader) : m_reader(reader) { }
        virtual void run();

    protected:
        MP3FileReader *m_reader;
    };

    DecodeThread *m_decodeThread;
};

#endif

#endif
