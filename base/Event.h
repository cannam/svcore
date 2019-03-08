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

#ifndef SV_EVENT_H
#define SV_EVENT_H

#include "BaseTypes.h"
#include "NoteData.h"
#include "XmlExportable.h"

#include <vector>
#include <stdexcept>

#include <QString>

/**
 * An immutable type used for point and event representation in sparse
 * models, as well as for interchange within the clipboard. An event
 * always has a frame and (possibly empty) label, and optionally has
 * numerical value, level, duration in frames, and a mapped reference
 * frame. Event has an operator< defining a total ordering, by frame
 * first and then by the other properties.
 * 
 * Event is based on the Clipboard::Point type up to SV v3.2.1 and is
 * intended also to replace the custom point types previously found in
 * sparse models.
 */
class Event
{
public:
    Event(sv_frame_t frame) :
        m_haveValue(false), m_haveLevel(false), m_haveReferenceFrame(false),
        m_value(0.f), m_level(0.f), m_frame(frame),
        m_duration(0), m_referenceFrame(0), m_label() { }
        
    Event(sv_frame_t frame, QString label) :
        m_haveValue(false), m_haveLevel(false), m_haveReferenceFrame(false),
        m_value(0.f), m_level(0.f), m_frame(frame),
        m_duration(0), m_referenceFrame(0), m_label(label) { }
        
    Event(sv_frame_t frame, float value, QString label) :
        m_haveValue(true), m_haveLevel(false), m_haveReferenceFrame(false),
        m_value(value), m_level(0.f), m_frame(frame),
        m_duration(0), m_referenceFrame(0), m_label(label) { }
        
    Event(sv_frame_t frame, float value, sv_frame_t duration, QString label) :
        m_haveValue(true), m_haveLevel(false), m_haveReferenceFrame(false),
        m_value(value), m_level(0.f), m_frame(frame),
        m_duration(duration), m_referenceFrame(0), m_label(label) {
        if (m_duration < 0) throw std::logic_error("duration must be >= 0");
    }
        
    Event(sv_frame_t frame, float value, sv_frame_t duration,
          float level, QString label) :
        m_haveValue(true), m_haveLevel(true), m_haveReferenceFrame(false),
        m_value(value), m_level(level), m_frame(frame),
        m_duration(duration), m_referenceFrame(0), m_label(label) {
        if (m_duration < 0) throw std::logic_error("duration must be >= 0");
    }

    Event(const Event &event) =default;
    Event &operator=(const Event &event) =default;
    Event &operator=(Event &&event) =default;
    
    sv_frame_t getFrame() const { return m_frame; }

    Event withFrame(sv_frame_t frame) const {
        Event p(*this);
        p.m_frame = frame;
        return p;
    }
    
    bool hasValue() const { return m_haveValue; }
    float getValue() const { return m_value; }
    
    Event withValue(float value) const {
        Event p(*this);
        p.m_haveValue = true;
        p.m_value = value;
        return p;
    }
    Event withoutValue() const {
        Event p(*this);
        p.m_haveValue = false;
        p.m_value = 0.f;
        return p;
    }
    
    bool hasDuration() const { return m_duration != 0; }
    sv_frame_t getDuration() const { return m_duration; }

    Event withDuration(sv_frame_t duration) const {
        Event p(*this);
        p.m_duration = duration;
        if (duration < 0) throw std::logic_error("duration must be >= 0");
        return p;
    }
    Event withoutDuration() const {
        Event p(*this);
        p.m_duration = 0;
        return p;
    }
    
    QString getLabel() const { return m_label; }

    Event withLabel(QString label) const {
        Event p(*this);
        p.m_label = label;
        return p;
    }
    
    bool hasLevel() const { return m_haveLevel; }
    float getLevel() const { return m_level; }

    Event withLevel(float level) const {
        Event p(*this);
        p.m_haveLevel = true;
        p.m_level = level;
        return p;
    }
    Event withoutLevel() const {
        Event p(*this);
        p.m_haveLevel = false;
        p.m_level = 0.f;
        return p;
    }
    
    bool hasReferenceFrame() const { return m_haveReferenceFrame; }
    sv_frame_t getReferenceFrame() const { return m_referenceFrame; }
        
    bool referenceFrameDiffers() const { // from event frame
        return m_haveReferenceFrame && (m_referenceFrame != m_frame);
    }
    
    Event withReferenceFrame(sv_frame_t frame) const {
        Event p(*this);
        p.m_haveReferenceFrame = true;
        p.m_referenceFrame = frame;
        return p;
    }
    Event withoutReferenceFrame() const {
        Event p(*this);
        p.m_haveReferenceFrame = false;
        p.m_referenceFrame = 0;
        return p;
    }

    bool operator==(const Event &p) const {

        if (m_frame != p.m_frame) return false;
        if (m_duration != p.m_duration) return false;

        if (m_haveValue != p.m_haveValue) return false;
        if (m_haveValue && (m_value != p.m_value)) return false;

        if (m_haveLevel != p.m_haveLevel) return false;
        if (m_haveLevel && (m_level != p.m_level)) return false;

        if (m_haveReferenceFrame != p.m_haveReferenceFrame) return false;
        if (m_haveReferenceFrame &&
            (m_referenceFrame != p.m_referenceFrame)) return false;
        
        if (m_label != p.m_label) return false;
        
        return true;
    }

    bool operator<(const Event &p) const {

        if (m_frame != p.m_frame) return m_frame < p.m_frame;
        if (m_duration != p.m_duration) return m_duration < p.m_duration;

        // events without a property sort before events with that property

        if (m_haveValue != p.m_haveValue) return !m_haveValue;
        if (m_haveValue && (m_value != p.m_value)) return m_value < p.m_value;
        
        if (m_haveLevel != p.m_haveLevel) return !m_haveLevel;
        if (m_haveLevel && (m_level != p.m_level)) return m_level < p.m_level;

        if (m_haveReferenceFrame != p.m_haveReferenceFrame) {
            return !m_haveReferenceFrame;
        }
        if (m_haveReferenceFrame && (m_referenceFrame != p.m_referenceFrame)) {
            return m_referenceFrame < p.m_referenceFrame;
        }
        
        return m_label < p.m_label;
    }

    void toXml(QTextStream &stream,
               QString indent = "",
               QString extraAttributes = "") const {

        // For I/O purposes these are points, not events
        stream << indent << QString("<point frame=\"%1\" ").arg(m_frame);
        if (m_haveValue) stream << QString("value=\"%1\" ").arg(m_value);
        if (m_duration) stream << QString("duration=\"%1\" ").arg(m_duration);
        if (m_haveLevel) stream << QString("level=\"%1\" ").arg(m_level);
        if (m_haveReferenceFrame) stream << QString("referenceFrame=\"%1\" ")
                                      .arg(m_referenceFrame);
        stream << QString("label=\"%1\" ")
            .arg(XmlExportable::encodeEntities(m_label));
        stream << extraAttributes << ">\n";
    }

    QString toXmlString(QString indent = "",
                        QString extraAttributes = "") const {
        QString s;
        QTextStream out(&s);
        toXml(out, indent, extraAttributes);
        out.flush();
        return s;
    }

    NoteData toNoteData(sv_samplerate_t sampleRate, bool valueIsMidiPitch) {

        sv_frame_t duration;
        if (m_duration > 0) {
            duration = m_duration;
        } else {
            duration = sv_frame_t(sampleRate / 6); // arbitrary short duration
        }

        int midiPitch;
        float frequency = 0.f;
        if (m_haveValue) {
            if (valueIsMidiPitch) {
                midiPitch = int(roundf(m_value));
            } else {
                frequency = m_value;
                midiPitch = Pitch::getPitchForFrequency(frequency);
            }
        } else {
            midiPitch = 64;
            valueIsMidiPitch = true;
        }

        int velocity = 100;
        if (m_haveLevel) {
            if (m_level > 0.f && m_level <= 1.f) {
                velocity = int(roundf(m_level * 127.f));
            }
        }

        NoteData n(m_frame, duration, midiPitch, velocity);
        n.isMidiPitchQuantized = valueIsMidiPitch;
        if (!valueIsMidiPitch) {
            n.frequency = frequency;
        }

        return n;
    }
    
private:
    // The order of fields here is chosen to minimise overall size of struct.
    // We potentially store very many of these objects.
    // If you change something, check what difference it makes to packing.
    bool m_haveValue : 1;
    bool m_haveLevel : 1;
    bool m_haveReferenceFrame : 1;
    float m_value;
    float m_level;
    sv_frame_t m_frame;
    sv_frame_t m_duration;
    sv_frame_t m_referenceFrame;
    QString m_label;
};

typedef std::vector<Event> EventVector;

#endif