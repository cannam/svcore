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

#ifndef _FFT_DATA_SERVER_H_
#define _FFT_DATA_SERVER_H_

#include "base/Window.h"
#include "base/Thread.h"

#include <fftw3.h>

#include <QMutex>
#include <QWaitCondition>
#include <QString>

#include <vector>
#include <deque>

class DenseTimeValueModel;
class FFTCache;

class FFTDataServer
{
public:
    static FFTDataServer *getInstance(const DenseTimeValueModel *model,
                                      int channel,
                                      WindowType windowType,
                                      size_t windowSize,
                                      size_t windowIncrement,
                                      size_t fftSize,
                                      bool polar,
                                      size_t fillFromColumn = 0);

    static FFTDataServer *getFuzzyInstance(const DenseTimeValueModel *model,
                                           int channel,
                                           WindowType windowType,
                                           size_t windowSize,
                                           size_t windowIncrement,
                                           size_t fftSize,
                                           bool polar,
                                           size_t fillFromColumn = 0);

    static void claimInstance(FFTDataServer *);
    static void releaseInstance(FFTDataServer *);

    const DenseTimeValueModel *getModel() const { return m_model; }
    int        getChannel() const { return m_channel; }
    WindowType getWindowType() const { return m_windower.getType(); }
    size_t     getWindowSize() const { return m_windowSize; }
    size_t     getWindowIncrement() const { return m_windowIncrement; }
    size_t     getFFTSize() const { return m_fftSize; }
    bool       getPolar() const { return m_polar; }

    size_t     getWidth() const  { return m_width;  }
    size_t     getHeight() const { return m_height; }

    float      getMagnitudeAt(size_t x, size_t y);
    float      getNormalizedMagnitudeAt(size_t x, size_t y);
    float      getMaximumMagnitudeAt(size_t x);
    float      getPhaseAt(size_t x, size_t y);
    void       getValuesAt(size_t x, size_t y, float &real, float &imaginary);
    bool       isColumnReady(size_t x);

    void       suspend();
    void       suspendWrites();
    void       resume(); // also happens automatically if new data needed

    // Convenience functions:

    bool isLocalPeak(size_t x, size_t y) {
        float mag = getMagnitudeAt(x, y);
        if (y > 0 && mag < getMagnitudeAt(x, y - 1)) return false;
        if (y < getHeight()-1 && mag < getMagnitudeAt(x, y + 1)) return false;
        return true;
    }
    bool isOverThreshold(size_t x, size_t y, float threshold) {
        return getMagnitudeAt(x, y) > threshold;
    }

    size_t getFillCompletion() const;
    size_t getFillExtent() const;

private:
    FFTDataServer(QString fileBaseName,
                  const DenseTimeValueModel *model,
                  int channel,
                  WindowType windowType,
                  size_t windowSize,
                  size_t windowIncrement,
                  size_t fftSize,
                  bool polar,
                  size_t fillFromColumn = 0);

    virtual ~FFTDataServer();

    FFTDataServer(const FFTDataServer &); // not implemented
    FFTDataServer &operator=(const FFTDataServer &); // not implemented

    typedef float fftsample;

    QString m_fileBaseName;
    const DenseTimeValueModel *m_model;
    int m_channel;

    Window<fftsample> m_windower;

    size_t m_windowSize;
    size_t m_windowIncrement;
    size_t m_fftSize;
    bool m_polar;

    size_t m_width;
    size_t m_height;
    size_t m_cacheWidth;
    size_t m_cacheWidthPower;
    size_t m_cacheWidthMask;
    bool m_memoryCache;
    bool m_compactCache;

    typedef std::vector<FFTCache *> CacheVector;
    CacheVector m_caches;
    
    typedef std::deque<int> IntQueue;
    IntQueue m_dormantCaches;

    int m_lastUsedCache;
    FFTCache *getCache(size_t x, size_t &col) {
        col   = x % m_cacheWidth;
        int c = x / m_cacheWidth;
        // The only use of m_lastUsedCache without a lock is to
        // establish whether a cache has been created at all (they're
        // created on demand, but not destroyed until the server is).
        if (c == m_lastUsedCache) return m_caches[c];
        else return getCacheAux(c);
    }
    bool haveCache(size_t x) {
        int c = x / m_cacheWidth;
        if (c == m_lastUsedCache) return true;
        else return (m_caches[c] != 0);
    }
        
    FFTCache *getCacheAux(size_t c);
    QMutex m_writeMutex;
    QWaitCondition m_condition;

    fftsample *m_fftInput;
    fftwf_complex *m_fftOutput;
    float *m_workbuffer;
    fftwf_plan m_fftPlan;

    class FillThread : public Thread
    {
    public:
        FillThread(FFTDataServer &server, size_t fillFromColumn) :
            m_server(server), m_extent(0), m_completion(0),
            m_fillFrom(fillFromColumn) { }

        size_t getExtent() const { return m_extent; }
        size_t getCompletion() const { return m_completion ? m_completion : 1; }
        virtual void run();

    protected:
        FFTDataServer &m_server;
        size_t m_extent;
        size_t m_completion;
        size_t m_fillFrom;
    };

    bool m_exiting;
    bool m_suspended;
    FillThread *m_fillThread;

    void deleteProcessingData();
    void fillColumn(size_t x);

    QString generateFileBasename() const;
    static QString generateFileBasename(const DenseTimeValueModel *model,
                                        int channel,
                                        WindowType windowType,
                                        size_t windowSize,
                                        size_t windowIncrement,
                                        size_t fftSize,
                                        bool polar);

    typedef std::pair<FFTDataServer *, int> ServerCountPair;
    typedef std::map<QString, ServerCountPair> ServerMap;

    static ServerMap m_servers;
    static QMutex m_serverMapMutex;
    static FFTDataServer *findServer(QString); // call with serverMapMutex held
    static void purgeLimbo(int maxSize = 3); // call with serverMapMutex held
};

#endif
