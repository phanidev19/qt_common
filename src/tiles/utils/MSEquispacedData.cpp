/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: David Todd, davidtodd.rtts@yahoo.com
 */

#include "MSEquispacedData.h"
#include "CsvWriter.h"
#include "pmi_common_ms_debug.h"

#include <QString>
#include <QStringList>

_PMI_BEGIN

QStringList qStringListFromArray(const QString &rowTitle, const QVector<double> &vec,
                                 unsigned int formatPrecision = 6)
{
    // Create an list of strings from a title and a vector of doubles.
    // Each value is converted to a string with a fixed number of digits
    // after the decimal, and with all trailing zeros eliminated.
    QStringList outLine;
    outLine.append(rowTitle);
    if (vec.empty()) {
        return outLine;
    }
    auto itv = vec.begin();
    for (; itv != vec.end(); itv++) {
        if (*itv == 0.0) {
            outLine.append(QString("0"));
        } else {
            outLine.append(QString::number(*itv, 'f', formatPrecision));
        }
    }    
    return outLine;
}

MSEquispacedData::MSEquispacedData(unsigned int mzPts, unsigned int timePts, qreal minX,
    qreal stepX, int minY, int stepY)
{
    // Create a 2 dimensional array of data to hold our data.
    setSize(mzPts, timePts, minX, stepX, minY, stepY);
}

MSEquispacedData::~MSEquispacedData() {
    // QVectors and a vector of QVectors should clean themselves up.
}

bool MSEquispacedData::setSize(unsigned int mzPts, unsigned int timePts, qreal minX, qreal stepX,
    int minY, int stepY)
{
    if (m_msInitialized) {
        debugMs() << "setSize() was called on a previoulsy initialized object";
        return false;
    }
    m_stepX = stepX;
    m_stepY = stepY;
    m_minX = minX;
    m_minY = minY;

    // Create a 2 dimensional array of data to hold our data.
    m_mzTimePoints.resize(timePts);
    for (unsigned int i = 0; i < timePts; ++i) {
        m_mzTimePoints[i].resize(mzPts);
    }

    // Create 1-dimensional arrays to hold our actual time and mz values.
    m_timeValues.resize(timePts);
    for (unsigned int i = 0; i < timePts; ++i) {
        m_timeValues[i] = m_minY + i * m_stepY;
    }
    m_mzValues.resize(mzPts);
    for (unsigned int i = 0; i < mzPts; ++i) {
        m_mzValues[i] = m_minX + i * m_stepX;
    }
    m_msInitialized = true;
    return true;
}

bool MSEquispacedData::setPoint(unsigned int mzIndex, unsigned int timeIndex, double val)
{
    if (mzIndex < (unsigned int)m_mzValues.size() && timeIndex < (unsigned int)m_timeValues.size()) {
        m_mzTimePoints[timeIndex].replace(mzIndex, val);
        return true;
    }
    return false;
}

const QVector<double> MSEquispacedData::sliceAtTimeIndex(unsigned int timeSliceIndex)
{
    // returning one of our time slices.
    if (timeSliceIndex < (unsigned int)m_mzTimePoints.size()) {
        return m_mzTimePoints[timeSliceIndex];
    }
    qDebug() << "timeSliceIndex exceeds size of array. Returning empty vector.";
    QVector<double> emptyVec;
    return emptyVec;
}

const QVector<double> *MSEquispacedData::slicePtrAtTimeIndex(unsigned int timeSliceIndex)
{
    // returning one of our time slices.
    if (timeSliceIndex < (unsigned int)m_mzTimePoints.size()) {
        return &(m_mzTimePoints[timeSliceIndex]);
    }
    return nullptr;
}

QVector<double> MSEquispacedData::sliceAtMzIndex(unsigned int mzIndex)
{
    // We don't have an object of this type at the moment.
    // So we'll have to create one.
    QVector<double> newTimeSlice;
    if (mzIndex < (unsigned int)m_mzValues.size()) {
        newTimeSlice.resize(m_mzValues.size());
        for (int i = 0; i < m_mzTimePoints.size(); ++i) {
            newTimeSlice[i] = m_mzTimePoints[i][mzIndex];
        }
    } else {
        qDebug() << "mzIndex exceeds size of array. Returning empty vector.";
    }
    return newTimeSlice;
}

void MSEquispacedData::saveToCsv(const QString &csvRawFilePath)
{
    bool rowSuccess;

    CsvWriter writer(csvRawFilePath, "\r\n", ',');
    if (!writer.open()) {
        warningMs() << "Cannot open destination file" << csvRawFilePath;
        return;
    }

    int count = 0;
    // Add in the column and row identifiers.
    // Column headers first.
    QString rowTitle("Time");
    QStringList rowList = qStringListFromArray(rowTitle, m_mzValues);
    rowSuccess = writer.writeRow(rowList);
    if (rowSuccess) {
        for (int i = 0; i < m_timeValues.size(); ++i) {
            // Row titles in minutes.
            rowTitle = QString::number(m_timeValues[i]);
            const QVector<double> *vp = slicePtrAtTimeIndex(i);
            if (vp == nullptr) {
                break;
            }
            rowList = qStringListFromArray(rowTitle, *vp);
            rowSuccess = writer.writeRow(rowList);
            if (rowSuccess) {
                count += 1;
            } else {
                break;
            }
        }
    }
}

_PMI_END