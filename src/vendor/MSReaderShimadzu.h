/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MSREADERSHIMADZU_H
#define MSREADERSHIMADZU_H

#include "CommonVendor.h"
#include "MSReaderBase.h"
#include "common_errors.h"

_PMI_BEGIN
_MSREADER_BEGIN

class PMI_COMMON_MS_TEST_EXPORT MSReaderShimadzu : public MSReaderBase
{
public:
    MSReaderShimadzu();
    virtual ~MSReaderShimadzu();

    Q_DISABLE_COPY(MSReaderShimadzu);

    virtual MSReaderClassType classTypeName() const { return MSReaderClassTypeShimadzu; }

    bool canOpen(const QString &filename) const override;
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = nullptr) override;
    Err closeFile() override;

    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &win, point2dList *points, int ms_level = 1) const override;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;

    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;

    /*!
     * \brief This function will eventually generalize getTIC and getUV.  For now, this replace
     * getUVData() but not getTICData() \param chroList \return Err
     */
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &internalChannelName = QString()) override;

    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

#ifdef PMI_QT_COMMON_BUILD_TESTING
public:
    struct DebugOptions
    {
        /*!
        * \brief sets max scan number for brute force increment
        *       see ShimadzuReaderWrapperInterface.h for additional info
        */
        int getNumberOfSpectraMaxScan = -1;
    };
    DebugOptions debugOptions() const;
    void setDebugOptions(const DebugOptions& debugOptions);
#endif

private:
    Err getTICDataByEvent(int eventNo, point2dList *points) const;

private:
    class PrivateData;
    QScopedPointer<PrivateData> d;
};

_MSREADER_END
_PMI_END

#endif // MSREADERSHIMADZU_H
