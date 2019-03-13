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

#ifndef SV_NOTE_MODEL_H
#define SV_NOTE_MODEL_H

#include "Model.h"
#include "TabularModel.h"
#include "base/UnitDatabase.h"
#include "base/EventSeries.h"
#include "base/NoteData.h"
#include "base/NoteExportable.h"
#include "base/RealTime.h"
#include "base/PlayParameterRepository.h"
#include "base/Pitch.h"
#include "system/System.h"

#include <QMutex>
#include <QMutexLocker>

class NoteModel : public Model,
                  public TabularModel,
                  public NoteExportable
{
    Q_OBJECT
    
public:
    NoteModel(sv_samplerate_t sampleRate, int resolution,
              bool notifyOnAdd = true) :
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_valueMinimum(0.f),
        m_valueMaximum(0.f),
        m_haveExtents(false),
        m_valueQuantization(0),
        m_units(""),
        m_extendTo(0),
        m_notifyOnAdd(notifyOnAdd),
        m_sinceLastNotifyMin(-1),
        m_sinceLastNotifyMax(-1),
        m_completion(0) {
        PlayParameterRepository::getInstance()->addPlayable(this);
    }

    NoteModel(sv_samplerate_t sampleRate, int resolution,
              float valueMinimum, float valueMaximum,
              bool notifyOnAdd = true) :
        m_sampleRate(sampleRate),
        m_resolution(resolution),
        m_valueMinimum(valueMinimum),
        m_valueMaximum(valueMaximum),
        m_haveExtents(true),
        m_valueQuantization(0),
        m_units(""),
        m_extendTo(0),
        m_notifyOnAdd(notifyOnAdd),
        m_sinceLastNotifyMin(-1),
        m_sinceLastNotifyMax(-1),
        m_completion(0) {
        PlayParameterRepository::getInstance()->addPlayable(this);
    }

    virtual ~NoteModel() {
        PlayParameterRepository::getInstance()->removePlayable(this);
    }
    
    QString getTypeName() const override { return tr("Note"); }

    bool isOK() const override { return true; }
    sv_frame_t getStartFrame() const override { return m_events.getStartFrame(); }
    sv_frame_t getEndFrame() const override { return m_events.getEndFrame(); }
    sv_samplerate_t getSampleRate() const override { return m_sampleRate; }

    bool canPlay() const override { return true; }
    QString getDefaultPlayClipId() const override {
        return "elecpiano";
    }

    QString getScaleUnits() const { return m_units; }
    void setScaleUnits(QString units) {
        m_units = units;
        UnitDatabase::getInstance()->registerUnit(units);
    }

    float getValueQuantization() const { return m_valueQuantization; }
    void setValueQuantization(float q) { m_valueQuantization = q; }

    float getValueMinimum() const { return m_valueMinimum; }
    float getValueMaximum() const { return m_valueMaximum; }
    
    int getCompletion() const { return m_completion; }

    void setCompletion(int completion, bool update = true) {

        bool emitCompletionChanged = true;
        bool emitGeneralModelChanged = false;
        bool emitRegionChanged = false;

        {
            QMutexLocker locker(&m_mutex);

            if (m_completion != completion) {
                m_completion = completion;

                if (completion == 100) {

                    if (m_notifyOnAdd) {
                        emitCompletionChanged = false;
                    }

                    m_notifyOnAdd = true; // henceforth
                    emitGeneralModelChanged = true;

                } else if (!m_notifyOnAdd) {

                    if (update &&
                        m_sinceLastNotifyMin >= 0 &&
                        m_sinceLastNotifyMax >= 0) {
                        emitRegionChanged = true;
                    }
                }
            }
        }

        if (emitCompletionChanged) {
            emit completionChanged();
        }
        if (emitGeneralModelChanged) {
            emit modelChanged();
        }
        if (emitRegionChanged) {
            emit modelChangedWithin(m_sinceLastNotifyMin, m_sinceLastNotifyMax);
            m_sinceLastNotifyMin = m_sinceLastNotifyMax = -1;
        }        
    }
    
    void toXml(QTextStream &out,
               QString indent = "",
               QString extraAttributes = "") const override {

        //!!! what is valueQuantization used for?
        
        Model::toXml
            (out,
             indent,
             QString("type=\"sparse\" dimensions=\"3\" resolution=\"%1\" "
                     "notifyOnAdd=\"%2\" dataset=\"%3\" subtype=\"note\" "
                     "valueQuantization=\"%4\" minimum=\"%5\" maximum=\"%6\" "
                     "units=\"%7\" %8")
             .arg(m_resolution)
             .arg(m_notifyOnAdd ? "true" : "false")
             .arg(getObjectExportId(&m_events))
             .arg(m_valueQuantization)
             .arg(m_valueMinimum)
             .arg(m_valueMaximum)
             .arg(m_units)
             .arg(extraAttributes));

        m_events.toXml(out, indent, QString("dimensions=\"3\""));
    }

    /**
     * TabularModel methods.  
     */

    int getRowCount() const override {
        return m_events.count();
    }
    
    int getColumnCount() const override {
        return 6;
    }

    bool isColumnTimeValue(int column) const override {
        // NB duration is not a "time value" -- that's for columns
        // whose sort ordering is exactly that of the frame time
        return (column < 2);
    }

    sv_frame_t getFrameForRow(int row) const override {
        if (row < 0 || row >= m_events.count()) {
            return 0;
        }
        Event e = m_events.getEventByIndex(row);
        return e.getFrame();
    }

    int getRowForFrame(sv_frame_t frame) const override {
        return m_events.getIndexForEvent(Event(frame));
    }
    
    QString getHeading(int column) const override {
        switch (column) {
        case 0: return tr("Time");
        case 1: return tr("Frame");
        case 2: return tr("Pitch");
        case 3: return tr("Duration");
        case 4: return tr("Level");
        case 5: return tr("Label");
        default: return tr("Unknown");
        }
    }

    QVariant getData(int row, int column, int role) const override {

        if (row < 0 || row >= m_events.count()) {
            return QVariant();
        }

        Event e = m_events.getEventByIndex(row);

        switch (column) {
        case 0: return adaptFrameForRole(e.getFrame(), getSampleRate(), role);
        case 1: return int(e.getFrame());
        case 2: return adaptValueForRole(e.getValue(), getScaleUnits(), role);
        case 3: return int(e.getDuration());
        case 4: return e.getLevel();
        case 5: return e.getLabel();
        default: return QVariant();
        }
    }

    class EditCommand : public Command
    {
    public:
        //!!! borrowed ptr
        EditCommand(NoteModel *model, QString name) :
            m_model(model), m_name(name) { }

        QString getName() const override {
            return m_name;
        }

        void addPoint(Event e) {
            m_add.insert(e);
        }
        void deletePoint(Event e) {
            m_remove.insert(e);
        }
        
        void execute() override {
            for (const Event &e: m_add) m_model->addPoint(e);
            for (const Event &e: m_remove) m_model->deletePoint(e);
        }

        void unexecute() override {
            for (const Event &e: m_remove) m_model->addPoint(e);
            for (const Event &e: m_add) m_model->deletePoint(e);
        }

    private:
        NoteModel *m_model;
        std::set<Event> m_add;
        std::set<Event> m_remove;
        QString m_name;
    };

    //!!! rename Point to Note throughout? Just because we can now?
    void addPoint(Event e) {

        bool allChange = false;
           
        {
            QMutexLocker locker(&m_mutex);
            m_events.add(e);
//!!!???        if (point.getLabel() != "") m_hasTextLabels = true;

            float v = e.getValue();
            if (!ISNAN(v) && !ISINF(v)) {
                if (!m_haveExtents || v < m_valueMinimum) {
                    m_valueMinimum = v; allChange = true;
                }
                if (!m_haveExtents || v > m_valueMaximum) {
                    m_valueMaximum = v; allChange = true;
                }
                m_haveExtents = true;
            }
            
            sv_frame_t f = e.getFrame();

            if (!m_notifyOnAdd) {
                if (m_sinceLastNotifyMin == -1 || f < m_sinceLastNotifyMin) {
                    m_sinceLastNotifyMin = f;
                }
                if (m_sinceLastNotifyMax == -1 || f > m_sinceLastNotifyMax) {
                    m_sinceLastNotifyMax = f;
                }
            }
        }
        
        if (m_notifyOnAdd) {
            emit modelChangedWithin(e.getFrame(),
                                    e.getFrame() + e.getDuration() + m_resolution);
        }
        if (allChange) {
            emit modelChanged();
        }
    }
    
    void deletePoint(Event e) {
        {
            QMutexLocker locker(&m_mutex);
            m_events.remove(e);
        }
        emit modelChangedWithin(e.getFrame(),
                                e.getFrame() + e.getDuration() + m_resolution);
    }
    
    EventVector getPoints() const /*!!! override? - and/or rename? */ {
        EventVector ee;
        for (int i = 0; i < m_events.count(); ++i) {
            ee.push_back(m_events.getEventByIndex(i));
        }
        return ee;
    }

    //!!! bleah
    EventVector getPoints(sv_frame_t start, sv_frame_t end) const {
        return m_events.getEventsSpanning(start, end - start);        
    }
    
    int getPointCount() const {
        return m_events.count();
    }

    bool isEmpty() const {
        return m_events.isEmpty();
    }

    bool containsPoint(const Event &e) const {
        return m_events.contains(e);
    }

    Command *getSetDataCommand(int row, int column, const QVariant &value, int role) override
    {
        if (row < 0 || row >= m_events.count()) return nullptr;
        if (role != Qt::EditRole) return nullptr;

        Event e0 = m_events.getEventByIndex(row);
        Event e1;

        switch (column) {
        case 0: e1 = e0.withFrame(sv_frame_t(round(value.toDouble() *
                                                   getSampleRate()))); break;
        case 1: e1 = e0.withFrame(value.toInt()); break;
        case 2: e1 = e0.withValue(float(value.toDouble())); break;
        case 3: e1 = e0.withDuration(value.toInt()); break;
        case 4: e1 = e0.withLevel(float(value.toDouble())); break;
        case 5: e1 = e0.withLabel(value.toString()); break;
        }

        EditCommand *command = new EditCommand(this, tr("Edit Data"));
        command->deletePoint(e0);
        command->addPoint(e1);
        return command;
    }

    SortType getSortType(int column) const override
    {
        if (column == 5) return SortAlphabetical;
        return SortNumeric;
    }

    /**
     * NoteExportable methods.
     */

    NoteList getNotes() const override {
        return getNotesStartingWithin(getStartFrame(),
                                      getEndFrame() - getStartFrame());
    }

    NoteList getNotesActiveAt(sv_frame_t frame) const override {

        NoteList notes;
        EventVector ee = m_events.getEventsCovering(frame);
        for (const auto &e: ee) {
            notes.push_back(e.toNoteData(getSampleRate(),
                                         getScaleUnits() != "Hz"));
        }
        return notes;
    }
    
    NoteList getNotesStartingWithin(sv_frame_t startFrame,
                                    sv_frame_t duration) const override {

        NoteList notes;
        EventVector ee = m_events.getEventsStartingWithin(startFrame, duration);
        for (const auto &e: ee) {
            notes.push_back(e.toNoteData(getSampleRate(),
                                         getScaleUnits() != "Hz"));
        }
        return notes;
    }

protected:
    sv_samplerate_t m_sampleRate;
    int m_resolution;

    float m_valueMinimum;
    float m_valueMaximum;
    bool m_haveExtents;
    float m_valueQuantization;
    QString m_units;
    
    sv_frame_t m_extendTo;

    bool m_notifyOnAdd;
    sv_frame_t m_sinceLastNotifyMin;
    sv_frame_t m_sinceLastNotifyMax;

    EventSeries m_events;

    int m_completion;

    mutable QMutex m_mutex;

    //!!! do we have general docs for ownership and synchronisation of models?
    // this might be a good opportunity to stop using bare pointers to them
};

#endif
