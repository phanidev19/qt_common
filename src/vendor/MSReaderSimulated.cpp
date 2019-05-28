/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com , Yong Kil ykil@proteinmetrics.com
*/
#include "MSReaderSimulated.h"
#include "MzCalibration.h"
#include "PathUtils.h"
#include "PlotBase.h"
#include "ProgressBarInterface.h"
#include "QtSqlUtils.h"
#include "VendorPathChecker.h"
#include "pmi_common_ms_debug.h"

#include <PmiQtCommonConstants.h>

#include <plot_utils.h>
#include <qt_string_utils.h>
#include <qt_utils.h>

#include <CacheFileManager.h>

_PMI_BEGIN

_MSREADER_BEGIN

MSReaderSimulated::MSReaderSimulated()
{
}

MSReaderSimulated::~MSReaderSimulated()
{
}

MSReaderBase::MSReaderClassType MSReaderSimulated::classTypeName() const
{
    return MSReaderClassTypeSimulated;
}

void MSReaderSimulated::clear()
{
    MSReaderBase::clear();
}

bool MSReaderSimulated::canOpen(const QString &fileName) const
{
    if ( fileName.endsWith(".msfaux", Qt::CaseInsensitive) ) {
        return true;
    }
    return false;
}

Err MSReaderSimulated::openFile(const QString &fileName, MSConvertOption convert_options,
                                QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    clear();

    FauxSpectraReader fauxSpectra;

    /*!
    * @brief See comments in FauxSpectraReader.h for detailed instructions on usage.
    */
    e = fauxSpectra.readMSFauxParametersCSV(fileName + ".params");
    e = fauxSpectra.readMSFauxCSV(fileName); ree;

    m_scansPerSecond = fauxSpectra.parameters().scansPerSecond;

    debugMs() << "ScansPerSecond" << fauxSpectra.parameters().scansPerSecond;
    debugMs() << "MedianNoiseLevelMS1" << fauxSpectra.parameters().medianNoiseLevelMS1;
    debugMs() << "MinMz" << fauxSpectra.parameters().minMz;
    debugMs() << "MaxMz" << fauxSpectra.parameters().maxMz;
    debugMs() << "LowerWindowOffset" << fauxSpectra.parameters().lowerWindowOffset;
    debugMs() << "UpperWindowOffset" << fauxSpectra.parameters().upperWindowOffset;

    FauxScanCreator fauxScanCreator;

    e = fauxScanCreator.init(fauxSpectra, fauxSpectra.parameters()); ree;
    e = fauxScanCreator.constructMS1Scans(); ree;
    e = fauxScanCreator.constructMS2Scans(); ree;

    m_scans = fauxScanCreator.scans();
    m_msReaderSimulatedOptions.numberOfScans = fauxScanCreator.numberOfScans();
    m_msReaderSimulatedOptions.timePerScan = 
        1 / static_cast<double>(fauxSpectra.parameters().scansPerSecond);
 
    return e;
}


Err MSReaderSimulated::closeFile()
{
    Err e = kNoErr;
    MSReaderBase::clear();
    m_scans.clear();
    return e;
}

Err MSReaderSimulated::getBasePeak(point2dList *points) const
{
    Err e = kNoErr;

    for (const FauxScanCreator::Scan &scan : m_scans) {

        const point2dList  &scanPoints = scan.scanIons;

        if (scanPoints.empty()) {
            points->push_back(point2d(scan.retTimeMinutes, 0));
        }

        auto it = std::max_element(scanPoints.begin(), scanPoints.end(), [](const point2d &a, const point2d &b) {return a.y() < b.y(); });
        double basePeak = it->y();
        points->push_back(point2d(scan.retTimeMinutes, basePeak));
    }

    return e;
}

Err MSReaderSimulated::getTICData(point2dList *points) const
{

    Err e = kNoErr;

    if (points->empty()) {
        rrr(kError);
    }

    for (const FauxScanCreator::Scan &scan : m_scans) {

        const point2dList  &scanPoints = scan.scanIons;

        if (scanPoints.empty()) {
            point2d(scan.retTimeMinutes, 0);
        }

        double sum = accumulate(scanPoints.begin(), scanPoints.end()
            , 0, [](int sum, const point2d& curr) { return sum + curr.y(); });

        points->push_back(point2d(scan.retTimeMinutes, sum));
    }

    return e;
}

Err MSReaderSimulated::getScanData(long scanNumber, point2dList *points, bool do_centroiding,
                                   PointListAsByteArrays *pointListAsByteArrays)
{
    Err e = kNoErr;
    Q_UNUSED(pointListAsByteArrays);
    Q_UNUSED(do_centroiding);

    points->clear();

    if (scanNumber <= 0 || scanNumber > m_msReaderSimulatedOptions.numberOfScans) {
        rrr(kError)
    }

    //// Subtract 1 here because scanNumber is not 0 based and m_scans is.
    point2dList scanIons = m_scans[scanNumber - 1].scanIons;
    std::sort(scanIons.begin(), scanIons.end(), point2d_less_x);

    *points = std::move(scanIons);

    return e;
}

Err MSReaderSimulated::getXICData(const XICWindow &win, point2dList *xic_points, int ms_level) const
{
    Q_ASSERT(xic_points);

    Err e = kNoErr;
    Q_UNUSED(ms_level);

    if (!win.isMzRangeDefined() 
        || !win.isTimeRangeDefined()
        || m_scans.isEmpty()) {
        warningMs() << "XICWindow not set or scan vector empty";
        e = kRawReadingError; ree;
    }

    for (const FauxScanCreator::Scan &scan : m_scans) {


        if (!win.isTimeRangeDefined()
            || (win.time_start <= scan.retTimeMinutes 
            && scan.retTimeMinutes <= win.time_end))
        {
            double pointIntensity = 0;
            const point2dList &points = scan.scanIons;
            for (const point2d &point : points) {
                if (!win.isMzRangeDefined()
                    || (win.mz_start <= point.x() 
                        && point.x() <= win.mz_end)) {
                    pointIntensity += point.y();
                }
            }
            xic_points->push_back(point2d(scan.retTimeMinutes, pointIntensity));
        }
    }

    return e;
}

Err MSReaderSimulated::getScanInfo(long scanNumber, ScanInfo *obj) const
{
    Q_ASSERT(obj);

    Err e = kNoErr;

    obj->peakMode = PeakPickingCentroid;
    obj->scanMethod = ScanMethodFullScan;

    if ( scanNumber > m_scans.size()) {
        warningMs() << "Scan number exceeds size of total scans";
        rrr(kError);
    }

    obj->retTimeMinutes = m_scans[scanNumber - 1].retTimeMinutes;
    obj->nativeId = QString("fauxScanNumber=%1").arg(scanNumber);

    //// Subtract 1 here because scanNumber is not 0 based and m_scans is.
    obj->scanLevel = m_scans[scanNumber - 1].scanLevel;

    return e;
}

Err MSReaderSimulated::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Err e = kNoErr;

    if (scanNumber > m_scans.size()) {
        warningMs() << "Scan number exceeds size of total scans";
        rrr(kError);
    }

    const FauxScanCreator::Scan &scan = m_scans[scanNumber - 1];

    pinfo->dIsolationMass = scan.dIsolationMass;
    pinfo->dMonoIsoMass = scan.dMonoIsoMass;
    pinfo->nChargeState = scan.nChargeState;

    ////This is horribly named.  nScanNumber should be nPrecursorScanNumber.
    pinfo->nScanNumber = scan.precursorScanNumber;

    pinfo->nativeId = QString("fauxScanNumber=%1").arg(scan.precursorScanNumber);
    pinfo->lowerWindowOffset = scan.lowerWindowOffset;
    pinfo->upperWindowOffset = scan.upperWindowOffset;

    return e;
}

Err MSReaderSimulated::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Err e = kNoErr;

    *totalNumber = m_msReaderSimulatedOptions.numberOfScans;
    *startScan = 1;
    *endScan = m_msReaderSimulatedOptions.numberOfScans;

    return e;
}

Err MSReaderSimulated::getFragmentType(long scanNumber, long scanLevel,
                                       QString *fragmentType) const
{
    Err e = kNoErr;

    if (scanNumber > m_scans.size()) {
        rrr(kError);
    }

    fragmentType->clear();
    QString fragtype = m_scans[scanNumber - 1].fragmentationType;
    *fragmentType = fragtype;

    return e;
}

Err MSReaderSimulated::getChromatograms(QList<msreader::ChromatogramInfo> *chroInfoList,
                                        const QString &channelInternalName)
{
    Q_ASSERT(chroInfoList);
    Err e = kNoErr;
    chroInfoList->clear();

    ChromatogramInfo chroInfo;
    e = getTICData(&chroInfo.points); ree;
    chroInfo.channelName = "TIC";
    chroInfo.internalChannelName = "TIC_InternalFauxChannel";
    chroInfoList->push_back(chroInfo);

    chroInfo.clear();

    e = getBasePeak(&chroInfo.points); ree;
    chroInfo.channelName = "BPI";
    chroInfo.internalChannelName = "BPI_InternalFauxChannel";
    chroInfoList->push_back(chroInfo);

    return e;
}

Err MSReaderSimulated::getBestScanNumber(int msLevel, double scanTimeMinutes,
                                         long *scanNumber) const
{
    Err e = kNoErr;
    
    int curr_mslevel = -1;
    int curr_scanNumber = std::floor(scanTimeMinutes * m_scansPerSecond * 60);

    while (curr_scanNumber >= 1) {
        const FauxScanCreator::Scan &scan = m_scans[curr_scanNumber];
        curr_mslevel = scan.scanLevel;
        if (curr_mslevel == msLevel) {
            *scanNumber = curr_scanNumber;
            return e;
        }
        curr_scanNumber--;
    }

    //No MS level found.
    e = kBadParameterError; eee;

error:
    return e;
}

Err MSReaderSimulated::getLockmassScans(QList<ScanInfoWrapper> *list) const
{
    Err e = kNoErr;
    return e;
}

MSReaderBase::FileInfoList MSReaderSimulated::filesInfoList() const
{
    FileInfoList list;
    return list;
}

_MSREADER_END
_PMI_END
