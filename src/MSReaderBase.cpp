/*
 * Copyright (C) 2015-2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "MSReaderBase.h"

#include "pmi_common_ms_debug.h"
#include "VendorPathChecker.h"

_PMI_BEGIN

using namespace msreader;

double MSReaderBase::scantimeFindParentFudge  = 0.000000001;

ScanInfo::ScanInfo(double retTimeMinutes, int scanLevel, const QString &nativeId,
                   PeakPickingMode peakMode, ScanMethod scanMethod)
    : retTimeMinutes(retTimeMinutes)
    , scanLevel(scanLevel)
    , nativeId(nativeId)
    , peakMode(peakMode)
    , scanMethod(scanMethod)
{
}

ScanInfo::ScanInfo()
{
    clear();
}

void ScanInfo::clear()
{
    retTimeMinutes = -1;
    scanLevel = -1;
    nativeId.clear();
    peakMode = PeakPickingModeUnknown;
    scanMethod = ScanMethodUnknown;
}

Err MSReaderBase::ScanInfoList::updateCacheFully(const MSReaderBase *ms) const
{
    Err e = kNoErr;

    if (!isFullyCached()) {
        // Using this hack to do lazy update so that we don't have to make this during open
        ScanInfoList *ptr = const_cast<ScanInfoList *>(this);

        e = ptr->updateFully(ms); eee;
    }

error:
    return e;
}

void MSReaderBase::ScanInfoList::clear()
{
    m_scanNumberToInfo.clear();
    m_scanTimeToScanNumber.clear();
    m_totalNumberOfScansExpected = -1;
}

Err MSReaderBase::ScanInfoList::updateFully(const MSReaderBase *ms)
{
    Err e = kNoErr;
    long startScan = 0;
    long endScan = -1;
    long totalScans;

    clear();

    if (ms == NULL) {
        return kError;
    }

    e = ms->getNumberOfSpectra(&totalScans, &startScan, &endScan); eee;

    m_totalNumberOfScansExpected = endScan - startScan + 1;

    debugMs() << "update scans start,end=" << startScan << "," << endScan;

    for (long scan = startScan; scan <= endScan; scan++) {
        ScanInfo obj;

        e = ms->getScanInfo(scan, &obj); eee;

        m_scanNumberToInfo.insert(scan, obj);
        m_scanTimeToScanNumber[obj.scanLevel][obj.retTimeMinutes] = scan;

        if (scan % 1000 == 0) {
            debugMs() << "scan=" << scan;
        }
    }

    debugMs() << "Done updating cached scanInfo";

error:
    return e;
}

bool MSReaderBase::ScanInfoList::isFullyCached() const
{
    if (m_totalNumberOfScansExpected < 0) {
        return false;
    }

    if (m_scanNumberToInfo.size() == m_totalNumberOfScansExpected) {
        return true;
    }

    return false;
}

int MSReaderBase::ScanInfoList::scanInfoCacheCount(int *totalScansExpected) const
{
    if (totalScansExpected) {
        *totalScansExpected = m_totalNumberOfScansExpected;
    }

    return m_scanNumberToInfo.size();
}

Err MSReaderBase::ScanInfoList::getAllScansInfoAtLevel(const MSReaderBase *ms, int level,
                                                       QList<ScanInfoWrapper> *list) const
{
    Err e = kNoErr;

    list->clear();

    e = updateCacheFully(ms); ree;

    QList<long> scanNumbers = m_scanNumberToInfo.keys();
    ScanInfoWrapper obj;

    foreach (long scanNum, scanNumbers) {
        obj.scanNumber = scanNum;
        obj.scanInfo = m_scanNumberToInfo[scanNum];

        if (obj.scanInfo.scanLevel == level) {
            list->push_back(obj);
        }
    }

    return e;
}

bool MSReaderBase::ScanInfoList::scanInfoCache(long scanNumber, ScanInfo *snfo) const
{
    auto itr = m_scanNumberToInfo.find(scanNumber);

    if (itr == m_scanNumberToInfo.end()) {
        return false;
    }

    if (snfo) {
        *snfo = *itr;
    }

    return true;
}

void MSReaderBase::ScanInfoList::setScanInfoCache(long scanNumber, const ScanInfo &obj)
{
    m_scanNumberToInfo[scanNumber] = obj;
}

Err MSReaderBase::ScanInfoList::setScanInfoCache(const QMap<long, ScanInfo> &scanInfoCacheFull)
{
    m_totalNumberOfScansExpected = scanInfoCacheFull.size();
    m_scanNumberToInfo = scanInfoCacheFull;

    m_scanTimeToScanNumber.clear();

    QList<long> scanNumberList = scanInfoCacheFull.keys();

    for (long scanNumber : scanNumberList) {
        const ScanInfo &obj = scanInfoCacheFull[scanNumber];
        m_scanTimeToScanNumber[obj.scanLevel][obj.retTimeMinutes] = scanNumber;
    }

    return kNoErr;
}

/*
Err ScanInfoList::getScanTimeFromScanNumber(long scan, double* scanTime) const
{
    Err e = kNoErr;

    const_iter iter = m_scanNumberToInfo.find(scan);

    if (iter == m_scanNumberToInfo.end()) {
        warningMs() << "Could not find scan number =" << scan;

        e = kBadParameterError; eee;
    }

    *scanTime = iter->retTimeMinutes;

error:
    return e;
}
*/

Err MSReaderBase::ScanInfoList::getBestScanNumberFromScanTime(const MSReaderBase* ms,
    int scanLevel, double scanTime, long* scanNumber) const
{
    Err e = kNoErr;

    e = updateCacheFully(ms); ree;

    QMap<double,long>::const_iterator iter;
    double best_diff = 1e10;
    double startTime = -1;
    double endTime = -1;

    if (m_scanTimeToScanNumber.size() <= 0) {
        debugMs() << "No scan numbers are exists.";

        e = kBadParameterError; ree;
    }

    if (!m_scanTimeToScanNumber.contains(scanLevel)) {
        debugMs() << "No scan level found scanLevel=" << scanLevel;

        e = kBadParameterError; ree;
    }

    const ScanTimeScanNumberMap &scanTimeToScanNumber = m_scanTimeToScanNumber[scanLevel];

    startTime = scanTimeToScanNumber.firstKey();
    endTime = scanTimeToScanNumber.lastKey();

    if (scanTime < startTime) {
        *scanNumber = scanTimeToScanNumber.first();

        return e;
    } else if (scanTime > endTime) {
        *scanNumber = scanTimeToScanNumber.last();

        return e;
    }

    //Check both upper and lower to make the code easier to read and boundary cases
    iter = scanTimeToScanNumber.lowerBound(scanTime);

    if (iter == scanTimeToScanNumber.end()) {
        iter = scanTimeToScanNumber.upperBound(scanTime);
    }

    if (iter == scanTimeToScanNumber.end()) {
        debugMs() << "Given scanTime cannot be found, "
                     "startTime,endTime,scanTime,m_scanTimeToScanNumber.size()="
                  << startTime << "," << endTime << "," << scanTime << ","
                  << scanTimeToScanNumber.size();

        e = kBadParameterError; eee;
    }

    best_diff = std::abs(iter.key() - scanTime);

    if (1) {
        //Check current iterator and before and after.
        QMap<double,long>::const_iterator iter_before = iter;

        if (iter_before != scanTimeToScanNumber.begin()) {
            --iter_before;

            double diff = std::abs(iter_before.key() - scanTime);

            if (diff < best_diff) {
                iter = iter_before;
                best_diff = diff;
            }

        }

        QMap<double, long>::const_iterator iter_after = iter;

        if (iter_after != scanTimeToScanNumber.end()) {
            ++iter_after;

            if (iter_after != scanTimeToScanNumber.end()) {
                double diff = std::abs(iter_after.key() - scanTime);

                if (diff < best_diff) {
                    iter = iter_after;
                    best_diff = diff;
                }
            }
        }
    }

    *scanNumber = iter.value();

    if (best_diff > 1) {
        debugMs() << "Note, scan time diff from expected is greater than expected.  "
                     "best_diff,scanTime,best_scan_time,best_scannumber"
                  << best_diff << "," << scanTime << ","
                  << "," << iter.key() << "," << *scanNumber;
    }

error:
    return e;
}

int MSReaderBase::ScanInfoList::numberOfScansAtLevel(const MSReaderBase *ms, int level) const
{
    Err e = kNoErr;

    e = updateCacheFully(ms);

    if (e == kNoErr) {
        if (m_scanTimeToScanNumber.contains(level)) {
            return m_scanTimeToScanNumber[level].size();
        }

        return 0;
    }

    return -1;
}

MSReaderBase::~MSReaderBase()
{
}

Err MSReaderBase::cacheScanNumbers(const QList<int> &scanNumberList, QSharedPointer<ProgressBarInterface> progress)
{
    Q_UNUSED(scanNumberList);
    Q_UNUSED(progress);

    return kNoErr;
}

Err MSReaderBase::getBestScanNumberFromScanTime(int msLevel, double scanTime,
                                                long *scanNumber) const
{
    Err e = kNoErr;

    e = m_scanInfoList.getBestScanNumberFromScanTime(this, msLevel, scanTime, scanNumber); eee;

error:
    return e;
}

QString MSReaderBase::getFilename() const
{
    return m_filename;
}

void MSReaderBase::setFilename(const QString &filename)
{
    m_scanInfoList.clear();

    m_filename = filename;
}

Err MSReaderBase::getLockmassScans(QList<ScanInfoWrapper> *lockmassList) const
{
    Err e = kNoErr;

    e = m_scanInfoList.getAllScansInfoAtLevel(this, 1, lockmassList); eee;

error:
    return e;
}

Err MSReaderBase::getScanInfoListAtLevel(int level, QList<ScanInfoWrapper> *lockmassList) const
{
    return m_scanInfoList.getAllScansInfoAtLevel(this, level, lockmassList);
}

bool MSReaderBase::scanInfoCache(long scanNumber, ScanInfo *obj) const
{
    return m_scanInfoList.scanInfoCache(scanNumber, obj);
}

void MSReaderBase::setScanInfoCache(long scanNumber, const ScanInfo &obj) const
{
    m_scanInfoList.setScanInfoCache(scanNumber, obj);
}

int MSReaderBase::scanInfoCacheCount(int *totalScansExpected) const
{
    return m_scanInfoList.scanInfoCacheCount(totalScansExpected);
}

void MSReaderBase::clearScanInfoList()
{
    m_scanInfoList.clear();
}

Err MSReaderBase::setScanInfoCache(const QMap<long, ScanInfo> &scanInfoCacheFull)
{
    return m_scanInfoList.setScanInfoCache(scanInfoCacheFull);
}

int MSReaderBase::numberOfScansAtLevel(int level) const
{
    return m_scanInfoList.numberOfScansAtLevel(this, level);
}

MSReaderBase::FileInfoList MSReaderBase::filesInfoList() const
{
    return FileInfoList();
}

void MSReaderBase::clear()
{
    m_filename.clear();

    m_scanInfoList.clear();
}

CentroidOptions::CentroidMethod MSReaderBase::getPreferedCentroidSource(const QString &filename)
{
    if (VendorPathChecker::isWatersFile(filename) || VendorPathChecker::isABSCIEXFile(filename)) {
        return CentroidOptions::CentroidMethod_UseCustom;
    }
    return CentroidOptions::CentroidMethod_PreferVendor;
}

CentroidOptions::CentroidMethod
MSReaderBase::getCentroidMethod_Vendor_Or_Custom(const CentroidOptions &options,
                                                 const QString &filename)
{
    if (options.getCentroidMethod() == CentroidOptions::CentroidMethod_Auto) {
        return getPreferedCentroidSource(filename);
    }
    return options.getCentroidMethod();
}

_PMI_END
