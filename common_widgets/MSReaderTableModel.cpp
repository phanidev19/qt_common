/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "MSReaderTableModel.h"

MSReaderTableModel::MSReaderTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_peakModeMap.insert(pmi::msreader::PeakPickingModeUnknown, "Unknown");
    m_peakModeMap.insert(pmi::msreader::PeakPickingProfile, "Profile");
    m_peakModeMap.insert(pmi::msreader::PeakPickingCentroid, "Centroid");

    m_scanMethodMap.insert(pmi::msreader::ScanMethodUnknown, "Unknown");
    m_scanMethodMap.insert(pmi::msreader::ScanMethodFullScan, "Full Scan");
    m_scanMethodMap.insert(pmi::msreader::ScanMethodZoomScan, "Zoom Scan");
}

int MSReaderTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return m_msInfoList.size();
}

int MSReaderTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return MAX_SIZE;
}

QVariant MSReaderTableModel::data(const QModelIndex &index, int role) const
{
    QVariant value;
    if (Qt::DisplayRole == role) {
        int row(index.row());
        int column(index.column());
        pmi::msreader::ScanInfo info(m_msInfoList[row].second.scanInfo);
        pmi::msreader::PrecursorInfo precursorInfo(m_msInfoList[row].second.precursorInfo);

        switch (column) {
        case SCAN_NUMBER_COLUMN:
            value = m_msInfoList[row].first;
            break;
        case RETENTION_TIME_MINUTES:
            value = info.retTimeMinutes;
            break;
        case SCAN_LEVEL:
            value = info.scanLevel;
            break;
        case NATIVE_ID:
            value = info.nativeId;
            break;
        case PEAK_MODE:
            value = m_peakModeMap[info.peakMode];
            break;
        case SCAN_METHOD:
            value = m_scanMethodMap[info.scanMethod];
            break;
        case OBSERVED_MZ:
            if (info.scanLevel > 1) {
                value = precursorInfo.dMonoIsoMass;
            }
            break;
        case LOWER_WINDOW_OFFSET:
            if (info.scanLevel > 1) {
                value = precursorInfo.lowerWindowOffset;
            }
            break;
        case UPPER_WINDOW_OFFSET:
            if (info.scanLevel > 1) {
                value = precursorInfo.upperWindowOffset;
            }
            break;
        case CHARGE_LIST:
            if (info.scanLevel > 1) {
                value = int(precursorInfo.nChargeState);
            }
            break;
        case PARENT_NATIVE_ID:
            if (info.scanLevel > 1) {
                value = precursorInfo.nativeId;
            }
            break;
        case FRAGMENTATION_TYPE:
            value = m_msInfoList[row].second.fragmentationType;
            break;
        default:
            break;
        }
    }

    if (Qt::TextAlignmentRole == role) {
        return Qt::AlignCenter;
    }

    return value;
}

QVariant MSReaderTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    QString header;
    if (Qt::Horizontal == orientation) {
        switch (section) {
        case SCAN_NUMBER_COLUMN:
            header = tr("Scan Number");
            break;
        case RETENTION_TIME_MINUTES:
            header = tr("Scan Time (mins)");
            break;
        case SCAN_LEVEL:
            header = tr("Scan Level");
            break;
        case NATIVE_ID:
            header = tr("Native ID");
            break;
        case PEAK_MODE:
            header = tr("Peak Mode");
            break;
        case SCAN_METHOD:
            header = tr("Scan Method");
            break;
        case OBSERVED_MZ:
            header = tr("Observed Mz");
            break;
        case LOWER_WINDOW_OFFSET:
            header = tr("Lower Window Offset");
            break;
        case UPPER_WINDOW_OFFSET:
            header = tr("Upper Window Offset");
            break;
        case CHARGE_LIST:
            header = tr("Charge List");
            break;
        case PARENT_NATIVE_ID:
            header = tr("Parent Native ID");
            break;
        case FRAGMENTATION_TYPE:
            header = tr("Fragmentation type");
            break;
        default:
            break;
        }
    }

    if (Qt::Vertical == orientation) {
        header = QString("%1").arg(section + 1);
    }

    if (Qt::DisplayRole == role && !header.isEmpty()) {
        return header;
    }

    return QVariant();
}

bool MSReaderTableModel::insertRows(int position, int rows,
                                    const QModelIndex &index = QModelIndex())
{
    beginInsertRows(index, position, position + rows - 1);
    endInsertRows();
    return true;
}

bool MSReaderTableModel::removeRows(int position, int rows,
                                    const QModelIndex &index = QModelIndex())
{
    beginRemoveRows(index, position, position + rows - 1);
    endRemoveRows();
    return true;
}

void MSReaderTableModel::setMSInfoList(const MSInfoList &list)
{
    if (m_msInfoList.size() > 0) {
        removeRows(0, m_msInfoList.size());
    }
    m_msInfoList = list;
    if (m_msInfoList.size() > 0) {
        insertRows(0, m_msInfoList.size());
        emit dataChanged(this->index(0, 0), this->index(rowCount() - 1, columnCount() - 1));
    }
}
