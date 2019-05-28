/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: David Todd
* davidtodd.rtts@yahoo.com
*/

#ifndef MS_EQUISPACED_DATA_H
#define MS_EQUISPACED_DATA_H

#include "pmi_common_ms_export.h"
#include "pmi_core_defs.h"
#include <QString>
#include <QVector>

/*! \brief Object for storing uncompressed Tile Data.
*
* Used for researching and analyzing detailed trends in data, and saving to csv file.
*/

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT MSEquispacedData
{
    
public:
    MSEquispacedData() = default;
    MSEquispacedData(unsigned int mzPts, unsigned int timePts, qreal minX, qreal stepX,
        int minY, int stepY);
    ~MSEquispacedData();

    bool setSize(unsigned int mzPts, unsigned int timePts, qreal minX, qreal stepX,
        int minY, int stepY);
    bool setPoint(unsigned int mzIndex, unsigned int timeIndex, double val);

    const QVector<double> sliceAtTimeIndex(unsigned int timeSliceIndex); // mzSlice
    QVector<double> sliceAtMzIndex(unsigned int mzIndex); // timeSlice

    void saveToCsv(const QString &csvFilePath);

private:
    const QVector<double> * slicePtrAtTimeIndex(unsigned int timeSliceIndex); // mzSlice

private:
    QVector<QVector<double>> m_mzTimePoints;
    bool m_msInitialized = false;

    qreal m_minX = 0;
    qreal m_stepX = 0;
    int m_minY = 0;
    int m_stepY = 0;

    QVector<double> m_timeValues;
    QVector<double> m_mzValues;
};

_PMI_END

#endif // MS_EQUISPACED_DATA_H