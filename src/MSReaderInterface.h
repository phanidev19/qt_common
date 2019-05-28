/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef __MS_READER_INTERFACE_H__
#define __MS_READER_INTERFACE_H__

#include "common_errors.h"
#include "MSReaderTypes.h"
#include "ProgressBarInterface.h"

#include <pmi_common_ms_export.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

/*!
 * \brief The MSReaderInterface is interface for all known MS file types.
 */
class PMI_COMMON_MS_EXPORT MSReaderInterface
{
public:
    virtual Err openFile(const QString &filename,
                         msreader::MSConvertOption convert_options
                         = msreader::ConvertWithCentroidButNoPeaksBlobs,
                         QSharedPointer<ProgressBarInterface> progress = NoProgress)
        = 0;
    virtual Err closeFile() = 0;

    virtual bool canOpen(const QString &filename) const = 0;

    virtual Err getLockmassScans(QList<msreader::ScanInfoWrapper> *lockmassList) const = 0;
    virtual Err getScanInfoListAtLevel(int level,
        QList<msreader::ScanInfoWrapper> *lockmassList) const = 0;

    virtual Err getBasePeak(point2dList *points) const = 0;
    virtual Err getTICData(point2dList *points) const = 0;
    virtual Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                            msreader::PointListAsByteArrays *pointListAsByteArrays = nullptr) = 0;
    virtual Err getXICData(const msreader::XICWindow &win, point2dList *points,
                           int msLevel = 1) const = 0;
    virtual Err getScanInfo(long scanNumber, msreader::ScanInfo *obj) const = 0;
    virtual Err getScanPrecursorInfo(long scanNumber, msreader::PrecursorInfo *pinfo) const = 0;
    virtual Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const = 0;

    virtual Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const = 0;

    virtual Err getChromatograms(QList<msreader::ChromatogramInfo> *chroList,
                                 const QString &internalChannelName = QString())
        = 0;

    virtual Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const = 0;
    virtual Err getBestScanNumber(double scanTimeMinutes, int msLevel, long *scanNumber) const = delete;

};

_PMI_END

#endif
