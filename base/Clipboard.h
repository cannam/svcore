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

#ifndef SV_CLIPBOARD_H
#define SV_CLIPBOARD_H

#include <vector>

#include "Point.h"

class Clipboard
{
public:
    Clipboard();
    ~Clipboard();

    typedef std::vector<Point> PointList;

    void clear();
    bool empty() const;
    const PointList &getPoints() const;
    void setPoints(const PointList &points);
    void addPoint(const Point &point);

    bool haveReferenceFrames() const;
    bool referenceFramesDiffer() const;

protected:
    PointList m_points;
};

#endif
