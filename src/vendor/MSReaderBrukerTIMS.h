/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
 */

#ifndef MSREADERBRUKERTIMS_H
#define MSREADERBRUKERTIMS_H

#include "CommonVendor.h"
#include "MSReaderBase.h"

_PMI_BEGIN

_MSREADER_BEGIN

struct MSReaderBrukerTimsPrivate;

class PMI_COMMON_MS_TEST_EXPORT MSReaderBrukerTims : public MSReaderBase
{
    struct ScanRec {
        int firstFrameId;
        int precursorId;
        int msLevel;
        int parentScan;

        ScanRec()
        {
            firstFrameId = 0;
            precursorId = 0;
            msLevel = 0;
            parentScan = 0;
        }

        ScanRec(int firstFrameId1, int precursorId1, int msLevel1, int parentScan1)
        {
            firstFrameId = firstFrameId1;
            precursorId = precursorId1;
            msLevel = msLevel1;
            parentScan = parentScan1;
        }

        bool isValid() const
        {
            return firstFrameId > 0;
        }

        static ScanRec invalid()
        {
            return ScanRec(-1, 0, 0, 0);
        }
    };

    struct MsMsInfo {
        int frameId;
        int scanNumBegin;
        int scanNumEnd;

        MsMsInfo(int frameId1, int scanNumBegin1, int scanNumEnd1)
        {
            frameId = frameId1;
            scanNumBegin = scanNumBegin1;
            scanNumEnd = scanNumEnd1;
        }
    };

public:
    MSReaderBrukerTims();
    virtual ~MSReaderBrukerTims();

    Q_DISABLE_COPY(MSReaderBrukerTims);

    bool canOpen(const QString &filename) const override;
    MSReaderClassType classTypeName() const override;
    Err openFile(const QString &filename,
                 MSConvertOption convert_options = ConvertWithCentroidButNoPeaksBlobs,
                 QSharedPointer<ProgressBarInterface> progress = NoProgress) override;

    Err closeFile() override;
    Err getBasePeak(point2dList *points) const override;
    Err getTICData(point2dList *points) const override;
    Err getScanData(long scanNumber, point2dList *points, bool do_centroiding = false,
                    PointListAsByteArrays *pointListAsByteArrays = NULL) override;
    Err getXICData(const XICWindow &xicWindow, point2dList *points, int msLevel = 1) const override;
    Err getScanInfo(long scanNumber, ScanInfo *obj) const override;
    Err getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const override;
    Err getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const override;
    Err getFragmentType(long scanNumber, long scanLevel, QString *fragType) const override;
    Err getChromatograms(QList<ChromatogramInfo> *chroList,
                         const QString &internalChannelName = QString()) override;
    Err getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const override;

    void setIonMobilityOptions(const IonMobilityOptions &opt) override;

    void setNumberRetainedPeaks(int value);

protected:
    bool initialize();
    bool isInitialized() const;

private:
    Err buildScanTable();
    Err getMs1Data(long scanNumber, point2dList *ms1Spectra);
    Err getMsMsData(long scanNumber, point2dList *msMsSpectra);
    Err getMsMsInfo(int precursorId, std::vector<MsMsInfo> *msMsInfo);
    void sumNeighbors(const point2dList &data, point2dList&);
    void filterPeaks(const point2dList &data, point2dList&);
    void filterPairs(const point2dList &data, point2dList&);
    void mergePointLists(const std::vector<point2dList> &data, point2dList&);
    void retainTopIntensityPeaks(point2dList *result);
    void processPeaks(const std::vector<point2dList> &data, point2dList&);

    Err calculateMobility(const ScanRec& scanRec, double* mobility) const;

    struct Private;
    QScopedPointer<Private> d;
};

_MSREADER_END
_PMI_END

#endif // MSREADERBRUKERTIMS_H
