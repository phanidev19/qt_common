/*!
 * \brief The DaoEntry represents the Spectrum table in the byspec2 db format.
 */
/*!
 * \brief The DaoEntry represents the Spectrum table in the byspec2 db format.
 */
/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Victor Suponev vsuponev@milosolutions.com
 */

#ifndef BYSPEC2TYPES_H
#define BYSPEC2TYPES_H

#include <pmi_common_ms_export.h>
#include <pmi_core_defs.h>

#include <QString>
#include <QVector>

_PMI_BEGIN

namespace byspec2 {

/*!
 * \brief The ChromatogramDaoEntry represents the Chromatogram table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT ChromatogramDaoEntry {
    int id = 0;
    int filesId = 0;
    int dataCount = 0;
    QString identifier;
    QString chromatogramType;
    QString metaText;
    QString debugText;
    QVector<double> dataX;
    QVector<double> dataY;
};

/*!
 * \brief The SpectrumDaoEntry represents the Spectrum table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT SpectrumDaoEntry {
    int id = 0;
    int filesId = 0;
    int peaksId = 0;
    int msLevel = 0;
    int scanNumber = 0;
    int parentScanNumber = 0;
    float retentionTime = 0.0f;
    double isolationWindowLowerOffset = 0.0;
    double isolationWindowUpperOffset = 0.0;
    double observedMz = 0.0;
    double precursorIntensity = 0.0;
    QString chargeList;
    QString comment;
    QString debugText;
    QString fragmentationType;
    QString metaText;
    QString nativeId;
    QString parentNativeId;
};

/*!
 * \brief The PeakDaoEntry represents the Peak table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT PeakDaoEntry {
    int id = 0;
    int compressionInfoId = 0;
    int peaksCount = 0;
    double intensitySum = 0.0;
    QVector<float> peaksIntensity;
    QVector<double> peaksMz;
    QString comment;
    QString metaText;
    QString spectraIdList;
};

/*!
 * \brief The CompressionInfoDaoEntry represents the CompressionInfo table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT CompressionInfoDaoEntry {
    int id = 0;
    QString property;
    QString version;
    QByteArray intensityDict;
    QByteArray mzDict;
};

/*!
 * \brief The FileDaoEntry represents the File table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT FileDaoEntry {
    int id = 0;
    QString filename;
    QString location;
    QString signature;
    QString type;
};

/*!
 * \brief The FileInfoDaoEntry represents the FileInfo table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT FileInfoDaoEntry {
    int id = 0;
    int filesId = 0;
    QString key;
    QString value;
};

/*!
 * \brief The InfoDaoEntry represents the Info table entry in the byspec2 db format.
 */
struct PMI_COMMON_MS_EXPORT InfoDaoEntry {
    int id = 0;
    QString key;
    QString value;
};

} // namespace byspec2

_PMI_END

#endif // BYSPEC2TYPES_H
