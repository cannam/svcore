/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2006-2007 Chris Cannam and QMUL.

    Based in part on QTAudioFile.h from SoundBite, copyright 2006
    Chris Sutton and Mark Levy.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _QUICKTIME_FILE_READER_H_
#define _QUICKTIME_FILE_READER_H_

#ifdef HAVE_QUICKTIME

#include "CodedAudioFileReader.h"

#include "base/Thread.h"

#include <set>

class QProgressDialog;

class QuickTimeFileReader : public CodedAudioFileReader
{
public:
    enum DecodeMode {
        DecodeAtOnce, // decode the file on construction, with progress dialog
        DecodeThreaded // decode in a background thread after construction
    };

    QuickTimeFileReader(std::string path,
                        DecodeMode decodeMode,
                        CacheMode cacheMode);
    virtual ~QuickTimeFileReader();

    virtual std::string getTitle() const { return m_title; }
    
    static void getSupportedExtensions(std::set<std::string> &extensions);

    virtual int getDecodeCompletion() const { return m_completion; }

    virtual bool isUpdating() const {
        return m_decodeThread && m_decodeThread->isRunning();
    }

protected:
    std::string m_path;
    std::string m_title;

    class D;
    D *m_d;

    QProgressDialog *m_progress;
    bool m_cancelled;
    int m_completion;

    class DecodeThread : public Thread
    {
    public:
        DecodeThread(QuickTimeFileReader *reader) : m_reader(reader) { }
        virtual void run();

    protected:
        QuickTimeFileReader *m_reader; 
    };

    DecodeThread *m_decodeThread;
};

#endif

#endif
