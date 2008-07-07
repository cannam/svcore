/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Sonic Visualiser
    An audio file viewer and annotation editor.
    Centre for Digital Music, Queen Mary, University of London.
    This file copyright 2008 QMUL.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _MODEL_DATA_TABLE_MODEL_H_
#define _MODEL_DATA_TABLE_MODEL_H_

#include <QAbstractItemModel>

#include <vector>

class TabularModel;
class Command;

class ModelDataTableModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ModelDataTableModel(TabularModel *m);
    virtual ~ModelDataTableModel();

    QVariant data(const QModelIndex &index, int role) const;

    bool setData(const QModelIndex &index, const QVariant &value, int role);

    bool insertRow(int row, const QModelIndex &parent = QModelIndex());
    bool removeRow(int row, const QModelIndex &parent = QModelIndex());

    Qt::ItemFlags flags(const QModelIndex &index) const;

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const;

    QModelIndex index(int row, int column,
                      const QModelIndex &parent = QModelIndex()) const;

    QModelIndex parent(const QModelIndex &index) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QModelIndex getModelIndexForFrame(size_t frame) const;
    size_t getFrameForModelIndex(const QModelIndex &) const;

    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder);

    void setCurrentRow(int row);
    int getCurrentRow();

signals:
    void frameSelected(size_t);
    void addCommand(Command *);
    void currentChanged(const QModelIndex &);

protected slots:
    void modelChanged();
    void modelChanged(size_t, size_t);

protected:
    TabularModel *m_model;
    int m_sortColumn;
    Qt::SortOrder m_sortOrdering;
    int m_currentRow;
    typedef std::vector<int> RowList;
    mutable RowList m_sort;
    mutable RowList m_rsort;
    int getSorted(int row) const;
    int getUnsorted(int row) const;
    void resort() const;
    void resortNumeric() const;
    void resortAlphabetical() const;
    void clearSort();
};

#endif