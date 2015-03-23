/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2007 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _ALIGNMENT_MODEL_H_
#define _ALIGNMENT_MODEL_H_

#include "Model.h"
#include "PathModel.h"
#include "base/RealTime.h"

#include <QString>
#include <QStringList>

class SparseTimeValueModel;

class AlignmentModel : public Model
{
    Q_OBJECT

public:
    AlignmentModel(Model *reference,
                   Model *aligned,
                   Model *inputModel, // probably an AggregateWaveModel; may be null; I take ownership
                   SparseTimeValueModel *path); // I take ownership
    ~AlignmentModel();

    virtual bool isOK() const;
    virtual int getStartFrame() const;
    virtual int getEndFrame() const;
    virtual int getSampleRate() const;
    virtual Model *clone() const;
    virtual bool isReady(int *completion = 0) const;
    virtual const ZoomConstraint *getZoomConstraint() const;

    QString getTypeName() const { return tr("Alignment"); }

    const Model *getReferenceModel() const;
    const Model *getAlignedModel() const;

    int toReference(int frame) const;
    int fromReference(int frame) const;

    void setPathFrom(SparseTimeValueModel *rawpath);
    void setPath(PathModel *path);

    virtual void toXml(QTextStream &stream,
                       QString indent = "",
                       QString extraAttributes = "") const;

signals:
    void modelChanged();
    void modelChangedWithin(int startFrame, int endFrame);
    void completionChanged();

protected slots:
    void pathChanged();
    void pathChangedWithin(int startFrame, int endFrame);
    void pathCompletionChanged();

protected:
    Model *m_reference; // I don't own this
    Model *m_aligned; // I don't own this

    Model *m_inputModel; // I own this

    SparseTimeValueModel *m_rawPath; // I own this
    mutable PathModel *m_path; // I own this
    mutable PathModel *m_reversePath; // I own this
    bool m_pathBegun;
    bool m_pathComplete;

    void constructPath() const;
    void constructReversePath() const;

    int align(PathModel *path, int frame) const;
};

#endif
