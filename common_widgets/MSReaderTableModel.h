/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef MSREADER_TABLE_MODEL_H
#define MSREADER_TABLE_MODEL_H

#include "MSReaderBase.h"
#include "MSReaderWidget.h"
#include <QAbstractTableModel>

class MSReaderTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum {
        SCAN_NUMBER_COLUMN = 0,
        RETENTION_TIME_MINUTES,
        SCAN_LEVEL,
        NATIVE_ID,
        PEAK_MODE,
        SCAN_METHOD,
        OBSERVED_MZ,
        LOWER_WINDOW_OFFSET,
        UPPER_WINDOW_OFFSET,
        CHARGE_LIST,
        PARENT_NATIVE_ID,
        FRAGMENTATION_TYPE,
        MAX_SIZE
    };
    explicit MSReaderTableModel(QObject *parent);
    void setMSInfoList(const MSInfoList &list);
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

protected:
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    bool removeRows(int position, int rows, const QModelIndex &index);
    bool insertRows(int position, int rows, const QModelIndex &index);

private:
    MSInfoList m_msInfoList;
    QMap<int, QString> m_peakModeMap;
    QMap<int, QString> m_scanMethodMap;
};

#endif // MSREADER_TABLE_MODEL_H
