/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "MSReaderShimadzu.h"

#ifdef Q_OS_WIN

#include "AdvancedSettings.h"
#include "pmi_common_ms_debug.h"
#include "pmi_core_defs.h"
#include "vendor_api/Shimadzu/ShimadzuReaderWrapperInterface.h"
#include "VendorPathChecker.h"

#include "qt_utils.h"

#include <QCoreApplication>
#include <QDir>

#include <Windows.h>

_PMI_BEGIN
_MSREADER_BEGIN

#ifdef _DEBUG
#define SHIMADZU_READER_WRAPPER_DLL "vendor_api/Shimadzu/ShimadzuReaderWrapper_d.dll"
#else
#define SHIMADZU_READER_WRAPPER_DLL "vendor_api/Shimadzu/ShimadzuReaderWrapper.dll"
#endif

static const int s_shimadzuDebugShow = false;
static const QString s_defaultFragmentType = QStringLiteral("QTOF/HCD");

static const QString ID_PARTS_DELIMITER = QStringLiteral(";");
static const QString ID_PARTS_KEYVALUE_DELIMITER = QStringLiteral("=");
static const QString ID_SEGMENT_KEY = QStringLiteral("s");
static const QString ID_SEGMENT_VALUE = QStringLiteral("1");
static const QString ID_EVENT_KEY = QStringLiteral("e");
static const QString ID_EVENT_DEFAULT_VALUE = QStringLiteral("1");

using namespace readerwrappers;

/*!
* \brief ID parser/generator for Shimadzu chromatograms
*/
struct ChromatogramCanonicalId
{
    QString chroType;
    QString eventNo;

    ChromatogramCanonicalId(const QString &chroType, const QString &eventNo)
        : chroType(chroType)
        , eventNo(eventNo)
    {
    }

    ChromatogramCanonicalId(const QString &canonicalName)
    {
        QStringList tokens = canonicalName.trimmed().split(ID_PARTS_DELIMITER, QString::SkipEmptyParts);
        if (!tokens.isEmpty()) {
            chroType = tokens.takeFirst();
            eventNo = ID_EVENT_DEFAULT_VALUE;
            for (auto token : tokens) {
                const QStringList keyValue
                    = token.trimmed().split(ID_PARTS_KEYVALUE_DELIMITER, QString::KeepEmptyParts);
                if (keyValue.size() != 2) {
                    continue;
                }
                if (keyValue[0] == ID_EVENT_KEY) {
                    eventNo = keyValue[1];
                }
            }
        }
    }

    QString toString() const
    {
        static const QString chroTypeSuffix = ID_PARTS_DELIMITER + ID_SEGMENT_KEY
            + ID_PARTS_KEYVALUE_DELIMITER + ID_SEGMENT_VALUE + ID_PARTS_DELIMITER + ID_EVENT_KEY
            + ID_PARTS_KEYVALUE_DELIMITER;
        QString result = chroType + chroTypeSuffix;
        if (eventNo.isEmpty()) {
            result += ID_EVENT_DEFAULT_VALUE;
        }
        else {
            result += eventNo;
        }
        return result;
    }

    bool isEmpty() const
    {
        return chroType.isEmpty();
    }

    bool operator==(const ChromatogramCanonicalId &other) const
    {
        return chroType == other.chroType && eventNo == other.eventNo;
    }
};


class MSReaderShimadzu::PrivateData
{
public:
    PrivateData() {}
    ~PrivateData() { releaseInstance(); }

    bool valid() const { return m_valid; }

    ShimadzuReaderWrapperInterface *shimadzuReaderWrapper()
    {
        if (m_shimadzuReaderWrapper.isNull()) {
            createInstance();
        }
        return m_shimadzuReaderWrapper.data();
    }

    QMap<int, QString> m_sampleNumberNames;

private:
    void createInstance()
    {
        m_valid = false;
        HRESULT hr = 0;

        const QDir appDir(QCoreApplication::applicationDirPath());
        const QString dllPath = appDir.absoluteFilePath(SHIMADZU_READER_WRAPPER_DLL);

        if (s_shimadzuDebugShow) {
            debugMs() << "dllPath = " << dllPath;
        }
        if (s_hDll == nullptr) {
            s_hDll = LoadLibrary((wchar_t*)dllPath.utf16());
            if (s_shimadzuDebugShow) {
                debugMs() << "hDLL = " << s_hDll;
            }
        }
        if (s_hDll != nullptr) {
            typedef void *(*pvFunctv)();
            pvFunctv CreateShimadzuReader
                = (pvFunctv)(GetProcAddress(s_hDll, "CreateShimadzuReaderWrapperInstance"));
            m_shimadzuReaderWrapper.reset(
                static_cast<ShimadzuReaderWrapperInterface*>(CreateShimadzuReader()));

            if (hr == S_OK) {
                m_valid = true;
            }
        }

        if (!m_valid) {
            debugMs() << "Could not load Shimadzu reader";
            if (s_shimadzuDebugShow) {
                debugMs() << "dllPth = " << dllPath;
                debugMs() << "hDll = " << s_hDll;
                debugMs() << "hr = " << hr;
            }
        }
    }

    void releaseInstance()
    {
        m_shimadzuReaderWrapper.reset();
    }

private:
    bool m_valid = false;
    static HMODULE s_hDll;
    QScopedPointer<ShimadzuReaderWrapperInterface> m_shimadzuReaderWrapper;
};

HMODULE MSReaderShimadzu::PrivateData::s_hDll = nullptr;

MSReaderShimadzu::MSReaderShimadzu()
    : d(new PrivateData)
{
}

MSReaderShimadzu::~MSReaderShimadzu()
{
    closeFile();
}

bool MSReaderShimadzu::canOpen(const QString &path) const
{
    return VendorPathChecker::formatShimadzu(path)
        == VendorPathChecker::ReaderShimadzuFormatValid;
}

Err MSReaderShimadzu::openFile(const QString &filename, MSConvertOption convertOptions,
                               QSharedPointer<ProgressBarInterface> progress)
{
    Q_UNUSED(progress);
    Q_UNUSED(convertOptions);

    ShimadzuReaderWrapperInterface *reader = d->shimadzuReaderWrapper();

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    HRESULT hr = reader->openFile(filename.toStdString());
    if (hr != S_OK) {
        debugMs() << "Could not open filename=" << filename << endl;
        debugMs() << "    Error: " << reader->getLastErrorMessage().c_str() << endl;
        rrr(kBadParameterError);
    }
    setFilename(filename);

    return kNoErr;
}

Err MSReaderShimadzu::closeFile()
{
    if (!d->valid()) {
        return kNoErr;
    }

    HRESULT hr = d->shimadzuReaderWrapper()->closeFile();
    if (hr != S_OK) {
        debugMs() << "Could not close file." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    return kNoErr;
}

Err MSReaderShimadzu::getBasePeak(point2dList *points) const
{
    Q_ASSERT(points);

    points->clear();

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::vector<double> times;
    std::vector<double> intensities;

    HRESULT hr = d->shimadzuReaderWrapper()->getBasePeakData(&times, &intensities);
    if (hr != S_OK) {
        debugMs() << "Could not get BasePeak." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    points->reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderShimadzu::getTICDataByEvent(int eventNo, point2dList *points) const
{
    Q_ASSERT(points);

    points->clear();

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::vector<double> times;
    std::vector<double> intensities;

    HRESULT hr = d->shimadzuReaderWrapper()->getTicDataByEvent(eventNo, &times, &intensities);
    if (hr != S_OK) {
        debugMs() << "Could not get TIC for event #" << eventNo << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
            << endl;
        rrr(kBadParameterError);
    }

    points->reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderShimadzu::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    points->clear();

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::vector<double> times;
    std::vector<double> intensities;

    HRESULT hr = d->shimadzuReaderWrapper()->getTicData(&times, &intensities);
    if (hr != S_OK) {
        debugMs() << "Could not get TIC." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    points->reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderShimadzu::getScanData(long scanNumber, point2dList *points, bool doCentroiding,
                                  PointListAsByteArrays *pointListAsByteArrays)
{
    Q_ASSERT(points);
    Q_UNUSED(pointListAsByteArrays);

    points->clear();

    // TODO: pointListAsByteArrays <-- fill this with compressed bytearray

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::vector<double> mzs;
    std::vector<double> intensities;
    HRESULT hr
        = d->shimadzuReaderWrapper()->getScanData(scanNumber, &mzs, doCentroiding, &intensities);
    if (hr != S_OK) {
        debugMs() << "Could not get Scan Data." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    points->reserve(mzs.size());

    for (size_t i = 0; i < mzs.size(); ++i) {
        points->push_back(point2d(mzs[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderShimadzu::getXICData(const XICWindow &win, point2dList *points, int msLevel) const
{
    Q_ASSERT(points);

    points->clear();

    if (msLevel != 1) {
        rrr(kBadParameterError);
    }
    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::vector<double> times;
    std::vector<double> intensities;

    HRESULT hr = d->shimadzuReaderWrapper()->getXicData(win.mz_start, win.mz_end, win.time_start,
                                                        win.time_end, &times, &intensities);
    if (hr != S_OK) {
        debugMs() << "Could not get XIC Data." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    points->reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderShimadzu::getScanInfo(long scanNumber, ScanInfo *obj) const
{
    Q_ASSERT(obj);

    double retTimeMinutes = 0;
    int scanLevel = 0;
    int peakMode = 0;
    int scanMethod = 0;
    std::string nativeIdString;

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    HRESULT hr = d->shimadzuReaderWrapper()->getScanInfo(scanNumber, &scanLevel, &retTimeMinutes,
                                                         &nativeIdString, &peakMode, &scanMethod);
    if (hr != S_OK) {
        debugMs() << "Could not get Scan Info." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    obj->retTimeMinutes = retTimeMinutes;
    obj->scanLevel = scanLevel;
    obj->peakMode = static_cast<PeakPickingMode>(peakMode);
    obj->scanMethod = static_cast<ScanMethod>(scanMethod);
    obj->nativeId = QString::fromStdString(nativeIdString);

    return kNoErr;
}

Err MSReaderShimadzu::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    double dIsolationMass = 0;
    double dMonoIsoMass = 0;
    long nChargeState = 0;
    long nScanNumber = 0;
    std::string nativeIdString;

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    HRESULT hr = d->shimadzuReaderWrapper()->getScanPrecursorInfo(
        scanNumber, &dIsolationMass, &dMonoIsoMass, &nChargeState, &nScanNumber, &nativeIdString);
    if (hr != S_OK) {
        debugMs() << "Could not get Scan Precursor Info." << endl;
        debugMs() << "    Error: " << d->shimadzuReaderWrapper()->getLastErrorMessage().c_str()
                  << endl;
        rrr(kBadParameterError);
    }

    pinfo->dIsolationMass = dIsolationMass;
    pinfo->dMonoIsoMass = dMonoIsoMass;
    pinfo->nChargeState = nChargeState;
    pinfo->nScanNumber = nScanNumber;
    pinfo->nativeId = QString::fromStdString(nativeIdString);

    return kNoErr;
}

Err MSReaderShimadzu::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Q_ASSERT(totalNumber);
    Q_ASSERT(startScan);
    Q_ASSERT(endScan);

    HRESULT hr = d->shimadzuReaderWrapper()->getNumberOfSpectra(totalNumber, startScan, endScan);
    if (hr != S_OK) {
        rrr(kBadParameterError);
    }
    return kNoErr;
}

Err MSReaderShimadzu::getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
{
    Q_ASSERT(scanNumber);

    HRESULT hr
        = d->shimadzuReaderWrapper()->getBestScanNumber(msLevel, scanTimeMinutes, scanNumber);
    if (hr != S_OK) {
        rrr(kBadParameterError);
    }
    return kNoErr;
}

Err MSReaderShimadzu::getChromatograms(QList<ChromatogramInfo> *chroList,
                                       const QString &internalChannelName)
{
    chroList->clear();

    static const QString CHANNEL_NAME_TIC = QStringLiteral("Shimadzu TIC");
    static const QString CHANNEL_ID_TIC = QStringLiteral("tic");
    static const QString CHANNEL_NAME_BP = QStringLiteral("Shimadzu BasePeak");
    static const QString CHANNEL_ID_BP = QStringLiteral("bp");
    static const QString CHANNEL_NAME_XIC = QStringLiteral("Shimadzu XIC");
    static const QString CHANNEL_ID_XIC = QStringLiteral("xic");

    const ChromatogramCanonicalId filterId(internalChannelName);

    if (filterId.isEmpty() || filterId.chroType == CHANNEL_ID_BP) {
        ChromatogramInfo basePeakChromatogram;
        getBasePeak(&basePeakChromatogram.points);
        basePeakChromatogram.channelName = CHANNEL_NAME_BP;
        basePeakChromatogram.internalChannelName
            = ChromatogramCanonicalId(CHANNEL_ID_BP).toString();
        chroList->append(basePeakChromatogram);
    }

    if (filterId.isEmpty() || filterId.chroType == CHANNEL_ID_XIC) {
        ChromatogramInfo xicChromatogram;
        getXICData(XICWindow(), &xicChromatogram.points);
        xicChromatogram.channelName = CHANNEL_NAME_XIC;
        xicChromatogram.internalChannelName = ChromatogramCanonicalId(CHANNEL_ID_XIC).toString();
        chroList->append(xicChromatogram);
    }

    if (filterId.isEmpty() || filterId.chroType == CHANNEL_ID_TIC) {
        std::vector<int> events;
        d->shimadzuReaderWrapper()->getEvents(&events);
        for (int eventNo : events) {
            const ChromatogramCanonicalId id(CHANNEL_ID_TIC, QString::number(eventNo));
            if (filterId.isEmpty() || filterId == id) {
                ChromatogramInfo ticChromatogram;
                getTICDataByEvent(eventNo, &ticChromatogram.points);
                ticChromatogram.channelName
                    = CHANNEL_NAME_TIC + QStringLiteral(" (%1)").arg(eventNo);
                ticChromatogram.internalChannelName = id.toString();
                chroList->append(ticChromatogram);
            }
        }
    }

    return kNoErr;
}

Err MSReaderShimadzu::getFragmentType(long scanNumber, long scanLevel, QString *fragType) const
{
    // this function is needed only for scanLevel >= 2.
    Q_UNUSED(scanNumber);
    
    Q_ASSERT(fragType != nullptr);

    if (scanLevel > 1) {
        *fragType = s_defaultFragmentType;
    }

    // DissociationType

    return kNoErr;
}

#ifdef PMI_QT_COMMON_BUILD_TESTING
MSReaderShimadzu::DebugOptions MSReaderShimadzu::debugOptions() const
{
    const ShimadzuReaderWrapperInterface::DebugOptions options
        = d->shimadzuReaderWrapper()->debugOptions();
    DebugOptions result;
    result.getNumberOfSpectraMaxScan = options.getNumberOfSpectraMaxScan;
    return result;
}

void MSReaderShimadzu::setDebugOptions(const DebugOptions & debugOptions)
{
    ShimadzuReaderWrapperInterface::DebugOptions options;
    options.getNumberOfSpectraMaxScan = debugOptions.getNumberOfSpectraMaxScan;
    d->shimadzuReaderWrapper()->setDebugOptions(options);
}
#endif

_MSREADER_END
_PMI_END

#endif // Q_OS_WIN
