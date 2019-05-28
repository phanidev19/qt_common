/*
 * Copyright (C) 2012-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "MSReader.h"

#include <algorithm>

#include <AdvancedSettings.h>
#include <CacheFileManager.h>
#include <PlotBaseUtils.h>
#include <qt_string_utils.h>
#include "VendorPathChecker.h"

#ifdef PMI_MS_ENABLE_CENTROID_CACHE
    #include <MSCentroidCacheManager.h>
#endif

#include "CacheFileCreatorThread.h"
#include "MS1PrefixSum.h"
#include "MSDataNonUniformAdapter.h"
#include "pmi_common_ms_debug.h"
#include "ProgressBarInterface.h"
#include "vendor/MSReaderABI.h"
#include "vendor/MSReaderAgilent.h"
#ifdef PMI_MS_ENABLE_BRUKER_API
#include "vendor/MSReaderBrukerTims.h"
#include "vendor/MSReaderBruker.h"
#endif
#include "vendor/MSReaderByspec.h"
#include "vendor/MSReaderThermo.h"
#ifdef PMI_MS_ENABLE_SHIMADZU_API
    #include "vendor/MSReaderShimadzu.h"
#endif
#ifdef PMI_MS_ENABLE_BRUKER_API
    #include "vendor/MSReaderBruker.h"
#endif
#ifdef PMI_QT_COMMON_BUILD_TESTING
    #include "MSReaderTesting.h"
#endif
#include "MSReaderSimulated.h"

#include <AdvancedSettings.h>
#include <CacheFileManager.h>
#include <PlotBaseUtils.h>
#include <PmiQtCommonConstants.h>

#include <qt_string_utils.h>

#include <algorithm>

_PMI_BEGIN

using namespace msreader;

MSReader *MSReader::s_instance = nullptr;

MSReader *MSReader::Instance()
{
    // static MSReader msreader_static_obj;

    if (s_instance == nullptr) {
        s_instance = new MSReader;
    }

    return s_instance;
}

void MSReader::releaseInstance()
{
    if (s_instance) {
        s_instance->closeAllFileConnections();

        delete s_instance;
        s_instance = nullptr;
    }
}

MSReader *MSReader::reallocateInstance(bool preserveCalibration)
{
    QHash<QString, MzCalibration> fileCaliInfo;
    MSReader *ms = MSReader::Instance();
    if (preserveCalibration && ms) {
        fileCaliInfo = ms->m_caliManager.fileToCalibration();
    }

    MSReader::releaseInstance();

    ms = MSReader::Instance();
    if (preserveCalibration) {
        QStringList keys = fileCaliInfo.keys();
        for (const QString &key : keys) {
            ms->m_caliManager.add(key, fileCaliInfo[key]);
        }
    }
    return ms;
}

static int calculateIdealThreadCount(const QStringList &files, int threadCount)
{
    return (std::min)((files.size()),
                      (threadCount > 0 ? threadCount : QThread::idealThreadCount()));
}

static void balanceLoad(const QStringList &files, const QVector<CacheFileCreatorThreadPtr> &threads)
{
    int fileIdx = 0;

    for (const auto &file : files) {
        // TODO(Ivan Skiba 2017-05-22): improve load balancing
        threads[fileIdx++ % threads.count()]->addFile(file);
    }
}

Err MSReader::constructCacheFiles(const QStringList &files, int threadCount)
{
    for (const auto &file : files) {
        if (!QFile::exists(file)) {
            warningMs() << "File does not exist: " << file;

            rrr(kFileOpenError);
        }
    }

    QVector<CacheFileCreatorThreadPtr> threads(calculateIdealThreadCount(files, threadCount));

    for (auto &thread : threads) {
        thread = CacheFileCreatorThreadPtr(new CacheFileCreatorThread());
    }

    balanceLoad(files, threads);

    for (const auto &thread : threads) {
        thread->start();
    }

    for (const auto &thread : threads) {
        thread->wait();
    }

    for (const auto &thread : threads) {
        if (thread->isFail()) {
            rrr(kError);
        }
    }

    return kNoErr;
}

// TODO move to some utils?
static bool isEnvVariableEnabled(const QLatin1String &name)
{
    if (!qEnvironmentVariableIsSet(name.data())) {
        return false;
    }

    const bool enabled = qgetenv(name.data()).toInt() != 0;
    return enabled;
}

MSReader::MSReader()
    : m_cacheFileManager(new CacheFileManager())
    , m_caliManager(*m_cacheFileManager.data())
    , m_xicMode(XICModeVendor)
#ifdef PMI_MS_ENABLE_CENTROID_CACHE
    , m_centroidCacheManager(new MSCentroidCacheManager)
#endif
{
    if (!s_instance) {
        s_instance = this;
    }

#ifdef PMI_QT_COMMON_BUILD_TESTING
    m_vendorList.push_back(QSharedPointer<MSReaderBase>(new pmi::MSReaderTesting()));
#endif

    m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderThermo()));
    {
#ifdef PMI_MS_ENABLE_BRUKER_API
        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderBrukerTims())); //<=== DK; must be before regular Bruker reader
        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderBruker()));
#endif

#ifdef PMI_MS_ENABLE_ABI_DISK_TO_MEMORY
        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderAbi()));
#endif

#ifdef PMI_MS_ENABLE_SHIMADZU_API
        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderShimadzu()));
#endif

#ifdef PMI_MS_ENABLE_AGILENT_API
        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderAgilent()));
#else
#error "This will fail the compilation with my custom error"
#endif
        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderSimulated()));

        m_vendorList.push_back(QSharedPointer<MSReaderBase>(new msreader::MSReaderByspec(*m_cacheFileManager.data())));
    }

    // By default, use MSReaderBypsec instead of direct MSReaderBrukerTims.
    // This is because the BrukerTims route is too slow today. So, we want to cache it in byspec2
    // first Note that MakeByspec2PmiCommand::execute must remove this list to avoid recurive calls.
    // See comment in that function.
    m_vendorsRoutedThroughByspec.insert(MSReaderBase::MSReaderClassTypeBrukerTims);

    QVariant xicMode = as::value("MSReader/XicMode", QVariant());
    if (xicMode.isNull()) {
        xicMode = as::value("MSReader/XICMode", QVariant());
    }

    if (!xicMode.isNull()) {
        const QString xicStr = xicMode.toString().toLower();

        if (xicStr == "vendor") {
            m_xicMode = XICModeVendor;
        } else if (xicStr == "manual") {
            m_xicMode = XICModeManual;
        } else if (xicStr == "tiled") {
            m_xicMode = XICModeTiledCache;
        }

        debugMs() << "Current XIC mode" << m_xicMode;
    }
}

MSReader::~MSReader()
{
    // Note: due to this getting destoried after the Qt UI gets destoryed, we get crash outputing
    // anything to commonMsDebug(), stderr, stdout. Need to fix this later. ML-284
    closeAllFileConnections();

#ifdef PMI_MS_ENABLE_CENTROID_CACHE
    delete m_centroidCacheManager;
#endif

    if (s_instance == this) {
        s_instance = nullptr;
    }
}

MSReaderBase::MSReaderClassType MSReader::classTypeName() const
{
    if (m_openReader && m_openReader->getFilename().size() > 0) {
        return m_openReader->classTypeName();
    }

    return MSReaderBase::MSReaderClassTypeUnknown;
}

Err MSReader::openFile(const QString &_fileName, msreader::MSConvertOption convert_options,
                       QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    const QString fileName(QDir::toNativeSeparators(QFileInfo(_fileName).canonicalFilePath()));

    m_cacheFileManager->setSourcePath(fileName);

    // Try current reader
    if (m_openReader) {
        // Already open, don't re-open.
        if (m_openReader->getFilename() == fileName) {
            _handlePassingCentroidToMSReaderByspec();

            return e;
        }
    }

    // Find existing reader from list
    for (int i = 0; i < m_openedReaderList.size(); i++) {
        if (m_openedReaderList[i]->getFilename() == fileName) {
            m_openReader = m_openedReaderList[i];
            _handlePassingCentroidToMSReaderByspec();
            return e;
        }
    }

    //Before we try and open a new one, let's make sure we don't have too many open files.
    //This is to make sure any potential bugs from increasing memory size.
    //Note: opening more than 12 Thermo connection appears to cause problems.
    //So we will only open 6 for now.
    static int openMaxCountDefault = 6;
    static int openMaxCountABI = 1;

    // Note(Yong,2016-02-16): We are observing that ABI files consume a lot of memory. So, it's
    // best to close them to free memory. In one test case in Byologic creation, we've reduced from
    // 1.2GB to 0.6GB using this stradegy. Tests done so far: But opening itself doesn't appear to be
    // the issue. MSReaderBase does has internal cache that consumes memory, but that shouldn't be
    // the only source of issue. Still uncertain where all the memory is going.
    // In Debug mode, we've observed that TableEntry.exec() was consuming memory; see
    // extractRAWScans()
    int openMaxCount = openMaxCountDefault;

    if (VendorPathChecker::isABSCIEXFile(fileName)) {
        openMaxCount = openMaxCountABI;
    }

    if (m_openedReaderList.size() > openMaxCount) {
        warningMs() << "There are too many file connections made.  We will close them all now. "
                       "m_openedReaderList.size()="
                    << m_openedReaderList.size();

        closeAllFileConnections();
    }

    e = _makeReader(fileName); eee;

    m_openReader->setIonMobilityOptions(ionMobilityOptions());
    _handlePassingCentroidToMSReaderByspec();

    e = m_openReader->openFile(fileName, convert_options, progress);

    if (e != kNoErr) {
        // Thermo can only open so many before crashing. So, clear out before attempting again.
        warningMs() << "Error opening file.  Close them all and re-try. m_openedReaderList.size()="
                    << m_openedReaderList.size();

        closeAllFileConnections();

        e = _makeReader(fileName); eee;

        m_openReader->setIonMobilityOptions(ionMobilityOptions());
        _handlePassingCentroidToMSReaderByspec();

        e = m_openReader->openFile(fileName, convert_options, progress); eee;
    }

    m_openReader->setFilename(fileName);

error:
    if (e != kNoErr) {
        debugMs() << "Error during opening with:";

        if (m_openReader) {
            debugMs() << "m_openedReaderList.size()=" << m_openedReaderList.size();
        }
    }

    if (e != kNoErr) {
        if (m_openReader) {
            m_openedReaderList.removeLast();
            m_openReader.clear();
        }
    }

    return e;
}

Err MSReader::closeFile()
{
    Err e = kNoErr;

    // closeAllFileConnections();
    /// Do lazy close during open or during destructor

    // Except, we will close all byspec/pwiz related ones.
    // This is to avoid complication with how we open them in different modes.
    // If that were openned in chronos mode, we need to close and re-open with full mode.
    _closeAllByspecConnections();

    // Check to see if ScanInfo memory can be freed.
    int scanInfoListCacheCount = 0;

    for (int i = 0; i < m_openedReaderList.size(); i++) {
        int scansCached = m_openedReaderList[i]->scanInfoCacheCount(nullptr);

        if (scansCached > 0) {
            scanInfoListCacheCount++;
        }

        if (scanInfoListCacheCount > m_scanInfoListCacheCountMax) {
            m_openedReaderList[i]->clearScanInfoList();
        }
    }

    m_cacheFileManager->resetSourcePath();

    return e;
}

bool MSReader::canOpen(const QString &filename) const
{
    Q_UNUSED(filename);
    return false;
}


Err MSReader::getBasePeak(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;

    if (m_openReader) {
        e = m_openReader->getBasePeak(points);

        if (e == kFunctionNotImplemented || points->size() <= 0) {
            e = _getBasePeakManual(points);
        }

    } else {
        e = kError;
    }

    ree;
    return e;
}

Err MSReader::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;

    if (m_openReader) {
        e = m_openReader->getTICData(points);

        if (e == kFunctionNotImplemented || points->size() <= 0) {
            e = _getTICManual(points);
        }

    } else {
        e = kError;
    }

    ree;
    return e;
}

namespace
{

Err InitUsingPointList_NoCompression(const point2dList *points, msreader::PointListAsByteArrays &pa)
{
    Q_ASSERT(points);

    Err e = kNoErr;

    pa.clear();

    std::vector<double> xlist;
    std::vector<float> ylist;

    convertPlotToDoubleFloatArray(*points, xlist, ylist);

    //make deep copy.
    pa.dataX = QByteArray((const char*)xlist.data(), static_cast<int>(xlist.size() * sizeof(double)));
    pa.dataY = QByteArray((const char*)ylist.data(), static_cast<int>(ylist.size() * sizeof(float)));

    // error:
    return e;
}
}

static bool isCustomCentroidingPreferred(const pmi::MSReaderBase *reader,
                                         const CentroidOptions &centroidOption, long scanNumber,
    msreader::PeakPickingMode peakPickingMode)
{
    Q_ASSERT(reader);

    Err e = kNoErr;

    // If custom centroiding, see if we need to do it first.
    // Note: Custom centroiding is too slow.
    //      The current implementation has centroided MS1 Peaks_MS1CentroidOnly table in byspec.
    //      So, no need to do this ourselves when handling byspec2 files.
    bool peformCustomCentroiding = peakPickingMode == PeakPickingProfile
        && MSReaderBase::getCentroidMethod_Vendor_Or_Custom(centroidOption, reader->getFilename())
            == CentroidOptions::CentroidMethod_UseCustom;

    // A special case for Agilent reader: do custom centroiding if Agilent file constains
    // profile data only.
    if (!peformCustomCentroiding
        && reader->classTypeName() == MSReaderBase::MSReaderClassTypeAgilent) {

        const auto agilentReader = dynamic_cast<const msreader::MSReaderAgilent *>(reader);
        if (agilentReader != nullptr) {
            e = agilentReader->isCustomCentroidingPreferred(scanNumber, &peformCustomCentroiding); ree;
        }
    }

    return peformCustomCentroiding;
}

Err MSReader::getScanData(long scanNumber, point2dList *points, bool do_centroiding,
    msreader::PointListAsByteArrays *pointListAsByteArrays)
{
    Q_ASSERT(points);

    Err e = kNoErr;

    if (m_openReader) {
        ScanInfo scanInfo;
        e = getScanInfo(scanNumber, &scanInfo); eee;

        const bool noCentroiding = !do_centroiding
            || m_openReader->classTypeName() == MSReaderBase::MSReaderClassTypeByspec;

        const bool peformCustomCentroiding = noCentroiding
            ? false
            : isCustomCentroidingPreferred(m_openReader.data(), m_centroidOption, scanNumber,
                                           scanInfo.peakMode);
        if (peformCustomCentroiding) {
            PlotBase plot;

            e = m_openReader->getScanData(scanNumber, &plot.getPointList(), false,
                                          pointListAsByteArrays); eee;

            e = gaussianSmooth(plot, m_centroidOption.getSmoothingWidth()); eee;

            plot.makeCentroidedPoints(points);

            // TODO: If there's compressed information, we currently do not calibrate them
            // correctly.  So, we are currently placing with InitUsingPointList_NoCompression below.
            // Later, we will  dynamically handle blobs by placing Ids.
            if (pointListAsByteArrays) {
                pointListAsByteArrays->clear();
            }
        } else {
            e = m_openReader->getScanData(scanNumber, points, do_centroiding,
                                          pointListAsByteArrays); eee;
        }
#ifdef PMI_MS_CHECK_MS_READER_SCAN_DATA
        // Keep the test quick.  Don't try and recover from a bad scan.
        QString errMsg;

        for (unsigned int i = 0; i < points->size(); i++) {
            // Note: using .at() is slower due to assert call.
            if (std::isnan((*points)[i].x()) || std::isnan((*points)[i].y())) {
                errMsg = QString("infinite point at i = %1").arg(i);

                break;
            }

            if (i > 0 && ((*points)[i].x() < (*points)[i - 1].x())) {
                errMsg = QString("order flipped at i = %1").arg(i);

                break;
            }
        }
        if (errMsg.size() > 0) {
            warningMs() << (QString("Scan number: %1 has issue: %2.\nTotal number of points: %3.")
                                .arg(scanNumber)
                                .arg(errMsg)
                                .arg(points->size()));

            points->clear(); // ignore the whole scan
        }
#endif // PMI_MS_CHECK_MS_READER_SCAN_DATA

        bool found = false;
        auto iter = m_caliManager.get(m_openReader->getFilename(), &found);

        if (found) {
            iter->calibrate(scanInfo.retTimeMinutes, *points);
        }

        if (pointListAsByteArrays) {
            if (pointListAsByteArrays->dataX.size() <= 0 && pointListAsByteArrays->dataY.size() <= 0
                && pointListAsByteArrays->compressionInfoId.isNull()) {
                DEBUG_WARNING_LIMIT(
                    debugMs() << "Using uncompressed blob format on scanNumber,do_centroiding"
                              << scanNumber << "," << do_centroiding,
                    50);

                e = InitUsingPointList_NoCompression(points, *pointListAsByteArrays); eee;
            }
        }
    } else {
        e = kError; eee;
    }
error:
    return e;
}

Err MSReader::getXICData(const XICWindow &win, point2dList *points, int msLevel) const
{
    Q_ASSERT(points);
    Err e = kNoErr;

    if (!m_openReader.isNull()) {
        if (m_xicMode == XICModeVendor) {
            // let us inside block for NonUniform and ManualXIC
            e = m_openReader->getXICData(win, points, msLevel);
        } else {
            e = kFunctionNotImplemented;
        }

        // fall-back to manual
        if (e == kFunctionNotImplemented) {
            if (m_xicMode == XICModeTiledCache) {
                // const_cast hack to allow getXICDataCached lazy-initialize the database on first
                // call
                MSReader *msConstless = const_cast<MSReader *>(this);

                e = msConstless->getXICDataCached(win, points, msLevel); eee;

// for testing purposes to verify NonUniformCache returns the same data as manual
//#define VERIFY_XIC_WINDOW_WITH_MANUAL
#ifdef VERIFY_XIC_WINDOW_WITH_MANUAL
                point2dList expected;

                e = _getXICManual(win, expected, msLevel); eee;

                if (points != expected) {
                    // m_fileName_windowsRequested[m_openReader->getFilename()].push_back(win);
                    dumpXicData(m_openReader->getFilename(), points, expected);
                }
#endif

                return e;
            } else {
                e = _getXICManual(win, points, msLevel); eee;
            }
        } else {
            eee;
        }
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReader::setScanInfoCache(long scanNumber, const msreader::ScanInfo &obj)
{
    if (!m_openReader) {
        rrr(kError);
    }

    m_openReader->setScanInfoCache(scanNumber, obj);
    return kNoErr;
}

Err MSReader::getScanInfo(long scanNumber, msreader::ScanInfo *obj) const
{
    Q_ASSERT(obj);

    Err e = kNoErr;

    if (m_openReader) {
        const bool useCache = m_openReader->classTypeName() != MSReaderBase::MSReaderClassTypeByspec;

        // Note: we don't need to cache scanInfo for byspec as it's fast enough.
        // And we close byspec in ::closeFile(), so re-caching can be slow.
        if (useCache) {
            if (m_useScanInfoCache) {
                const bool found = m_openReader->scanInfoCache(scanNumber, obj);

                if (found) {
                    return e;
                }
            }
        }

        e = m_openReader->getScanInfo(scanNumber, obj); eee;

        // Note: potential future work.  Hide the fact that we are consiering ms level 2 as 1 from
        // caller/application.  Curren solution makes the caller responsible for finding the proper ms
        // level (e.g. call for level 2 for certain intact files).  Alternate solution is to hide the
        // ms level completely and return as ms level 1 (even though vendor calls it 1).  There's
        // trade off with this as the application cannot not have this control.
        //
        // Make the scan level appear as 1 when it's infact 2. This makes downstream work properly.
        // Specifically, getPeakInfo(QSqlDatabase & db, qlonglong peaksId, PeaksIdScanInfo & psinfo)
        // requires knowledge of ms level.  This path makes it possible for downstream to assume
        // it's level 1.
        /*
        int msLevel = bestMSLevelOne();
        if (msLevel == 2 && obj.scanLevel == 2) {
            obj.scanLevel = 1;
        }
        //The problem with this solution is that we made the caller responsible for changing ms
        level from 1 to 2 (via bestMSLevelOne())
        */
        if (useCache) {
            m_openReader->setScanInfoCache(scanNumber, *obj);
        }
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReader::getScanPrecursorInfo(long scanNumber, msreader::PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    Err e = kNoErr;

    if (m_openReader) {
        e = m_openReader->getScanPrecursorInfo(scanNumber, pinfo); eee;
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReader::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Err e = kNoErr;

    if (m_openReader) {
        e = m_openReader->getNumberOfSpectra(totalNumber, startScan, endScan); eee;
    } else {
        e = kError; eee;
    }

    // The start/end scan is used in a for-loop. To make sure it doesn't go in when there aren't any
    // items. E.g. startScan=0, endScan=0 would enter for-loop (scan=startScan; scan<=endScan;
    // scan++)
    if (*totalNumber == 0) {
        *startScan = 0;
        *endScan = -1;
    }

error:
    return e;
}

Err MSReader::getFragmentType(long scanNumber, long scanLevel, QString *fragmentType) const
{
    Q_ASSERT(fragmentType);

    if (m_openReader == nullptr) {
        rrr(kError);
    }

    // scanLevel is 2 or higher.For scanLevel = 1 it returns kNoErr and puts empty string to fragType
    if (scanLevel < 2) {
        *fragmentType = QString();
        rrr(kNoErr);
    }

    Err e = m_openReader->getFragmentType(scanNumber, scanLevel, fragmentType); ree;

    return e;
}

Err MSReader::getChromatograms(QList<msreader::ChromatogramInfo> *chroInfoList,
                               const QString &channelInternalName)
{
    Q_ASSERT(chroInfoList);

    Err e = kNoErr;

    if (m_openReader) {
        e = m_openReader->getChromatograms(chroInfoList, channelInternalName); eee;
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReader::getBestScanNumber(int msLevel, double scanTime, long *scanNumber) const
{
    Q_ASSERT(scanNumber);

    Err e = kNoErr;

    if (m_openReader) {
        e = m_openReader->getBestScanNumber(msLevel, scanTime, scanNumber);
        // For Thermo data (other vendors have not been tested), requesting for scanTime that is out
        // of bounds (on the right side) will return error.  We will capture this and let the
        // _getBestScanNumber handle this.

        // Note: Later on, consider check for scanNumber of 0, which can occur with WIFF files when
        // scanTime is zero; That is, *scanNumber == 0
        if (e == kFunctionNotImplemented || e == kRawReadingError || e == kBadParameterError) {
            e = _getBestScanNumber(msLevel, scanTime, scanNumber); eee;
        } else {
            eee;
        }
    } else {
        rrr(kError);
    }

error:
    return e;
}

#ifdef PMI_QT_COMMON_BUILD_TESTING

Err MSReader::getBestScanNumberFromScanTime(int msLevel, double scanTime, long *scanNumber) const
{
    if (m_openReader) {
        return m_openReader->getBestScanNumberFromScanTime(msLevel, scanTime, scanNumber);
    }

    debugMs() << "No readers are opened.";

    return kBadParameterError;
}

#endif

QString MSReader::getFilename() const
{
    if (m_openReader) {
        return m_openReader->getFilename();
    }

    return QString(); // don't return MSReader::MSReaderBase::m_filename, which should not be used.
}


Err MSReader::getLockmassScans(QList<ScanInfoWrapper> *lockmassList) const
{
    Q_UNUSED(lockmassList);
    return kFunctionNotImplemented;
}

Err MSReader::getScanInfoListAtLevel(int level,
                                     QList<ScanInfoWrapper> *lockmassList) const
{
    Err e = kNoErr;

    lockmassList->clear();

    if (m_openReader) {
        e = m_openReader->getScanInfoListAtLevel(level, lockmassList); ree;
    }

    return e;
}

Err MSReader::computeLockMass(int filesId, const MzCalibrationOptions &option)
{
    Err e = kNoErr;
    MzCalibration obj(*m_cacheFileManager.data());
    QList<ScanInfoWrapper> lockmassList;

    if (m_openReader) {
        const auto massList = option.lockMassList();

        if (massList.size() > 0) {
            e = m_openReader->getLockmassScans(&lockmassList); eee;

            obj.setup(option);

#ifdef PMI_MS_MZCALIBRATION_USE_MSREADER_TO_READ
            e = obj.computeCoefficientsFromScanList(this, lockmassList); eee;
#endif

            obj.setFilesId_callAfterComputing(filesId);
            m_caliManager.add(m_openReader->getFilename(), obj);
            // if (result) *result = obj;
        }
    } else {
        e = kError; eee;
    }

error:
    return e;
}

Err MSReader::saveCalibrationToDatabase(QSqlDatabase &db) const
{
    Err e = kNoErr;

    e = m_caliManager.saveToDatabase(db); eee;

error:
    return e;
}

Err MSReader::saveCalibrationToDatabase(sqlite3 *db) const
{
    Err e = kNoErr;

    e = m_caliManager.saveToDatabase(db); eee;

error:
    return e;
}

Err MSReader::loadCalibrationAndCentroidOptionsFromDatabase(const QString &filename)
{
    Err e = kNoErr;

    e = m_caliManager.loadFromDatabase(filename); eee;

    e = _loadCentroidOptionsFromDatabase(filename); eee;

error:
    return e;
}

void MSReader::clearCalibrationCentroidOptions()
{
    m_caliManager.clear();
    m_centroidOption.initWithDefault();
}

void MSReader::closeAllFileConnections()
{
    for (int i = 0; i < m_openedReaderList.size(); i++) {
        m_openedReaderList[i]->closeFile();
    }

    m_openedReaderList.clear();

    if (m_openReader) {
        m_openReader.clear();
    }

    m_fileName_ms1PrefixSumPtr.clear();
#ifdef PMI_MS_ENABLE_CENTROID_CACHE
    m_centroidCacheManager->clear();
#endif
}

Err MSReader::getTimeDomain(double *startTime, double *endTime) const
{
    Err e = kNoErr;

    *startTime = -1;
    *endTime = -1;

    if (!m_openReader) {
        e = kError; ree;
    }

    long totalNumber = 0;
    long startScan = -1;
    long endScan = -1;
    // const int ms_level = 1;
    ScanInfo info;

    e = m_openReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan); eee;

    // Note(Yong,2016-02-13): Instead of calling MSReader::getScanInfo, we are directly calling the
    // child's ::getScanInfo.  This is because calling MSReader::getScanInfo currently triggers
    // caching all getScanInfo, which slows down  this function call; we just want two scan info.
    // This is called in Byomap NewPepmapProjectTreeTable::_getRowNodesForRAW.
    e = m_openReader->getScanInfo(startScan, &info); eee;

    *startTime = info.retTimeMinutes;

    e = m_openReader->getScanInfo(endScan, &info); eee;

    *endTime = info.retTimeMinutes;

error:
    return e;
}

void MSReader::setCentroidOptions(const CentroidOptions &centroidOptions)
{
    m_centroidOption = centroidOptions;
}

bool MSReader::isOpen() const
{
    if (!m_openReader) {
        return false;
    }

    if (m_openReader->getFilename().size() > 0) {
        return true;
    }

    return false;
}

namespace
{

//! @todo Move TEST_PREFIX_SUMMING to common_core autotests as it depends on PlotBase
#ifdef TEST_PREFIX_SUMMING
// Note: Failed when used fudge value 1e-7
Err test2()
{
    Err e = kNoErr;
    GridUniform gridA;
    GridUniform gridB;
    PlotBase outPlota;
    PlotBase outPlotb;
    auto ms = MSReader::Instance();
    /*    point2dList points;*/
    bool isSimilar = true;
    const double overallEndTime = 300; // minutes  //overallStartTime = -300,
    const int max_iteration = 30;

    const QString file = "V:\\!data\\intact\\IntactAntibody1.raw";

    e = ms->openFile(file); eee;

    for (int i = 0; i < max_iteration; i++) {
        double startTime = 0.2 * overallEndTime * (rand() % 1000 / 1000.0);
        double endTime = 0.2 * overallEndTime * (rand() % 1000 / 1000.0);

        if (endTime < startTime) {
            std::swap(endTime, startTime);
        }

        e = ms->_getScanDataMS1SumPrefix(startTime, endTime, &gridA); nee;

        e = ms->_getScanDataMS1SumManualScanTime(
            startTime, endTime, GridUniform::ArithmeticType::ArithmeticType_Add, &gridB); ree;

        gridA.makePlot(outPlota);
        gridB.makePlot(outPlotb);

        if (!isSimilar_pointbypoint(outPlota, outPlotb, 1e-6)) {
            isSimilar = false;

            break;
        }
    }

    e = ms->closeFile(); eee;

    debugMs() << "isSimilar " << isSimilar;

error:
    return e;
}
#endif
}

Err MSReader::getScanDataMS1Sum(double startTime, double endTime, GridUniform *outGrid,
                                QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

#ifdef TEST_PREFIX_SUMMING
    static int count = 0;

    if (count <= 0) {
        count++;
        test2();
    }
#endif

    if (startTime > endTime) {
        debugMs() << "time swapped startTime,endTime =" << startTime << "," << endTime;

        std::swap(startTime, endTime);
    }

    int msLevel = bestMSLevelOne();

    e = _getScanDataMS1SumPrefix(msLevel, startTime, endTime, outGrid, progress); nee;

    if (e != kNoErr) {
        debugMs() << "Running slow MS1 summing mode";

        e = _getScanDataMS1SumManualScanTime(
            msLevel, startTime, endTime, GridUniform::ArithmeticType::ArithmeticType_Add, outGrid); ree;
    }

    return e;
}

Err MSReader::createPrefixSumCache(QSharedPointer<ProgressBarInterface> progress)
{
    return getPrefixSumCache(nullptr, progress);
}

Err MSReader::getDomainInterval_sampleContent(int msLevel, double *startMz, double *endMz,
                                              int max_number_of_scans_to_check) const
{
    Err e = kNoErr;
    QList<ScanInfoWrapper> scanList;
    int scan_increment = 1;
    point2dList points;
    int first_time = 1;

    // default values, just in case.
    *startMz = 200;
    *endMz = 2000;

    e = getScanInfoListAtLevel(msLevel, &scanList); ree;

    scan_increment = scanList.size() / max_number_of_scans_to_check;

    if (scan_increment <= 0) {
        scan_increment = 1;
    }

    auto ths = const_cast<MSReader *>(this); // avoid const issue

    for (int i = 0; i < scanList.size(); i += scan_increment) {
        e = ths->getScanData(scanList[i].scanNumber, &points); ree;

        if (points.size() <= 0) {
            continue;
        }

        if (first_time) {
            *startMz = points.front().x();
            *endMz = points.back().x();
            first_time = 0;
        } else {
            *startMz = std::min<double>(*startMz, points.front().x());
            *endMz = std::max<double>(*endMz, points.back().x());
        }
    }

    return e;
}

// Note: excludes scanStart, so this will get prefix_sum(scanEnd) +/- sum(scanStart+1, scanStart+2,
// ..., scanEnd-1)
Err MSReader::_getScanDataMS1SumManualScanNumber(int msLevel, long scanStart, long scanEnd,
                                                 GridUniform::ArithmeticType type,
                                                 GridUniform *gridOut)
{
    Err e = kNoErr;
    ScanInfo scanInfo;
    PlotBase plot;

    const long scanExcludingStart = scanStart + 1;

    for (long scan = scanExcludingStart; scan <= scanEnd; scan++) {
        e = getScanInfo(scan, &scanInfo); ree;

        if (scanInfo.scanLevel == msLevel) {
            e = getScanData(scan, &plot.getPointList()); ree;

            plot.sortPointListByX();
            gridOut->accumulate(plot, type);
        }
    }

    return e;
}

Err MSReader::_getScanDataMS1SumManualScanTime(int msLevel, double startTime, double endTime,
                                               GridUniform::ArithmeticType type,
                                               GridUniform *outGrid)
{
    Err e = kNoErr;
    long scanStart = -1;
    long scanEnd = -1;
    double x_start = 0;
    double x_end = 1;
    // TODO: bad static usage, change to const
    static double mz_bin_space = 0.005;

    e = getDomainInterval_sampleContent(msLevel, &x_start, &x_end); ree;

    outGrid->initGridByMzBinSpace(x_start, x_end, mz_bin_space);

    e = getBestScanNumber(msLevel, startTime, &scanStart); ree;

    e = getBestScanNumber(msLevel, endTime, &scanEnd); ree;

    e = _getScanDataMS1SumManualScanNumber(msLevel, scanStart, scanEnd, type, outGrid); ree;

    return e;
}

Err MSReader::_getScanDataMS1SumPrefix(int msLevel, double startTime, double endTime,
                                       GridUniform *outGrid, QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;
    long scanStart = -1;
    long scanEnd = -1;
    GridUniform startGrid;
    std::shared_ptr<MS1PrefixSum> sumPtr;

    outGrid->clear();

    e = getPrefixSumCache(&sumPtr, progress); ree;

    long totalNumber = 0, totalStartScan = 0, totalEndScan = 0;
    e = getNumberOfSpectra(&totalNumber, &totalStartScan, &totalEndScan);

    e = getBestScanNumber(msLevel, startTime, &scanStart); ree;
    e = getBestScanNumber(msLevel, endTime, &scanEnd); ree;

    // Note: We want the scan sum to be inclusive.  This helps with corner cases when the start and
    // end scans are exactly the same.  E.g. sum of scans {1} should return that one scan instead of
    // sum of {}  E.g. sum of scans {1,2} should return the sum of two scans instead of sum of {1}.
    // Implementation detail: we want scans summed from 10 to 15.
    // Summed start scan should be 9 = {1,2,...,9}
    // Summed end scan should be 15 = {1,2,....,9,10,..15}
    // Subtraction will therefore be summed scans of {10,...,15}

    // NOTE(TODO): note that this current solution assumes that there only single ms level.  If this
    // contains mixture of ms level one and two,  current solution may fail on corner cases.  We need
    // to use getScanInfoListAtLevel(1,....) information to correct for this.  Minor issue for now as
    // this use case is not going to appear.

    if (scanStart > totalStartScan) {
        e = sumPtr->getScanData(scanStart - 1, &startGrid); ree;
    } else {
        // scanStart <= totalStartScan, which means scanStart == totalStartScan. (Most likley it's
        // 1)  We do not need to extract 'empty scan' just to have it subtracted.
    }

    e = sumPtr->getScanData(scanEnd, outGrid); ree;

    // make sure start and end are different values.
    if (scanStart > totalStartScan) {
        e = outGrid->accumulate(startGrid, GridUniform::ArithmeticType_Subtract); ree;
    }

    return e;
}

Err MSReader::getPrefixSumCache(std::shared_ptr<MS1PrefixSum> *ms1PrefixSumPtr,
                                QSharedPointer<ProgressBarInterface> progress)
{
    Err e = kNoErr;

    if (m_openReader.isNull()) {
        rrr(kBadParameterError);
    }

    debugMs() << "m_openReader->getFilename()=" << m_openReader->getFilename();

    std::shared_ptr<MS1PrefixSum> sumPtr = m_fileName_ms1PrefixSumPtr[m_openReader->getFilename()];

    if (!sumPtr) {
        sumPtr = std::shared_ptr<MS1PrefixSum>(new MS1PrefixSum(this));
        sumPtr->setMaxNumberOfScansToCache(m_maxNumberOfPrefixSumScansToCache);
        m_fileName_ms1PrefixSumPtr[m_openReader->getFilename()] = sumPtr;
    }

    if (!sumPtr->isValid()) {
        warningMs() << "Cache is invalid.";

        // if table could not be removed
        if (sumPtr->removeAllTablesRelatedToMS1Cache() != kNoErr) {
            warningMs() << "Warning: GridUniform tables could not be removed from cache.";

            // if cache file couldn't be deleted
            if (!sumPtr->deleteCacheFile()) {
                warningMs() << "Warning: Couldn't delete corrupted / invalid cache file. "
                               "Defaulting to manual ms1 summing.";

                rrr(kError);
            }
        }

        e = sumPtr->createPrefixSumCache(progress); ree;

        if (!sumPtr->isValid()) {
            warningMs() << "Warning: after recreating cache file, the file is still invalid";

            rrr(kBadParameterError);
        }
    }

    if (ms1PrefixSumPtr != nullptr) {
        *ms1PrefixSumPtr = sumPtr;
    }

    return e;
}

void MSReader::setUseScanInfoCache(bool useCache)
{
    m_useScanInfoCache = useCache;
}

int MSReader::bestMSLevelOne() const
{
    int msLevel = 1;

    if (!m_openReader) {
        debugMs() << "Calling bestMSLevelOne without open file";

        return msLevel;
    }

    // Some MS data do not have MS1 but only MS2; we'll assume that is MS1
    const int numOfScans = m_openReader->numberOfScansAtLevel(msLevel);

    if (numOfScans < 0) {
        warningMs() << "numOfScans returned " << numOfScans << "for m_openReader"
                    << m_openReader->getFilename();
    } else if (numOfScans == 0) {
        msLevel = 2;
    }

    return msLevel;
}

MSReaderBase::FileInfoList MSReader::filesInfoList() const
{
    if (m_openReader) {
        return m_openReader->filesInfoList();
    }

    return MSReaderBase::FileInfoList();
}

void MSReader::setXICDataMode(XICMode mode)
{
    m_xicMode = mode;
}

namespace
{

/*!
* \brief returns the sum of the critical points between the given mz range
* \param plot input profile or centroid data
* \param mz_start start mz range, if -1, assume no mz contraint
* \param mz_end end mz range, if -1, assume no mz contraint
* \param range_sum_value sum of y-intensity values
* \return sum of all values within the given m/z range.
*/
double extractMS1Sum(const PlotBase &plot, double mz_start, double mz_end)
{
    double range_sum_value = 0;

    if (plot.getPointListSize() <= 0) {
        return range_sum_value;
    }

    point2dList inList;
    const bool sum_all = mz_start < 0 && mz_end < 0; // If -1 and -1, sum all.

    if (sum_all) {
        return getYSumAll(plot.getPointList());
    } else {
        plot.getPoints(mz_start, mz_end, true, inList);
    }

    // critial_point = getMaxPoint(inList);

    for (unsigned int i = 0; i < inList.size(); i++) {
        range_sum_value += inList[i].y();
    }

    return range_sum_value;
}
}

Err MSReader::_getXICManual(const XICWindow &win, point2dList *points, int msLevel) const
{
    Q_ASSERT(points);

    Err e = kNoErr;
    long startScan = -1;
    long endScan = -1;

    if (!m_openReader) {
        e = kError; ree;
    }

    PlotBase plot;
    point2d time_point;

    points->clear();

    e = m_openReader->getBestScanNumberFromScanTime(msLevel, win.time_start, &startScan); eee;

    e = m_openReader->getBestScanNumberFromScanTime(msLevel, win.time_end, &endScan); eee;

    if (startScan > endScan) {
        debugMs()
            << "Error, scan numbers are in reverse order.  time_start,time_end,startScan,endScan="
            << win.time_start << "," << win.time_end << "," << startScan << "," << endScan;

        e = kError; eee;
    }

    for (long scan = startScan; scan <= endScan; scan++) {
        ScanInfo info;

        e = getScanInfo(scan, &info); eee;

        if (info.scanLevel != msLevel) {
            continue;
        }

        bool found = false;

#ifdef PMI_MS_ENABLE_CENTROID_CACHE
        auto cacheManager = const_cast<MSCentroidCacheManager *>(m_centroidCacheManager);
        auto it = cacheManager->getCacheIt(m_openReader->getFilename(), scan, &found);

        if (found) {
            plot.setPointList(it.value());
        }
#endif

        if (!found) {
            // Use this->getScanData instead of calling via m_openReader to get proper calibration
            // done.
            auto ms = const_cast<MSReader *>(this);

            e = ms->getScanData(scan, &plot.getPointList(), true, nullptr); eee;

#ifdef PMI_MS_ENABLE_CENTROID_CACHE
            cacheManager->setScanCache(m_openReader->getFilename(), scan, plot.getPointList());
#endif
        }

        time_point.rx() = info.retTimeMinutes;
        time_point.ry() = extractMS1Sum(plot, win.mz_start, win.mz_end);

        points->push_back(time_point);
    }

error:
    return e;
}

Err MSReader::_getBasePeakManual(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;

    if (!m_openReader) {
        e = kError; ree;
    }

    PlotBase plot;
    point2d time_point;
    long totalNumber = 0;
    long startScan = -1;
    long endScan = -1;
    const int msLevel = bestMSLevelOne();

    points->clear();

    e = m_openReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan); ree;

    for (long scan = startScan; scan <= endScan; scan++) {
        ScanInfo info;

        e = getScanInfo(scan, &info); ree;

        if (info.scanLevel != msLevel) {
            continue;
        }

        // Note: direct call to m_openReader->getScanData will not apply calibration.
        //      But since we don't need to m/z calibration for TIC, we will use m_openReader for
        //      speed.
        // 
        // Warning: If the Agilent file does not have peak data, than we get incorrect result here 
        // because MSReaderAgilent will return empty vector and kNoErr in that case
        // but we need similar behaviour like PeakElseProfile in this case
        e = m_openReader->getScanData(scan, &plot.getPointList(), true); ree;

        time_point.rx() = info.retTimeMinutes;
        time_point.ry() = plot.getMaxPoint().ry();

        points->push_back(time_point);
    }

    return e;
}

Err MSReader::_getTICManual(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;

    if (!m_openReader) {
        e = kError; ree;
    }

    PlotBase plot;
    point2d time_point;
    long totalNumber = 0;
    long startScan = -1;
    long endScan = -1;
    const int msLevel = bestMSLevelOne();

    points->clear();

    e = m_openReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan); eee;

    for (long scan = startScan; scan <= endScan; scan++) {
        ScanInfo info;

        e = getScanInfo(scan, &info); eee;

        if (info.scanLevel != msLevel) {
            continue;
        }

        // Note: direct call to m_openReader->getScanData will not apply calibration.
        //      But since we don't need to m/z calibration for TIC, we will use m_openReader for
        //      speed.
        // Note: same problem for agilent files without peak data here, e.g. P:\PMI-Dev\J\SS\800\SS-837\18Sep01_03.d
        // Will return kNoErr and empty scan, but calling MSReader::getScanData would provide proper result, 
        // but worse performance?
        e = m_openReader->getScanData(scan, &plot.getPointList(), true); eee;

        time_point.rx() = info.retTimeMinutes;
        time_point.ry() = extractMS1Sum(plot, -1, -1);

        points->push_back(time_point);
    }

error:
    return e;
}

Err MSReader::_getBestScanNumber(int msLevel, double scanTime, long *scanNumber) const
{
    Q_ASSERT(scanNumber);

    Err e = kNoErr;
    long curr_scanNumber = -1;

    *scanNumber = -1;

    if (!m_openReader) {
        rrr(kError);
    }

    e = m_openReader->getBestScanNumberFromScanTime(
        msLevel, scanTime + MSReaderBase::scantimeFindParentFudge, &curr_scanNumber); eee;

    while (curr_scanNumber >= 1) {
        ScanInfo obj;

        e = getScanInfo(curr_scanNumber, &obj); eee;

        if (obj.scanLevel == msLevel) {
            *scanNumber = curr_scanNumber;

            return e;
        }

        curr_scanNumber--;
    }

    // No MS level found.
    e = kBadParameterError; eee;

error:
    return e;
}

Err MSReader::_loadCentroidOptionsFromDatabase(const QString &filename)
{
    Err e = kNoErr;

    {
        QSqlDatabase db;

        e = addDatabaseAndOpen("loadCentroidOptionsFromDatabase", filename, db); eee;

        if (containsTable(db, kProjectCreationOptions)) {
            RowNode centroidOptions;

            pmi::CentroidOptions::initOptions(centroidOptions); // to avoid warning messages

            e = rn::loadOptions(db, kProjectCreationOptions, kCentroidingOptions, centroidOptions); eee;

            m_centroidOption.loadFromRowNode(centroidOptions);
        }
    }

error:
    QSqlDatabase::removeDatabase("loadCentroidOptionsFromDatabase");

    return e;
}


void MSReader::clearVendorsRoutedThroughByspec()
{
    m_vendorsRoutedThroughByspec.clear();
}

void MSReader::setVendorsRoutedThroughByspec(
    const QSet<MSReaderBase::MSReaderClassType> &routedVendors)
{
    m_vendorsRoutedThroughByspec = routedVendors;
}

QSet<MSReaderBase::MSReaderClassType> MSReader::vendorsRoutedThroughByspec() const
{
    return m_vendorsRoutedThroughByspec;
}

bool MSReader::isBrukerTims() const
{
    return m_openReader != nullptr
        && m_openReader->classTypeName() == MSReaderBase::MSReaderClassTypeBrukerTims;
}

Err MSReader::_makeReader(const QString &fileName)
{
    Err e = kNoErr;

    if (m_openReader) {
        // Note: closing file here will release bruker handle but keep it in the m_openedReaderList.
        // This would cause a bug.  We also shouldn't be closing existing open connection.
        // m_openReader->closeFile();
        m_openReader.clear();
    }

    // No existing reader exists.  We'll attempt to create a new one.
    for (int i = 0; i < m_vendorList.size(); i++) {
        if (m_vendorList[i]->canOpen(fileName)) {

            if (m_vendorsRoutedThroughByspec.contains(m_vendorList[i]->classTypeName())) {
                m_openedReaderList.push_back(makeReader(MSReaderBase::MSReaderClassTypeByspec));
                m_openReader = m_openedReaderList.back();
                debugMs() << "Creating new ms list using byspec for classTypeName=" << m_vendorList[i]->classTypeName();
                break;
            }

            m_openedReaderList.push_back(makeReader(m_vendorList[i]->classTypeName()));
            m_openReader = m_openedReaderList.back();
            break;
        }
    }

    if (m_openReader.isNull()) {
        rrr(kFileIncorrectTypeError);
    }
    debugMs() << "Creating new ms list with total open list of ="
              << m_openedReaderList.size();

    return e;
}

void MSReader::_closeAllByspecConnections()
{
    QList<QSharedPointer<MSReaderBase>> saveList;

    for (int i = 0; i < m_openedReaderList.size(); i++) {
        if (m_openedReaderList[i]->classTypeName() == MSReaderBase::MSReaderClassTypeByspec) {
            m_openedReaderList[i]->closeFile();
            m_openedReaderList[i]->setFilename(QString());
        } else {
            saveList.push_back(m_openedReaderList[i]);
        }
    }

    m_openedReaderList.clear();

    if (m_openReader) {
        m_openReader.clear();
    }

    m_openedReaderList = saveList;
}

// This is short-term solution to pass centroid information to MSReaderByspec, which will then call
// proper centroid arguments to  the two pico and msconvert-pmi .exe applications.
void MSReader::_handlePassingCentroidToMSReaderByspec()
{
    if (m_openReader) {
        auto ms_pwiz = dynamic_cast<msreader::MSReaderByspec *>(m_openReader.data());

        if (ms_pwiz) {
            ms_pwiz->setCentroidOptionsByspec(m_centroidOption);
        }
    }
}

Err MSReader::getXICDataCached(const XICWindow &win, point2dList *points, int ms_level)
{
    Q_ASSERT(points);

    Err e = kNoErr;

    QSharedPointer<MSDataNonUniformAdapter> nonUniformTilesCache
        = m_fileName_nonUniformTiles[m_openReader->getFilename()];

    // lazy creation
    if (!nonUniformTilesCache) {
        const QString rawFileName = m_openReader->getFilename();

        QString nonUniformFilePath;
        e = m_cacheFileManager->findOrCreateCachePath(MSDataNonUniformAdapter::formatSuffix(),
                                                      &nonUniformFilePath); ree;

        debugMs() << "Instatiate NonUniform tile cache for file" << rawFileName;
        nonUniformTilesCache = QSharedPointer<MSDataNonUniformAdapter>::create(nonUniformFilePath);
        m_fileName_nonUniformTiles[rawFileName] = nonUniformTilesCache;
    }

    // lazy initialization
    QList<ScanInfoWrapper> scanInfo;

    if (!nonUniformTilesCache->hasValidCacheDbFile()) {
        m_openReader->getScanInfoListAtLevel(1, &scanInfo);

        debugMs() << "Initializing NonUniform tile cache content for" << scanInfo.size()
                  << "scan numbers";

        bool ok = false;
        NonUniformTileRange range
            = nonUniformTilesCache->createRange(this, true, scanInfo.size(), &ok);

        if (!ok) {
            rrr(kBadParameterError);
        }

        // override tile settings here (e.g. from advanced settings's ini file)
        // range.setMzTileLength(1.0);
        // range.setScanIndexLength(10);
        e = nonUniformTilesCache->createNonUniformTiles(this, range, scanInfo,
                                                        nullptr /*progress.data()*/); ree;
    }

    if (!nonUniformTilesCache->isLoaded()) {
        debugMs() << "Loading NonUniform tile cache!";

        if (scanInfo.isEmpty()) {
            m_openReader->getScanInfoListAtLevel(1, &scanInfo);
        }

        e = nonUniformTilesCache->load(scanInfo); ree;
    }

    XICWindow fixedWindow;

    e = alignTimesInWindow(win, nonUniformTilesCache.data(), ms_level, &fixedWindow); ree;

    // now we should be ready to provide results
    e = nonUniformTilesCache->getXICData(fixedWindow, points, ms_level); ree;

    return e;
}

Err MSReader::alignTimesInWindow(const XICWindow &win, const MSDataNonUniformAdapter *adapter,
                                 int msLevel, XICWindow *window) const
{
    if (!adapter || !window || !m_openReader) {
        return kBadParameterError;
    }

    // transform the times
    long startScanNumber;
    long endScanNumber;

    Err e = m_openReader->getBestScanNumberFromScanTime(msLevel, win.time_start, &startScanNumber); ree;

    e = m_openReader->getBestScanNumberFromScanTime(msLevel, win.time_end, &endScanNumber); ree;

    XICWindow result = win;

    result.time_start = adapter->scanNumberToScanTime(startScanNumber);
    result.time_end = adapter->scanNumberToScanTime(endScanNumber);

    *window = result;

    return kNoErr;
}

int MSReader::maxNumberOfPrefixSumScansToCache() const
{
    return m_maxNumberOfPrefixSumScansToCache;
}

void MSReader::setMaxNumberOfPrefixSumScansToCache(int size)
{
    m_maxNumberOfPrefixSumScansToCache = size;
}

CacheFileManagerInterface *MSReader::cacheFileManager()
{
    return m_cacheFileManager.data();
}

const CacheFileManagerInterface *MSReader::cacheFileManager() const
{
    return m_cacheFileManager.data();
}

void MSReader::dumpXicData(const QString &msFilePath, const point2dList &actual,
                           const point2dList &expected) const
{
    static int dumpCounter = 0;

    dumpCounter++;

    qWarning() << "Nonuniform differs to expected for" << dumpCounter << "time";

    const QString dumpDir = QDir::tempPath() + QString("/pmi/nut/"); // customize-here
    const QString rawBaseName = QFileInfo(msFilePath).baseName();
    QString fileName = QString("%1_%2_nonUniform.csv").arg(rawBaseName).arg(dumpCounter);

    PlotBase(actual).saveDataToFile(QDir(dumpDir).filePath(fileName));

    fileName = QString("%1_%2_expected.csv").arg(rawBaseName).arg(dumpCounter);

    PlotBase(expected).saveDataToFile(QDir(dumpDir).filePath(fileName));
}

QSharedPointer<MSReaderBase> MSReader::makeReader(MSReaderBase::MSReaderClassType reader) const
{
    if (reader == MSReaderBase::MSReaderClassTypeThermo) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderThermo());
    }

#ifdef PMI_MS_ENABLE_BRUKER_API
    if (reader == MSReaderBase::MSReaderClassTypeBrukerTims) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderBrukerTims());
    }
    if (reader == MSReaderBase::MSReaderClassTypeBruker) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderBruker());
    }
#endif

#ifdef PMI_MS_ENABLE_ABI_DISK_TO_MEMORY
    if (reader == MSReaderBase::MSReaderClassTypeABSCIEX) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderAbi());
    }
#endif

#ifdef PMI_MS_ENABLE_SHIMADZU_API
    if (reader == MSReaderBase::MSReaderClassTypeShimadzu) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderShimadzu());
    }
#endif

#ifdef PMI_MS_ENABLE_AGILENT_API
    if (reader == MSReaderBase::MSReaderClassTypeAgilent) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderAgilent());
    }
#endif

#ifdef PMI_QT_COMMON_BUILD_TESTING
    if (reader == MSReaderBase::MSReaderClassTypeTesting) {
        return QSharedPointer<MSReaderBase>(new pmi::MSReaderTesting());
    }
#endif
    if (reader == MSReaderBase::MSReaderClassTypeSimulated) {
        return QSharedPointer<MSReaderBase>(new msreader::MSReaderSimulated);
    }

    return QSharedPointer<MSReaderBase>(new msreader::MSReaderByspec(*m_cacheFileManager.data()));
}

_PMI_END
