/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef SV_EVENT_SERIES_H
#define SV_EVENT_SERIES_H

#include "Event.h"
#include "XmlExportable.h"

#include <set>

//#define DEBUG_EVENT_SERIES 1

/**
 * Container storing a series of events, with or without durations,
 * and supporting the ability to query which events are active at a
 * given frame or within a span of frames.
 *
 * To that end, in addition to the series of events, it stores a
 * series of "seams", which are frame positions at which the set of
 * simultaneous events changes (i.e. an event of non-zero duration
 * starts or ends) associated with a set of the events that are active
 * at or from that position. These are updated when an event is added
 * or removed.
 *
 * Performance is highly dependent on the extent of overlapping events
 * and the order in which events are added. Each event (with duration)
 * that is added requires updating all the seams within the extent of
 * that event, taking a number of ordered-set updates proportional to
 * the number of events already existing within its extent. Add events
 * in order of start frame if possible.
 */
class EventSeries : public XmlExportable
{
public:
    EventSeries() { }
    ~EventSeries() =default;

    EventSeries(const EventSeries &) =default;
    EventSeries &operator=(const EventSeries &) =default;
    EventSeries &operator=(EventSeries &&) =default;
    
    bool operator==(const EventSeries &other) const {
        return m_events == other.m_events;
    }
    
    void clear();
    void add(const Event &p);
    void remove(const Event &p);
    bool contains(const Event &p) const;
    bool isEmpty() const;
    int count() const;

    /**
     * Retrieve all events any part of which falls within the span in
     * frames defined by the given frame f and duration d.
     *
     * - An event without duration is within the span if its own frame
     * is greater than or equal to f and less than f + d.
     * 
     * - An event with duration is within the span if its start frame
     * is less than f + d and its start frame plus its duration is
     * greater than f.
     * 
     * Note: Passing a duration of zero is seldom useful here; you
     * probably want getEventsCovering instead. getEventsSpanning(f,
     * 0) is not equivalent to getEventsCovering(f). The latter
     * includes durationless events at f and events starting at f,
     * both of which are excluded from the former.
     */
    EventVector getEventsSpanning(sv_frame_t frame,
                                  sv_frame_t duration) const;

    /**
     * Retrieve all events that cover the given frame. An event without
     * duration covers a frame if its own frame is equal to it. An event
     * with duration covers a frame if its start frame is less than or
     * equal to it and its end frame (start + duration) is greater
     * than it.
     */
    EventVector getEventsCovering(sv_frame_t frame) const;

    /**
     * Emit to XML as a dataset element.
     */
    void toXml(QTextStream &out,
               QString indent,
               QString extraAttributes) const override;
    
private:
    /**
     * This vector contains all events in the series, in the normal
     * sort order. For backward compatibility we must support series
     * containing multiple instances of identical events, so
     * consecutive events in this vector will not always be distinct.
     * The vector is used in preference to a multiset or map<Event,
     * int> in order to allow indexing by "row number" as well as by
     * properties such as frame.
     * 
     * Because events are immutable, we do not have to worry about the
     * order changing once an event is inserted - we only add or
     * delete them.
     */
    typedef std::vector<Event> Events;
    Events m_events;
    
    /**
     * The FrameEventMap maps from frame number to a set of events. In
     * the seam map this is used to represent the events that are
     * active at that frame, either because they begin at that frame
     * or because they are continuing from an earlier frame. There is
     * an entry here for each frame at which an event starts or ends,
     * with the event appearing in all entries from its start time
     * onward and disappearing again at its end frame.
     *
     * Only events with duration appear in this map; point events
     * appear only in m_events. Note that unlike m_events, we only
     * store one instance of each event here, even if we hold many -
     * we refer back to m_events when we need to know how many
     * identical copies of a given event we have.
     */
    typedef std::map<sv_frame_t, std::vector<Event>> FrameEventMap;
    FrameEventMap m_seams;

    /** Create a seam at the given frame, copying from the prior seam
     *  if there is one. If a seam already exists at the given frame,
     *  leave it untouched.
     */
    void createSeam(sv_frame_t frame) {
        auto itr = m_seams.lower_bound(frame);
        if (itr == m_seams.end() || itr->first > frame) {
            if (itr != m_seams.begin()) {
                --itr;
            }
        }
        if (itr == m_seams.end()) {
            m_seams[frame] = {};
        } else if (itr->first < frame) {
            m_seams[frame] = itr->second;
        } else if (itr->first > frame) { // itr must be begin()
            m_seams[frame] = {};
        }
    }

    bool seamsEqual(const std::vector<Event> &s1,
                    const std::vector<Event> &s2) const {
        
        if (s1.size() != s2.size()) {
            return false;
        }

        // precondition: no event appears more than once in s1 or more
        // than once in s2

#ifdef DEBUG_EVENT_SERIES
        for (int i = 0; in_range_for(s1, i); ++i) {
            for (int j = i + 1; in_range_for(s1, j); ++j) {
                if (s1[i] == s1[j] || s2[i] == s2[j]) {
                    throw std::runtime_error
                        ("debug error: duplicate event in s1 or s2");
                }
            }
        }
#endif

        std::set<Event> ee;
        for (const auto &e: s1) {
            ee.insert(e);
        }
        for (const auto &e: s2) {
            if (ee.find(e) == ee.end()) {
                return false;
            }
        }
        return true;
    }

#ifdef DEBUG_EVENT_SERIES
    void dumpEvents() const {
        std::cerr << "EVENTS (" << m_events.size() << ") [" << std::endl;
        for (const auto &i: m_events) {
            std::cerr << "  " << i.toXmlString();
        }
        std::cerr << "]" << std::endl;
    }
    
    void dumpSeams() const {
        std::cerr << "SEAMS (" << m_seams.size() << ") [" << std::endl;
        for (const auto &s: m_seams) {
            std::cerr << "  " << s.first << " -> {" << std::endl;
            for (const auto &p: s.second) {
                std::cerr << p.toXmlString("    ");
            }
            std::cerr << "  }" << std::endl;
        }
        std::cerr << "]" << std::endl;
    }
#endif
};

#endif
