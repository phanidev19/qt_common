/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#ifndef MSREADER_TESTING_H
#define MSREADER_TESTING_H

#include "MSReaderBase.h"
#include "pmi_core_defs.h"

#include "CentroidOptions.h"
#include "MSReaderBase.h"
#include "Reader.h"
#include <QList>
#include <QMap>
#include <QSharedPointer>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>

#include "common_errors.h"

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT MSReaderTesting : public MSReaderBase
{

public:
    MSReaderTesting();

    static const QString MAGIC_FILENAME;

    // MSReaderBase interface

    MSReaderClassType classTypeName() const override { return MSReaderClassTypeTesting; }

    void setEnableCanOpen(bool on) { m_canOpen = on; }

    bool canOpen(const QString &filename) const override;

    Err openFile(const QString &filename,
                 msreader::MSConvertOption convert_options
                 = msreader::ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override
    {
        return kNoErr;
    }
    Err closeFile() override { return kNoErr; }

    Err getBasePeak(point2dList *points) const override { return kNoErr; }
    Err getTICData(point2dList *points) const override { return kNoErr; }
    /// Note: should be const, but the updating the byspec prevents us from doing that.
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                            msreader::PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const msreader::XICWindow &win, point2dList *points,
                           int ms_level = 1) const override
    {
        return kNoErr;
    }
    Err getScanInfo(long scanNumber, msreader::ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, msreader::PrecursorInfo *pinfo) const override { return kNoErr; }

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override
    {
        return kNoErr;
    }

    Err cacheScanNumbers(const QList<int> &scanNumberList, QSharedPointer<ProgressBarInterface> progress)
    {
        return kNoErr;
    }

    Err getChromatograms(QList<msreader::ChromatogramInfo> *chroList,
                         const QString &channelInternalName = QString()) override
    {
        return kNoErr;
    }

    using MSReaderBase::getBestScanNumber;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override
    {
        return kNoErr;
    }

    // mocks
    void setScanData(const point2dList &points) { m_scanData = points; }

private:
    bool m_canOpen;
    point2dList m_scanData;
};

_PMI_END

#endif // MSREADER_TESTING_H
