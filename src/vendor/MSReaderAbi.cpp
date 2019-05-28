/*
 * Copyright (C) 2015-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil <ykil@proteinmetrics.com>
 * Convert from COM to C++ calls: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "MSReaderAbi.h"

#include "AdvancedSettings.h"
#include "pmi_common_ms_debug.h"
#include "pmi_core_defs.h"
#include "vendor_api/ABI/AbiReaderWrapperInterface.h"
#include "VendorPathChecker.h"

#include "qt_utils.h"

#include <QCoreApplication>
#include <QDir>

#include <Windows.h>

_PMI_BEGIN
_MSREADER_BEGIN

using namespace readerwrappers;

static const int s_abiDebugShow = false;

#ifdef Q_OS_WIN

#ifdef _DEBUG
#define ABI_READER_WRAPPER_DLL "vendor_api/ABI/AbiReaderWrapper_d.dll"
#else
#define ABI_READER_WRAPPER_DLL "vendor_api/ABI/AbiReaderWrapper.dll"
#endif

static const QString ID_PARTS_DELIMITER = QStringLiteral(";");
static const QString ID_PARTS_KEYVALUE_DELIMITER = QStringLiteral("=");
static const QString ID_SAMPLE_KEY = QStringLiteral("s");
static const QString ID_SAMPLE_DEFAULT_VALUE = QStringLiteral("0");
static const QString ID_ADC_SAMPLE_KEY = QStringLiteral("a");

MSReaderAbi::ChromatogramCanonicalId::ChromatogramCanonicalId(){};

MSReaderAbi::ChromatogramCanonicalId::ChromatogramCanonicalId(const QString &chroType,
                                                              const QString &sampleNo,
                                                              const QString &adcSampleNo)
    : chroType(chroType)
    , sampleNo(sampleNo)
    , adcSampleNo(adcSampleNo)
{
}

MSReaderAbi::ChromatogramCanonicalId
MSReaderAbi::ChromatogramCanonicalId::fromInternalChannelName(const QString &internalChannelName)
{
    if (internalChannelName.trimmed().isEmpty()) {
        return MSReaderAbi::ChromatogramCanonicalId();
    }
    QStringList tokens
        = internalChannelName.trimmed().split(ID_PARTS_DELIMITER, QString::SkipEmptyParts);
    if (tokens.isEmpty()) {
        warningMs() << "MSReaderAbi::fromInternalChannelName: invalid argument internalChannelName="
                    << internalChannelName;
        return MSReaderAbi::ChromatogramCanonicalId();
    }

    MSReaderAbi::ChromatogramCanonicalId result;
    result.chroType = tokens.takeFirst();
    result.sampleNo = ID_SAMPLE_DEFAULT_VALUE;
    for (auto token : tokens) {
        const QStringList keyValue
            = token.trimmed().split(ID_PARTS_KEYVALUE_DELIMITER, QString::KeepEmptyParts);
        if (keyValue.size() != 2) {
            continue;
        }
        if (keyValue[0] == ID_SAMPLE_KEY) {
            result.sampleNo = keyValue[1];
        }
        if (keyValue[0] == ID_ADC_SAMPLE_KEY) {
            result.adcSampleNo = keyValue[1];
        }
    }
    return result;
}

QString MSReaderAbi::ChromatogramCanonicalId::toString() const
{
    if (isEmpty()) {
        return QString();
    }
    static const QString chroTypeSuffix
        = ID_PARTS_DELIMITER + ID_SAMPLE_KEY + ID_PARTS_KEYVALUE_DELIMITER;
    QString result = chroType + chroTypeSuffix;
    if (sampleNo.isEmpty()) {
        result += ID_SAMPLE_DEFAULT_VALUE;
    } else {
        result += sampleNo;
    }
    if (!adcSampleNo.isEmpty()) {
        result
            += ID_PARTS_DELIMITER + ID_ADC_SAMPLE_KEY + ID_PARTS_KEYVALUE_DELIMITER + adcSampleNo;
    }
    return result;
}

bool MSReaderAbi::ChromatogramCanonicalId::isEmpty() const
{
    return chroType.isEmpty();
}

bool MSReaderAbi::ChromatogramCanonicalId::operator==(const ChromatogramCanonicalId &other) const
{
    return chroType == other.chroType && sampleNo == other.sampleNo
        && adcSampleNo == other.adcSampleNo;
}

class MSReaderAbi::PrivateData
{
public:
    PrivateData() {}
    ~PrivateData() { releaseInstance(); }

    bool valid() const { return m_valid; }

    AbiReaderWrapperInterface *abiReaderWrapper()
    {
        if (m_abiReaderWrapper.isNull()) {
            createInstance();
        }
        return m_abiReaderWrapper.data();
    }

    QMap<int, QString> m_sampleNumberNames;

private:
    void createInstance()
    {
        m_valid = false;
        HRESULT hr = 0;

        const QDir appDir(QCoreApplication::applicationDirPath());
        const QString dllPath = appDir.absoluteFilePath(ABI_READER_WRAPPER_DLL);

        if (s_abiDebugShow) {
            debugMs() << "MSReaderAbi: dllPath = " << dllPath;
        }
        if (s_hDll == nullptr) {
            s_hDll = LoadLibrary((wchar_t*)dllPath.utf16());
            if (s_abiDebugShow) {
                debugMs() << "MSReaderAbi: hDLL = " << s_hDll;
            }
        }
        if (s_hDll != nullptr) {
            typedef void *(*pvFunctv)();
            pvFunctv CreateAbiReader
                = (pvFunctv)(GetProcAddress(s_hDll, "CreateAbiReaderWrapperInstance"));
            m_abiReaderWrapper.reset(
                static_cast<AbiReaderWrapperInterface*>(CreateAbiReader()));

            if (hr == S_OK) {
                m_valid = true;
            }
        }

        if (!m_valid) {
            debugMs() << "Could not load ABI reader";
            if (s_abiDebugShow) {
                debugMs() << "dllPth = " << dllPath;
                debugMs() << "hDll = " << s_hDll;
                debugMs() << "hr = " << hr;
            }
        }
    }

    void releaseInstance()
    {
        m_abiReaderWrapper.reset();
    }

private:
    bool m_valid = false;
    static HMODULE s_hDll;
    QScopedPointer<AbiReaderWrapperInterface> m_abiReaderWrapper;
};

HMODULE MSReaderAbi::PrivateData::s_hDll = nullptr;

MSReaderAbi::MSReaderAbi()
{
    d = new PrivateData;
}

MSReaderAbi::~MSReaderAbi()
{
    delete d;
}

MSReaderBase::MSReaderClassType MSReaderAbi::classTypeName() const
{
    return MSReaderClassTypeABSCIEX;
}

bool MSReaderAbi::canOpen(const QString &path) const
{
    return VendorPathChecker::formatAbi(path)
        == VendorPathChecker::ReaderAbiFormatValid;
}

Err MSReaderAbi::openFile(const QString &filename, MSConvertOption convertOptions,
                          QSharedPointer<ProgressBarInterface> progress)
{
    Q_UNUSED(progress);
    Q_UNUSED(convertOptions);

    AbiReaderWrapperInterface *reader = d->abiReaderWrapper();

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    HRESULT hr = reader->openFile(filename.toStdString());
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not open filename=" << filename << endl;
        debugMs() << "    Error: " << reader->getLastErrorMessage().c_str() << endl;
        rrr(kBadParameterError);
    }
    setFilename(filename);

    return kNoErr;
}

Err MSReaderAbi::closeFile()
{
    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    HRESULT hr = d->abiReaderWrapper()->closeFile();
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not close file." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str() << endl;
        rrr(kBadParameterError);
    }
    return kNoErr;
}

Err MSReaderAbi::getSampleName(int sampleNumber, QString *sampleName)
{
    Q_ASSERT(sampleName);

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::string sampleNameStd;
    HRESULT hr = d->abiReaderWrapper()->getSampleName(sampleNumber, &sampleNameStd);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not getSampleName." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str() << endl;
        rrr(kBadParameterError);
    }
    *sampleName = QString::fromStdString(sampleNameStd);
    return kNoErr;
}

Err MSReaderAbi::setupSampleList()
{
    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    QString sampleName;
    std::vector<int> samples;

    if (S_OK != d->abiReaderWrapper()->getSampleList(&samples)) {
        debugMs() << "MSReaderAbi: Could not getSampleList." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str() << endl;
        rrr(kBadParameterError);
    }

    for (const int sampleNumber : samples) {
        const Err e = getSampleName(sampleNumber, &sampleName); ree;
        d->m_sampleNumberNames[sampleNumber] = sampleName;
    }

    debugMs() << "samples=" << d->m_sampleNumberNames;
    return kNoErr;
}

Err MSReaderAbi::getBasePeak(point2dList *points) const
{
    Q_UNUSED(points);

    return kFunctionNotImplemented;
}

Err MSReaderAbi::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    points->clear();

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    std::vector<double> times;
    std::vector<double> intensities;

    HRESULT hr = d->abiReaderWrapper()->getTic(&times, &intensities);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not get TIC." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str()
            << endl;
        rrr(kBadParameterError);
    }

    points->reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderAbi::getScanData(long scanNumber, point2dList *points, bool doCentroiding,
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
        = d->abiReaderWrapper()->getScanData(scanNumber, &mzs, doCentroiding, &intensities);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not get Scan Data." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str()
            << endl;
        rrr(kBadParameterError);
    }

    points->reserve(mzs.size());

    for (size_t i = 0; i < mzs.size(); ++i) {
        points->push_back(point2d(mzs[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderAbi::getXICData(const XICWindow &win, point2dList *points, int msLevel) const
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

    const Range<double> mzFilter
        = win.isMzRangeDefined() ? Range<double>(win.mz_start, win.mz_end) : Range<double>();

    const Range<double> timeFilter
        = win.isTimeRangeDefined() ? Range<double>(win.time_start, win.time_end) : Range<double>();

    HRESULT hr = d->abiReaderWrapper()->getXicData(mzFilter, timeFilter, &times, &intensities);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not get XIC Data." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str()
            << endl;
        rrr(kBadParameterError);
    }

    points->reserve(times.size());

    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}

Err MSReaderAbi::getScanInfo(long scanNumber, ScanInfo *obj) const
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

    HRESULT hr = d->abiReaderWrapper()->getScanInfo(scanNumber, &retTimeMinutes, &scanLevel,
        &nativeIdString, &peakMode, &scanMethod);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not get Scan Info." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str()
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

Err MSReaderAbi::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    double dIsolationMass = 0;
    double dMonoIsoMass = 0;
    int nChargeState = 0;
    int nScanNumber = 0;
    std::string nativeIdString;

    if (!d->valid()) {
        rrr(kBadParameterError);
    }

    HRESULT hr = d->abiReaderWrapper()->getScanPrecursorInfo(
        scanNumber, &dIsolationMass, &dMonoIsoMass, &nChargeState, &nScanNumber, &nativeIdString);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not get Scan Precursor Info." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str()
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

Err MSReaderAbi::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Q_ASSERT(totalNumber);
    Q_ASSERT(startScan);
    Q_ASSERT(endScan);

    HRESULT hr = d->abiReaderWrapper()->getNumberOfSpectra(totalNumber, startScan, endScan);
    if (hr != S_OK) {
        rrr(kBadParameterError);
    }
    return kNoErr;
}

Err MSReaderAbi::getFragmentType(long scanNumber, long scanLevel, QString *fragType) const
{
    Q_ASSERT(fragType);

    if (!d->valid()) {
        rrr(kBadParameterError);
    }
    std::string fragTypeStd;
    HRESULT hr = d->abiReaderWrapper()->getFragmentType(scanNumber, scanLevel, &fragTypeStd);
    if (hr != S_OK) {
        debugMs() << "MSReaderAbi: Could not get Fragment Type." << endl;
        debugMs() << "    Error: " << d->abiReaderWrapper()->getLastErrorMessage().c_str()
            << endl;
        rrr(kBadParameterError);
    }
    *fragType = QString::fromStdString(fragTypeStd);
    return kNoErr;
}

Err MSReaderAbi::getChromatograms(QList<ChromatogramInfo> *chroList,
                                  const QString &internalChannelName)
{
    chroList->clear();

    static const QString CHANNEL_NAME_TIC = QStringLiteral("ABI TIC");
    static const QString CHANNEL_ID_TIC = QStringLiteral("tic");
    static const QString CHANNEL_NAME_XIC = QStringLiteral("ABI XIC");
    static const QString CHANNEL_ID_XIC = QStringLiteral("xic");
    static const QString CHANNEL_NAME_ADC = QStringLiteral("ABI ADC");
    static const QString CHANNEL_ID_ADC = QStringLiteral("adc");
    static const QString CHANNEL_NAME_DAD = QStringLiteral("ABI DAD");
    static const QString CHANNEL_ID_DAD = QStringLiteral("dad");

    const ChromatogramCanonicalId canonicalId
        = ChromatogramCanonicalId::fromInternalChannelName(internalChannelName);

    int sampleNo = 0;
    d->abiReaderWrapper()->getCurrentSample(&sampleNo);

    if (canonicalId.isEmpty() || canonicalId.chroType == CHANNEL_ID_XIC) {
        ChromatogramInfo xicChromatogram;
        if (kNoErr == getXICData(XICWindow(), &xicChromatogram.points)) {
            xicChromatogram.channelName = CHANNEL_NAME_XIC;
            xicChromatogram.internalChannelName
                = ChromatogramCanonicalId(CHANNEL_ID_XIC, QString::number(sampleNo)).toString();
            chroList->append(xicChromatogram);
        }
    }

    if (canonicalId.isEmpty() || canonicalId.chroType == CHANNEL_ID_TIC) {
        ChromatogramInfo ticChromatogram;
        if (kNoErr == getTICData(&ticChromatogram.points)) {
            ticChromatogram.channelName = CHANNEL_NAME_TIC;
            ticChromatogram.internalChannelName
                = ChromatogramCanonicalId(CHANNEL_ID_TIC, QString::number(sampleNo)).toString();
            chroList->append(ticChromatogram);
        }
    }

    if (canonicalId.isEmpty() || canonicalId.chroType == CHANNEL_ID_ADC) {
        int adcSamplesCount;
        d->abiReaderWrapper()->adcSamplesCount(&adcSamplesCount);
        std::vector<double> times;
        std::vector<double> intensities;
        for (int i = 0; i < adcSamplesCount; ++i) {
            const ChromatogramCanonicalId id(CHANNEL_ID_ADC, QString::number(sampleNo),
                                    QString::number(i));
            if (canonicalId.isEmpty() || canonicalId == id) {
                times.clear();
                intensities.clear();

                if (kNoErr != d->abiReaderWrapper()->getAdcData(i, &times, &intensities)) {
                    continue;
                }

                ChromatogramInfo adcChromatogram;
                adcChromatogram.points.reserve(times.size());
                for (size_t i = 0; i < times.size(); ++i) {
                    adcChromatogram.points.push_back(point2d(times[i], intensities[i]));
                }

                std::string name;
                d->abiReaderWrapper()->getAdcSampleName(i, &name);
                adcChromatogram.channelName = name.empty()
                    ? (CHANNEL_NAME_ADC + QStringLiteral(" (%1)").arg(i + 1))
                    : QString::fromStdString(name);
                adcChromatogram.internalChannelName = id.toString();
                chroList->append(adcChromatogram);
            }
        }
    }

    bool hasDadData;
    d->abiReaderWrapper()->hasDadData(&hasDadData);
    if (hasDadData && (canonicalId.isEmpty() || canonicalId.chroType == CHANNEL_ID_DAD)) {
        std::vector<double> times;
        std::vector<double> intensities;
        if (kNoErr == d->abiReaderWrapper()->getDadData(&times, &intensities)) {
            ChromatogramInfo dadChromatogram;
            dadChromatogram.points.reserve(times.size());
            for (size_t i = 0; i < times.size(); ++i) {
                dadChromatogram.points.push_back(point2d(times[i], intensities[i]));
            }
            dadChromatogram.channelName = CHANNEL_NAME_DAD;
            dadChromatogram.internalChannelName
                = ChromatogramCanonicalId(CHANNEL_ID_DAD, QString::number(sampleNo)).toString();
            chroList->append(dadChromatogram);
        }
    }

    return kNoErr;
}

Err MSReaderAbi::getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
{
    const bool useABIGetBestScanNumber
        = as::value(QStringLiteral("MSReader/UseABIGetBestScanNumber"), false).toBool();

    if (!useABIGetBestScanNumber) {
        return kFunctionNotImplemented;
    }

    // This API can only handle level one.
    if (msLevel != 1
        && !(msLevel == 2
             && as::value(QStringLiteral("MSReader/UseABIGetBestScanNumberLevel2"), false)
                    .toBool())) {
        return kFunctionNotImplemented;
    }

    Q_ASSERT(scanNumber);

    const HRESULT hr
        = d->abiReaderWrapper()->getBestScanNumber(msLevel, scanTimeMinutes, scanNumber);
    if (hr != S_OK) {
        rrr(kBadParameterError);
    }
    if (*scanNumber == 0) {
        return kFunctionNotImplemented;
    }
    return kNoErr;
}

_MSREADER_END
_PMI_END


#endif //Q_OS_WIN
