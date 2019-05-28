/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
*/

#include "MSReaderAgilent.h"
#include <AdvancedSettings.h>
#include "PlotBase.h"
#include "VendorPathChecker.h"
#include "pmi_common_ms_debug.h"
#include "safearraywrapper.h"
#include <pmi_core_defs.h>

#include <QSet>

#include <functional>

/*
 * 1)
 * To properly compile with .tlb, you must register first:
 * Call RegisterMassHunterDataAccess.bat:

        set REGASMPROG=%SystemRoot%\Microsoft.NET\Framework\v4.0.30319\regasm.exe

        %REGASMPROG% BaseCommon.dll /tlb /u
        %REGASMPROG% BaseDataAccess.dll /tlb /u
        %REGASMPROG% MassSpecDataReader.dll /tlb /u

        %REGASMPROG% BaseCommon.dll /tlb
        %REGASMPROG% BaseDataAccess.dll /tlb
        %REGASMPROG% MassSpecDataReader.dll /tlb

   And use these import in code:
        #import <vendor_api\Agilent\BaseCommon.tlb> raw_interfaces_only, no_namespace, named_guids
        #import <vendor_api\Agilent\BaseDataAccess.tlb> named_guids, raw_interfaces_only, rename_namespace("BDA")
        #import <vendor_api\Agilent\MassSpecDataReader.tlb> named_guids, raw_interfaces_only, no_namespace

 * 2)
 * Afterwards, *.tlh will be generated in the build folder.  Use those with #include instead to avoid using #import
        #include <vendor_api\Agilent\BaseCommon.tlh>
        #include <vendor_api\Agilent\BaseDataAccess.tlh>
        #include <vendor_api\Agilent\MassSpecDataReader.tlh>
 */

#include <vendor_api\Agilent\BaseCommon.tlh>
#include <vendor_api\Agilent\BaseDataAccess.tlh>
#include <vendor_api\Agilent\MassSpecDataReader.tlh>

#include <QFileInfo>

#include <functional>
#include <fstream>
#include <iostream>
#include <limits>
#include <iomanip>

static QString bstrToQString(BSTR bstrVal)
{
    char* p = const_cast<char*>(_com_util::ConvertBSTRToString(bstrVal));
    return QString(p);
}

static QDateTime comDateToQDateTime(DATE date)
{
    SYSTEMTIME st;
    VariantTimeToSystemTime(date, &st);

    const QDate d = QDate(st.wYear, st.wMonth, st.wDay);
    const QTime t = QTime(st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    return QDateTime(d, t);
}

static std::string variantBoolToStr(VARIANT_BOOL b)
{
    return b == VARIANT_FALSE ? "False" : "True";
}


_PMI_BEGIN
_MSREADER_BEGIN

// IMPT! Keep in sync with ChromType enum
static QMap<ChromType, QString> chromTypeDescriptions =
{
    //Note(Yong,2018-05-09): added comments to each of these items
    { ChromType_Unspecified, "Unspecified" },
    { ChromType_Signal, "Signal" },  //unsure what this is, will keep
    { ChromType_InstrumentParameter, "InstrumentParameter" },  //seems like not useful information to show customers
    { ChromType_TotalWavelength, "TotalWavelength" },  // unsure what this is
    { ChromType_ExtractedWavelength, "ExtractedWavelength" },  //uv information?
    { ChromType_TotalIon, "TotalIon" },  //TIC -- we already extract through different API, but doesn't hurt to show.
    { ChromType_BasePeak, "BasePeak" },  //BPI -- we already extract through different API, but doesn't hurt to show.
    { ChromType_AutoTotalIonBasePeak, "AutoTotalIonBasePeak" },  //unsure
    { ChromType_ExtractedIon, "ExtractedIon" }, //XIC -- slow and not needed as chromatagram
    { ChromType_ExtractedCompound, "ExtractedCompound" },  //unsure
    { ChromType_TotalCompound, "TotalCompound" }, //unsure
    { ChromType_NeutralLoss, "NeutralLoss" },  //unsure
    { ChromType_MultipleReactionMode, "MultipleReactionMode" }, //not what we do, can remove
    { ChromType_SelectedIonMonitoring, "SelectedIonMonitoring" } //not what we do, can remove
};

// Adding a list of chromType that we should expose to end users.
static QList<ChromType> chromTypeOfInterest =
{
    ChromType_Signal,
    ChromType_TotalWavelength,
    ChromType_ExtractedWavelength,
    ChromType_TotalIon,
    ChromType_BasePeak,
    ChromType_AutoTotalIonBasePeak,
    ChromType_TotalCompound,
    ChromType_NeutralLoss
};
//TODO: not all of the above list need to be extracted. We mostly care about UV related channels.

static QString makeInternalChannelName(ChromType chromType, DeviceType deviceType,
                                   const QString &deviceName, long deviceOrdinalNumber,
                                   const QString &signalName)
{
    const QString format("ct=%1,dt=%2,dn=%3,dot=%4,sn=%5");
    const QString internalChannelName = format
        .arg(chromType)
        .arg(deviceType)
        .arg(deviceName)
        .arg(deviceOrdinalNumber)
        .arg(signalName);

    return internalChannelName;
}

static QString makeChannelName(ChromType chromType, DeviceType deviceType,
    const QString &deviceName, long deviceOrdinalNumber,
    const QString &signalName, const QString &signalDescription)
{
    const QString format("ChromType:%1, DeviceType:%2, DeviceName:%3, DeviceNumber:%4, "
                         "SignalName:%5, SignalDescription:%6");
    const QString channelName = format.arg(chromTypeDescriptions[chromType])
                                    .arg(deviceType)
                                    .arg(deviceName)
                                    .arg(deviceOrdinalNumber)
                                    .arg(signalName)
                                    .arg(signalDescription);

    return channelName;
}

static bool channelInfoFromChromatogram(BDA::IBDAChromData *chromData, ChromatogramInfo &chromInfo)
{
    if (chromData == nullptr) {
        return false;
    }

    ChromType chromType = ChromType_Unspecified;
    chromData->get_ChromatogramType(&chromType);

    CComBSTR deviceName;
    chromData->get_DeviceName(&deviceName);

    DeviceType deviceType = DeviceType_Unknown;
    chromData->get_DeviceType(&deviceType);

    long deviceOrdinalNumber = -1;
    chromData->get_OrdinalNumber(&deviceOrdinalNumber);

    // Agilent API doesn't reference this function but we need something to make up a unique channel
    // name
    CComBSTR signalName;
    chromData->get_signalName(&signalName);

    CComBSTR signalDescription;
    chromData->get_SignalDescription(&signalDescription);

    chromInfo.channelName
        = makeChannelName(chromType, deviceType, bstrToQString(deviceName), deviceOrdinalNumber,
                          bstrToQString(signalName), bstrToQString(signalDescription));

    chromInfo.internalChannelName
        = makeInternalChannelName(chromType, deviceType, bstrToQString(deviceName),
                                  deviceOrdinalNumber, bstrToQString(signalName));

    return true;
}

static void
parseInternalChannelName(const QString &internalChannelName, ChromType *chromType = nullptr,
                         DeviceType *deviceType = nullptr, QString *deviceName = nullptr,
                         long *deviceOrdinalNumber = nullptr, QString *signalName = nullptr)
{
    bool ok = false;

    const QChar keyValuePairSep = ',';
    const QChar keyValueSep = '=';

    const QStringList pairs = internalChannelName.split(keyValuePairSep);

    const std::function<QString(const QStringList &, int)> valueExtractor
        = [keyValueSep](const QStringList &pairs, int pos) {
              if (pairs.size() > pos) {
                  const QStringList tokens = pairs.at(pos).split(keyValueSep);
                  if (tokens.size() > 1) {
                      return tokens.value(1).trimmed();
                  }
              }
              return QString();
          };

    // Expected key=val positions in string
    const int chromTypePos = 0;
    const int deviceTypePos = 1;
    const int deviceNamePos = 2;
    const int deviceOrdinalNumberPos = 3;
    const int signalNamePos = 4;

    if (chromType != nullptr) {
        const int ct = valueExtractor(pairs, chromTypePos).toInt(&ok);
        if (ok) {
            *chromType = (ChromType)ct;
        }
    }
    if (deviceType != nullptr) {
        const int dt = valueExtractor(pairs, deviceTypePos).toInt(&ok);
        if (ok) {
            *deviceType = (DeviceType)dt;
        }
    }

    if (deviceName != nullptr) {
        *deviceName = valueExtractor(pairs, deviceNamePos);
    }

    if (deviceOrdinalNumber != nullptr) {
        const int dot = valueExtractor(pairs, deviceOrdinalNumberPos).toInt(&ok);
        if (ok) {
            *deviceOrdinalNumber = dot;
        }
    }

    if (signalName != nullptr) {
        *signalName = valueExtractor(pairs, signalNamePos);
    }
}

//! Returns true if 'name' matches 'pattern'
static bool InternalChannelNamesMatch(const QString &pattern, const QString &name)
{
    ChromType chromType1 = ChromType_Unspecified;
    DeviceType deviceType1 = DeviceType_Unknown;
    QString deviceName1;
    long deviceOrdinalNumber1 = -1;
    QString signalName1;

    parseInternalChannelName(pattern, &chromType1, &deviceType1, &deviceName1,
                             &deviceOrdinalNumber1, &signalName1);

    ChromType chromType2 = ChromType_Unspecified;
    DeviceType deviceType2 = DeviceType_Unknown;
    QString deviceName2;
    long deviceOrdinalNumber2 = -1;
    QString signalName2;

    parseInternalChannelName(name, &chromType2, &deviceType2, &deviceName2, &deviceOrdinalNumber2,
                             &signalName2);

    // signal name is optional
    const bool mandatoryFiledsMatch = chromType1 == chromType2 && deviceType1 == deviceType2
        && deviceName1 == deviceName2 && deviceOrdinalNumber1 == deviceOrdinalNumber2;

    if (!mandatoryFiledsMatch) {
        return false;
    }

    if (signalName1.isEmpty()) {
        // An empty signal name matches any signal name, i.e.:
        // "ct=1,dt=14,dn=DAD,dot=1,sn=" matches "ct=1,dt=14,dn=DAD,dot=1"
        // "ct=1,dt=14,dn=DAD,dot=1,sn=C" matches both "ct=1,dt=14,dn=DAD,dot=1" and
        // "ct=1,dt=14,dn=DAD,dot=1,sn="
        return true;
    }

    return signalName1 == signalName2;
}

static bool makeMinMaxRange(double minValue, double maxValue, CComPtr<IMinMaxRange> &range)
{
    HRESULT hr = CoCreateInstance(CLSID_MinMaxRange, NULL, CLSCTX_INPROC_SERVER, IID_IMinMaxRange,
                                  (void **)&range);

    if (FAILED(hr)) {
        criticalMs() << "Failed to create a range (CLSID_MinMaxRange) [" << minValue << ".."
                     << maxValue << "]";
        return false;
    }

    range->put_Min(minValue);
    range->put_Max(maxValue);

    return true;
}

struct MSReaderAgilent::MSReaderAgilentPrivate {
public:
    CComPtr<IMsdrDataReader> msDataReader;
    bool fileOpened = false;
    CComPtr<BDA::IBDAFileInformation> fileInformation;
    CComPtr<BDA::IBDAMSScanFileInformation> scanFileInformation;
    CComQIPtr<INonmsDataReader> nonMsReader;

    MSReaderAgilentPrivate() {}

    virtual ~MSReaderAgilentPrivate() {}

    Q_DISABLE_COPY(MSReaderAgilentPrivate);

    Err getChromotograms(BDA::IBDAChromFilter *chromFilter,
                         std::vector<point2dList> &pointList) const
    {
        pointList.clear();

        SAFEARRAY *chromDataArray = nullptr;
        HRESULT hr = msDataReader->GetChromatogram(chromFilter, &chromDataArray);
        if (FAILED(hr)) {
            return kError;
        }

        // We expect one chromatogram only even though theoretically this function returns a list
        // perhaps, a combination of default settings ensures it's a single chromatogram,
        // specifically chromFilter->put_DoCycleSum(true);
        SafeArrayWrapper<BDA::IBDAChromData *> chromDataList(chromDataArray);
        if (chromDataList.count() == 0) {

            // Is this a normal case?
            warningMs() << "No chromatograms found";
            return kNoErr;
        }

        pointList.resize(chromDataList.count());

        for (int i = chromDataList.lower(); i <= chromDataList.upper(); ++i) {
            BDA::IBDAChromData *chromData = chromDataList.value(i);

            const int idx = i - chromDataList.lower();
            hr = extractDataPointsFromChromatogram(chromData, pointList[idx]);
            if (FAILED(hr)) {
                warningMs() << "Failed to extract points for chromatogram" << idx;
            }

            continue;
        }

        return kNoErr;
    }

    Err getChromatogramsByFilter(ChromType chromType, const QString &internalChannelName,
                                    QList<ChromatogramInfo> &chromatograms) const
    {
        chromatograms.clear();

        CComPtr<BDA::IBDAChromFilter> chromFilter;
        HRESULT hr = CoCreateInstance(BDA::CLSID_BDAChromFilter, NULL, CLSCTX_INPROC_SERVER,
                                      BDA::IID_IBDAChromFilter, (void **)&chromFilter);

        if (S_OK != hr) {
            criticalMs() << "ERROR - CoCreateInstance for Chrom Filter failed hr ="
                         << QString("0x%1").arg(hr, 16);
            return kError;
        }

        Q_ASSERT(chromType != ChromType_Unspecified);
        chromFilter->put_ChromatogramType(chromType);
        chromFilter->put_MSLevelFilter(MSLevel_All);

        const bool applyInternalChannelName = !internalChannelName.isEmpty();

        SAFEARRAY *chromDataArray = nullptr;
        hr = msDataReader->GetChromatogram(chromFilter, &chromDataArray);
        if (FAILED(hr)) {
            // Looks like there is no such data
            return kError;
        }

        SafeArrayWrapper<BDA::IBDAChromData *> chromDataList(chromDataArray);

        for (int i = chromDataList.lower(); i <= chromDataList.upper(); ++i) {
            BDA::IBDAChromData *chromData = chromDataList.value(i);

            ChromatogramInfo chromInfo;
            if (!channelInfoFromChromatogram(chromData, chromInfo)) {
                continue;
            }

            if (applyInternalChannelName
                && !InternalChannelNamesMatch(chromInfo.internalChannelName, internalChannelName)) {
                continue;
            }

            hr = extractDataPointsFromChromatogram(chromData, chromInfo.points);
            if (FAILED(hr)) {
                continue;
            }

            // Do not include empty chromatograms
            if (chromInfo.points.size() > 0) {
                chromatograms.push_back(chromInfo);
            }
        }

        return kNoErr;
    }

    Err getNonMsData(DeviceType deviceType, QList<ChromatogramInfo> &chroList) const
    {
        return getNonMsData(deviceType, QString(), chroList);
    }

    Err getNonMsDataPerDevice(IDeviceInfo *deviceInfo, const QString &internalChannelName,
                              QList<ChromatogramInfo> &chroList) const
    {
        chroList.clear();
        SAFEARRAY *signalInfoArray = nullptr;

        // StoredDataType_Chromatograms apparenty contains UV and FL data
        // StoredDataType_InstrumentCurves, StoredDataType_MassSpectra, StoredDataType_Spectra
        // contains no data in the test files
        HRESULT hr = nonMsReader->GetSignalInfo(deviceInfo, StoredDataType_Chromatograms,
                                                &signalInfoArray);
        if (FAILED(hr)) {
            return kError;
        }

        SafeArrayWrapper<ISignalInfo *> signalInfoList(signalInfoArray);
        if (signalInfoList.count() <= 0) {
            return kNoErr;
        }

        for (int j = signalInfoList.lower(); j <= signalInfoList.upper(); ++j) {

            CComPtr<BDA::IBDAChromData> chromData;

            hr = nonMsReader->GetSignal(signalInfoList.value(j), &chromData);
            if (FAILED(hr)) {
                continue;
            }

            ChromatogramInfo chromInfo;
            if (!channelInfoFromChromatogram(chromData, chromInfo)) {
                continue;
            }

            if (!internalChannelName.isEmpty()
                && !InternalChannelNamesMatch(internalChannelName, chromInfo.internalChannelName)) {
                // This is not the expected chromatogram
                continue;
            }

            hr = extractDataPointsFromChromatogram(chromData, chromInfo.points);
            if (FAILED(hr)) {
                continue;
            }

            // Do not include empty lists
            if (chromInfo.points.size() > 0) {
                chroList.push_back(chromInfo);
            }
        }

        return kNoErr;
    }

    Err getNonMsData(DeviceType deviceType, const QString &internalChannelName,
                     QList<ChromatogramInfo> &chroList) const
    {
        ChromType chromType = ChromType_Unspecified;

        if (!internalChannelName.isEmpty()) {
            DeviceType argDeviceType = DeviceType_Unknown;
            parseInternalChannelName(internalChannelName, &chromType, &argDeviceType);

            // Consistency check
            Q_ASSERT(argDeviceType == deviceType);
        }

        chroList.clear();

        HRESULT hr = S_OK;
        SAFEARRAY *nonMSDeviceArray = nullptr;

        hr = nonMsReader->GetNonmsDevices(&nonMSDeviceArray);
        if (FAILED(hr)) {
            return kError;
        }
        SafeArrayWrapper<IDeviceInfo *> nonMSDeviceList(nonMSDeviceArray);

        if (nonMSDeviceList.count() == 0) {
            return kError;
        }

        DeviceType devType = DeviceType_Unknown;
        for (int i = nonMSDeviceList.lower(); i <= nonMSDeviceList.upper(); ++i) {

            IDeviceInfo *deviceInfo = nonMSDeviceList.value(i);
            deviceInfo->get_DeviceType(&devType);

            // A minor optimization
            if (devType != deviceType) {
                continue;
            }

            QList<ChromatogramInfo> chromInfoList;
            Err e = getNonMsDataPerDevice(deviceInfo, internalChannelName, chromInfoList); ree;
            chroList += chromInfoList;
        }

        return kNoErr;
    }

    Err getNonMsData(const QString &internalChannelName, QList<ChromatogramInfo> &chroList) const
    {
        if (internalChannelName.isEmpty()) {
            criticalMs() << "internalChannelName must not be empty";
            return kError;
        }

        ChromType chromType = ChromType_Unspecified;
        DeviceType deviceType = DeviceType_Unknown;
        parseInternalChannelName(internalChannelName, &chromType, &deviceType);

        return getNonMsData(deviceType, internalChannelName, chroList);
    }

    static Err extractDataPointsFromChromatogram(BDA::IBDAChromData *chromData, point2dList &points)
    {
        points.clear();

        if (chromData == nullptr) {
            return kError;
        }

        HRESULT hr = S_OK;

        // Perhaps, can be omitted
        long dataPoints = 0;
        hr = chromData->get_TotalDataPoints(&dataPoints);
        if (FAILED(hr)) {
            return kError;
        }

        if (dataPoints <= 0) {
            return kNoErr;
        }

        SAFEARRAY *timeArray = nullptr;
        hr = chromData->get_XArray(&timeArray);
        if (FAILED(hr)) {
            return kError;
        }
        SafeArrayWrapper<double> times(timeArray);

        SAFEARRAY *intensityArray = NULL;
        hr = chromData->get_YArray(&intensityArray);
        if (FAILED(hr)) {
            return kError;
        }
        SafeArrayWrapper<float> intensities(intensityArray);

        const int timesCount = times.count();
        const int intensityCount = intensities.count();
        if (timesCount != intensityCount || timesCount != dataPoints) {
            const QString errMsg = QString("Inconsistent times.count(%1), intensities.count(%2), "
                                           "and dataPoints(%3) detected")
                                       .arg(timesCount)
                                       .arg(intensityCount)
                                       .arg(dataPoints);
            warningMs() << errMsg;
        }

        points.reserve(timesCount);

        for (int i = times.lower(); i <= times.upper(); ++i) {
            points.push_back(QPointF(times.value(i), intensities.value(i)));
        }

        return kNoErr;
    }

    Err getBasicScanInfo(long scanNumber, ScanInfo& scanInfo) const
    {
        const long scanNumberZeroBased = scanNumber - 1;

        HRESULT hr = S_OK;
        CComPtr<IMSScanRecord> scanRecord;
        hr = msDataReader->GetScanRecord(scanNumberZeroBased, &scanRecord);
        if (FAILED(hr)) {
            return kError;
        }

        scanInfo.clear();

        long scanId = -1;
        scanRecord->get_ScanID(&scanId);

        long timeSegment = -1;
        scanRecord->get_TimeSegment(&timeSegment);

        double retentionTime = 0.0;
        scanRecord->get_retentionTime(&retentionTime);
        scanInfo.retTimeMinutes = retentionTime; // / 60.0?

        double tic = 0.0;
        scanRecord->get_Tic(&tic);

        MSLevel msLevel = MSLevel_All;
        scanRecord->get_MSLevel(&msLevel);
        scanInfo.scanLevel = static_cast<int>(msLevel);

        MSScanType msScantype = MSScanType_Unspecified;
        scanRecord->get_MSScanType(&msScantype);

        // pwiz has ScanMethodUnknown but not sure if that's a right method
        // TODO ?
        scanInfo.scanMethod = ScanMethodFullScan;

        scanInfo.nativeId = QString("scanId=%1").arg(scanId);

        double basePeakMZ = 0.0;
        scanRecord->get_BasePeakMZ(&basePeakMZ);

        double basePeakIntensity = 0.0;
        scanRecord->get_BasePeakIntensity(&basePeakIntensity);

        return kNoErr;
    }

    Err getPeakModeScanInfo(long scanNumber, PeakPickingMode *peakMode) const
    {
        const long scanNumberZeroBased = scanNumber - 1;

        // Find out if the scan is Peak/Profile/Mixed
        CComPtr<BDA::IBDASpecData> specData;
        HRESULT hr = msDataReader->GetSpectrum_8(scanNumberZeroBased, nullptr, nullptr,
                                                 DesiredMSStorageType_ProfileElsePeak, &specData);
        if (FAILED(hr)) {
            return kError;
        }

        Q_ASSERT(peakMode != nullptr);

        *peakMode = PeakPickingModeUnknown;
        MSStorageMode msStorageMode = MSStorageMode_Unspecified;
        specData->get_MSStorageMode(&msStorageMode);

        switch (msStorageMode) {
        case MSStorageMode_Unspecified:
            *peakMode = PeakPickingModeUnknown;
            break;

        case MSStorageMode_PeakDetectedSpectrum:
            *peakMode = PeakPickingCentroid;
            break;

        case MSStorageMode_Mixed: // fall-through
        case MSStorageMode_ProfileSpectrum:
            *peakMode = PeakPickingProfile;
            break;

        default:
            return kError;
        }

        return kNoErr;
    }

    Err findParentId(long scanNumber, long *parentScanNumber, QString *parentId) const
    {
        // 1-based scan number and it cannot be the first scan either
        Q_ASSERT(scanNumber > 1);
        *parentScanNumber = -1;

        ScanInfo scanInfo;
        Err e = getBasicScanInfo(scanNumber, scanInfo); ree;
        const int expectedParentScanLevel = scanInfo.scanLevel - 1;
        if (expectedParentScanLevel <= 0) {
            criticalMs() << "No parent scan exist for the given scan level" << scanInfo.scanLevel;
            return kBadParameterError;
        }

        // start looking from the previous scan number
        for (int i = scanNumber - 1; i > 0; --i) {

            if (kNoErr != getBasicScanInfo(i, scanInfo)) {
                // is this a problem?
                continue;
            }

            if (scanInfo.scanLevel != expectedParentScanLevel) {
                continue;
            }

            // The parent ID has been found
            *parentId = scanInfo.nativeId;

            // 1-based index
            *parentScanNumber = i;

            return kNoErr;
        }

        // The parent ID has not been found
        criticalMs() << "Failed to find parent ID for scan " << scanNumber;
        return kError;
    }
};

MSReaderAgilent::MSReaderAgilent()
    : d(new MSReaderAgilentPrivate())
{ }

MSReaderAgilent::~MSReaderAgilent()
{
}

bool MSReaderAgilent::initialize()
{
    HRESULT hr = S_OK;

    try
    {
        hr = CoCreateInstance(CLSID_MassSpecDataReader, NULL,
            CLSCTX_INPROC_SERVER,
            IID_IMsdrDataReader,
            reinterpret_cast<void**>(&d->msDataReader));

        if (S_OK != hr) {
            criticalMs() << "ERROR - Could not create instance for Agilent, hr ="
                         << QString("0x%1").arg(hr, 16);
            throw hr;
        }
    }
    catch (_com_error& e) {
        hr = e.Error();
    }
    catch (HRESULT& hr) {
        _com_error e(hr);
        hr = e.Error();
    }
    catch (...) {
        hr = E_UNEXPECTED;
    }

    const bool success = SUCCEEDED(hr);
    debugMs() << "MSReaderAgilent::initialize success=" << success;

    return success;
}

bool MSReaderAgilent::isInitialized() const
{
    const bool init_val = d->msDataReader != nullptr;
    //debugMs() << "MSReaderAgilent::isInitialized() init_val = " << init_val;
    return init_val;
}

static std::string dumpIBDAFileInformation(CComPtr<BDA::IBDAFileInformation> &bdaFileInformation)
{
    std::stringstream f;

    CComBSTR dataFilename;
    bdaFileInformation->get_DataFileName(&dataFilename);

    DATE acquisitionTime;
    bdaFileInformation->get_AcquisitionTime(&acquisitionTime);

    VARIANT_BOOL msDataPresent = VARIANT_FALSE;
    bdaFileInformation->IsMSDataPresent(&msDataPresent);

    CComPtr<BDA::IBDAMSScanFileInformation> scanFileInformation;
    bdaFileInformation->get_MSScanFileInformation(&scanFileInformation);

    MeasurementType measurementType = MeasurementType_Unknown;
    bdaFileInformation->get_MeasurementType(&measurementType);

    SeparationTechnique separationTechnique;
    bdaFileInformation->get_SeparationTechnique(&separationTechnique);

    IRMStatus irmStatus;
    bdaFileInformation->get_IRMStatus(&irmStatus);

    VARIANT_BOOL uvSpectralDataPresent = VARIANT_FALSE;
    bdaFileInformation->IsUVSpectralDataPresent(&uvSpectralDataPresent);

    // getDeviceName - skipped

    f << "IBDAFileInformation: " << bstrToQString(dataFilename).toStdString() << std::endl;
    f << "acquisitionTime: "
      << comDateToQDateTime(acquisitionTime).toString("yyyy-MM-dd HH:mm:ss").toStdString()
      << std::endl;
    f << "msDataPresent: " << variantBoolToStr(msDataPresent) << std::endl;
    f << "measurementType: " << measurementType << std::endl;
    f << "separationTechnique: " << separationTechnique << std::endl;
    f << "irmStatus: " << irmStatus << std::endl;
    f << "uvSpectralDataPresent: " << variantBoolToStr(uvSpectralDataPresent) << std::endl;

    return f.str();
}

static std::string
dumpIBDAMSScanFileInformation(CComPtr<BDA::IBDAMSScanFileInformation> &scanFileInformation)
{
    std::stringstream f;

    VARIANT_BOOL fileHasMassSpectralData = VARIANT_FALSE;
    scanFileInformation->get_FileHasMassSpectralData(&fileHasMassSpectralData);
    double mzScanRangeMinimum = 0.0;
    scanFileInformation->get_MzScanRangeMinimum(&mzScanRangeMinimum);
    double mzScanRangeMaximum = 0.0;
    scanFileInformation->get_MzScanRangeMaximum(&mzScanRangeMaximum);
    MSScanType scanTypes = MSScanType_Unspecified;
    scanFileInformation->get_ScanTypes(&scanTypes);
    MSStorageMode spectraFormat = MSStorageMode_Unspecified;
    scanFileInformation->get_SpectraFormat(&spectraFormat);
    IonizationMode ionMode;
    scanFileInformation->get_IonModes(&ionMode);
    DeviceType deviceType = DeviceType_Unknown;
    scanFileInformation->get_DeviceType(&deviceType);
    MSLevel msLevel = MSLevel_All;
    scanFileInformation->get_MSLevel(&msLevel);

    SAFEARRAY* fragmentorVoltageArray = nullptr;
    scanFileInformation->get_FragmentorVoltage(&fragmentorVoltageArray);
    SafeArrayWrapper<double> fragmentorVoltages(fragmentorVoltageArray);

    SAFEARRAY* collisionEnergy = nullptr;
    scanFileInformation->get_CollisionEnergy(&collisionEnergy);
    SafeArrayWrapper<double> collisionEnergies(collisionEnergy);

    IonPolarity ionPolarity;
    scanFileInformation->get_IonPolarity(&ionPolarity);

    SAFEARRAY* mrmTransitions;
    scanFileInformation->get_MRMTransitions(&mrmTransitions);

    SAFEARRAY* simIonsArray;
    scanFileInformation->get_SIMIons(&simIonsArray);
    SafeArrayWrapper<double> simIons(simIonsArray);

    SAFEARRAY* scanMethodNumbersArray;
    scanFileInformation->get_ScanMethodNumbers(&scanMethodNumbersArray);
    SafeArrayWrapper<int> scanMethodNumbers(scanMethodNumbersArray);

    long long totalScansPresent = 0;
    scanFileInformation->get_TotalScansPresent(&totalScansPresent);

    VARIANT_BOOL fixedCycleLengthDataPresent = VARIANT_FALSE;
    scanFileInformation->IsFixedCycleLengthDataPresent(&fixedCycleLengthDataPresent);

    CComPtr<BDA::IBDAMSScanTypeInformation> scanTypeInformation;
    MSScanType scanType = MSScanType_ProductIon;
    scanFileInformation->GetMSScanTypeInformation(scanType, &scanTypeInformation);

    SAFEARRAY* msScanTypeInformation;
    scanFileInformation->GetMSScanTypeInformation_2(&msScanTypeInformation);

    long scanTypesInformationCount = 0;
    scanFileInformation->get_ScanTypesInformationCount(&scanTypesInformationCount);

    IRange* massRange;
    scanFileInformation->get_MassRange(&massRange);

    VARIANT_BOOL multipleSpectraPerScanPresent = VARIANT_FALSE;
    scanFileInformation->IsMultipleSpectraPerScanPresent(&multipleSpectraPerScanPresent);

    f << "*********************************************************************" << std::endl;
    f << "IBDAMSScanFileInformation" << std::endl;
    f << "*********************************************************************" << std::endl;
    f << "fileHasMassSpectralData: " << variantBoolToStr(fileHasMassSpectralData) << std::endl;
    f << "mzScanRangeMinimum: " << mzScanRangeMinimum << std::endl;
    f << "mzScanRangeMaximum: " << mzScanRangeMaximum << std::endl;
    f << "scanTypes:" << scanTypes << std::endl;
    f << "spectraFormat:" << spectraFormat << std::endl;
    f << "ionMode:" << ionMode << std::endl;
    f << "deviceType:" << deviceType << std::endl;
    f << "msLevel:" << msLevel << std::endl;
    f << "fragmentorVoltageArray:" << fragmentorVoltages.lower() << ".." << fragmentorVoltages.upper() << std::endl;
    f << "collisionEnergyArray:" << collisionEnergies.lower() << ".." << collisionEnergies.upper() << std::endl;
    f << "ionPolarity:" << ionPolarity << std::endl;
    f << "simIonsArray:" << simIons.lower() << ".." << simIons.upper() << std::endl;
    f << "scanMethodNumbersArray:" << scanMethodNumbers.lower() << ".." << scanMethodNumbers.upper() << std::endl;
    f << "totalScansPresent:" << totalScansPresent << std::endl;
    f << "fixedCycleLengthDataPresent:" << variantBoolToStr(fixedCycleLengthDataPresent) << std::endl;
    f << "scanTypesInformationCount:" << scanTypesInformationCount << std::endl;
    f << "multipleSpectraPerScanPresent:" << variantBoolToStr(multipleSpectraPerScanPresent) << std::endl;

    return f.str();
}

static std::string dumpIBDASpecData(CComPtr<BDA::IBDASpecData>& specData)
{
    std::stringstream f;

    long spectrumDataPoints = 0;
    specData->get_TotalDataPoints(&spectrumDataPoints);

    SpecType spectrumType = SpecType_Unspecified;
    specData->get_SpectrumType(&spectrumType);

    long totalScanCount = 0;
    specData->get_TotalScanCount(&totalScanCount);

    double abundanceLimit = 0.0;
    specData->get_AbundanceLimit(&abundanceLimit);

    double collisionEnergy = 0.0;
    specData->get_CollisionEnergy(&collisionEnergy);

    double fragmentorVoltage = 0.0;
    specData->get_FragmentorVoltage(&fragmentorVoltage);

    IonizationMode ionizationMode = IonizationMode_Unspecified;
    specData->get_IonizationMode(&ionizationMode);

    IonPolarity ionPolarity = IonPolarity_Unassigned;
    specData->get_IonPolarity(&ionPolarity);

    MSScanType msScanType = MSScanType_Unspecified;
    specData->get_MSScanType(&msScanType);

    MSLevel msLevelInfo = MSLevel_All;
    specData->get_MSLevelInfo(&msLevelInfo);

    MSStorageMode msStorageMode = MSStorageMode_Unspecified;
    specData->get_MSStorageMode(&msStorageMode);

    double threshold = 0.0;
    specData->get_Threshold(&threshold);

    double samplingPeriod = 0.0;
    specData->get_SamplingPeriod(&samplingPeriod);

    VARIANT_BOOL isDataInMassUnit;
    specData->get_IsDataInMassUnit(&isDataInMassUnit);

    VARIANT_BOOL getPrecursorCharge;
    long precursorCharge = 0;
    specData->GetPrecursorCharge(&precursorCharge, &getPrecursorCharge);

    VARIANT_BOOL getPrecursorIntensity;
    double precursorIntensity = 0.0;
    specData->GetPrecursorIntensity(&precursorIntensity, &getPrecursorIntensity);

    long scanId = 0;
    specData->get_ScanId(&scanId);

    long parentScanId = 0;
    specData->get_ParentScanId(&parentScanId);

    DataUnit xUnit;
    DataValueType xValueType;
    specData->GetXAxisInfoSpec(&xUnit, &xValueType);

    DataUnit yUnit;
    DataValueType yValueType;
    specData->GetYAxisInfoSpec(&yUnit, &yValueType);

    SAFEARRAY* precusrsorIonArray = nullptr;
    long precusrsorIonArraySize = 0;
    specData->GetPrecursorIon(&precusrsorIonArraySize, &precusrsorIonArray);
    SafeArrayWrapper<double> precusrsorIonList(precusrsorIonArray);

    f << "*********************************************************************" << std::endl;
    f << "IBDASpecData" << std::endl;
    f << "*********************************************************************" << std::endl;
    f << "scanId: " << scanId << std::endl;
    //f << "DeviceType: " << deviceType << std::endl;
    f << "parentScanId: " << parentScanId << std::endl;
    f << "totalScanCount: " << totalScanCount << std::endl;
    f << "spectrumDataPoints: " << spectrumDataPoints << std::endl;
    f << "spectrumType: " << spectrumType << std::endl;
    f << "abundanceLimit: " << abundanceLimit << std::endl;
    f << "collisionEnergy: " << collisionEnergy << std::endl;
    f << "fragmentorVoltage: " << fragmentorVoltage << std::endl;
    f << "ionizationMode: " << ionizationMode << std::endl;
    f << "ionPolarity: " << ionPolarity << std::endl;
    f << "msScanType: " << msScanType << std::endl;
    f << "msLevelInfo: " << msLevelInfo << std::endl;
    f << "msStorageMode: " << msStorageMode << std::endl;
    f << "threshold: " << threshold << std::endl;
    f << "samplingPeriod: " << samplingPeriod << std::endl;
    f << "precursorCharge: " << precursorCharge << std::endl;
    f << "precursorIntensity: " << precursorIntensity << std::endl;
    f << "xValueType: " << xValueType << std::endl;
    f << "yValueType: " << yValueType << std::endl;

    SAFEARRAY* psaX = nullptr;
    specData->get_XArray(&psaX);
    SafeArrayWrapper<double> xvals(psaX);

    SAFEARRAY* psaY = nullptr;
    specData->get_YArray(&psaY);
    SafeArrayWrapper<float> yvals(psaY);

    const bool peakSpectra = false;
    point2dList points;

    if (peakSpectra) {
        points.reserve(spectrumDataPoints);

        double x = 0.0;
        float y = 0.0;

        for (int i = xvals.lower(); i <= xvals.upper(); ++i) {
            x = xvals.value(i);
            y = yvals.value(i);
            points.push_back(QPointF(x, static_cast<double>(y)));
        }
    }
    else {
        MSReaderAgilent::removeZeroIntensities(xvals, yvals, &points);
    }

    f << "XY(" << xvals.count() << "," << points.size() << "): ";
    for (const auto& p : points) {
        f << QString("{%1,%2},").arg(p.x(), 0, 'f', 6).arg(p.y(), 0, 'f', 6).toStdString();
    }
    f << std::endl;

    QString zz;
    for (int i = precusrsorIonList.lower(); i <= precusrsorIonList.upper(); ++i) {
        zz += QString("%1,").arg(precusrsorIonList.value(i));
    }
    f << "precusrsorIonArraySize: " << precusrsorIonArraySize << std::endl;
    f << "PrecursorIonList: " << zz.toStdString() << std::endl;

    return f.str();
}

static std::string dumpIMSScanRecord(CComPtr<IMSScanRecord>& scanRecord)
{
    std::stringstream f;

    long scanId = 0;
    scanRecord->get_ScanID(&scanId);

    long timeSegment = -1;
    scanRecord->get_TimeSegment(&timeSegment);

    double retentionTime = 0.0;
    scanRecord->get_retentionTime(&retentionTime);

    double tic = 0.0;
    scanRecord->get_Tic(&tic);

    MSLevel msLevel = MSLevel_All;
    scanRecord->get_MSLevel(&msLevel);

    MSScanType msScantype = MSScanType_Unspecified;
    scanRecord->get_MSScanType(&msScantype);

    double basePeakMZ = 0.0;
    scanRecord->get_BasePeakMZ(&basePeakMZ);

    double basePeakIntensity = 0.0;
    scanRecord->get_BasePeakIntensity(&basePeakIntensity);

    IonizationMode ionizationMode;
    scanRecord->get_IonizationMode(&ionizationMode);

    IonPolarity ionPolarity;
    scanRecord->get_ionPolarity(&ionPolarity);

    double mzOfInterest = 0.0;
    scanRecord->get_MZOfInterest(&mzOfInterest);

    f << "*********************************************************************" << std::endl;
    f << "IMSScanRecord" << std::endl;
    f << "*********************************************************************" << std::endl;
    f << "scanId: " << scanId << std::endl;
    f << "timeSegment: " << timeSegment << std::endl;
    f << "retentionTime: " << retentionTime << std::endl;
    f << "tic: " << tic << std::endl;
    f << "msLevel: " << msLevel << std::endl;
    f << "msScantype: " << msScantype << std::endl;
    f << "basePeakMZ: " << basePeakMZ << std::endl;
    f << "basePeakIntensity: " << basePeakIntensity << std::endl;
    f << "ionizationMode: " << ionizationMode << std::endl;
    f << "ionPolarity: " << ionPolarity << std::endl;
    f << "mzOfInterest: " << mzOfInterest << std::endl;

    return f.str();
}

static std::string dumpIBDAChromData(CComPtr<BDA::IBDAChromData> &chromData,
                                     CComPtr<IMsdrDataReader> &msDataReader)
{
    std::stringstream f;

    long totalDataPoints = 0;
    chromData->get_TotalDataPoints(&totalDataPoints);

    ChromType chromatogramType;
    chromData->get_ChromatogramType(&chromatogramType);

    VARIANT_BOOL mzRegionsWereExcluded;
    chromData->get_MzRegionsWereExcluded(&mzRegionsWereExcluded);

    double abundanceLimit = 0.0;
    chromData->get_AbundanceLimit(&abundanceLimit);

    double collisionEnergy = 0.0;
    chromData->get_CollisionEnergy(&collisionEnergy);

    double fragmentorVoltage = 0.0;
    chromData->get_FragmentorVoltage(&fragmentorVoltage);

    IonizationMode ionizationMode = IonizationMode_Unspecified;
    chromData->get_IonizationMode(&ionizationMode);

    IonPolarity ionPolarity = IonPolarity_Unassigned;
    chromData->get_IonPolarity(&ionPolarity);

    MSScanType msScanType = MSScanType_Unspecified;
    chromData->get_MSScanType(&msScanType);

    MSLevel msLevelInfo = MSLevel_All;
    chromData->get_MSLevelInfo(&msLevelInfo);

    MSStorageMode msStorageMode = MSStorageMode_Unspecified;
    chromData->get_MSStorageMode(&msStorageMode);

    double threshold = 0.0;
    chromData->get_Threshold(&threshold);

    double samplingPeriod = 0.0;
    chromData->get_SamplingPeriod(&samplingPeriod);

    CComBSTR deviceName;
    chromData->get_DeviceName(&deviceName);

    DeviceType deviceType;
    chromData->get_DeviceType(&deviceType);

    long ordinalNumber = 0;
    chromData->get_OrdinalNumber(&ordinalNumber);

    DataUnit xChromunit;
    DataValueType xChromvalueType;
    chromData->GetXAxisInfoChrom(&xChromunit, &xChromvalueType);

    DataUnit yChromunit;
    DataValueType yChromvalueType;
    chromData->GetYAxisInfoChrom(&yChromunit, &yChromvalueType);

    VARIANT_BOOL isPrimaryMrm;
    chromData->get_IsPrimaryMrm(&isPrimaryMrm);


    f << "*********************************************************************" << std::endl;
    f << "IBDAChromData" << std::endl;
    f << "*********************************************************************" << std::endl;

    f << "chromatogramType: " << chromatogramType << std::endl;
    f << "totalDataPoints: " << totalDataPoints << std::endl;
    f << "mzRegionsWereExcluded: " << variantBoolToStr(mzRegionsWereExcluded) << std::endl;
    f << "abundanceLimit: " << abundanceLimit << std::endl;
    f << "collisionEnergy: " << collisionEnergy << std::endl;
    f << "fragmentorVoltage: " << fragmentorVoltage << std::endl;
    f << "ionizationMode: " << ionizationMode << std::endl;
    f << "ionPolarity: " << ionPolarity << std::endl;
    f << "msScanType: " << msScanType << std::endl;
    f << "msLevelInfo: " << msLevelInfo << std::endl;
    f << "msStorageMode: " << msStorageMode << std::endl;
    f << "threshold: " << threshold << std::endl;
    f << "samplingPeriod: " << samplingPeriod << std::endl;
    f << "deviceName: " << bstrToQString(deviceName).toStdString() << std::endl;
    f << "deviceType: " << deviceType << std::endl;
    f << "ordinalNumber: " << ordinalNumber << std::endl;
    f << "xChromunit: " << xChromunit << std::endl;
    f << "xChromvalueType: " << xChromvalueType << std::endl;
    f << "yChromunit: " << yChromunit << std::endl;
    f << "yChromvalueType: " << yChromvalueType << std::endl;
    f << "isPrimaryMrm: " << variantBoolToStr(isPrimaryMrm) << std::endl;

    SAFEARRAY* xSafeArray = nullptr;
    HRESULT hr1 = chromData->get_XArray(&xSafeArray);
    SafeArrayWrapper<double> xvals(xSafeArray);

    SAFEARRAY* ySafeArray = nullptr;
    HRESULT hr2 = chromData->get_YArray(&ySafeArray);
    SafeArrayWrapper<float> yvals(ySafeArray);

    long actualXVals = 0;

    for (long i = xvals.lower(); i <= xvals.upper(); ++i) {
        CComPtr<BDA::IBDASpecData> specData;

        // TODO Full profile or just peaks?
        msDataReader->GetSpectrum_8(i, nullptr, nullptr, DesiredMSStorageType_Peak, &specData);

        long numPeaks = 0;
        specData->get_TotalDataPoints(&numPeaks);

        if (numPeaks > 0) {
            ++actualXVals;
        }

        SAFEARRAY* psaY = nullptr;
        specData->get_YArray(&psaY);
        SafeArrayWrapper<float> yy(psaY);

        float totalY = yy.sum();

        //f << "profile for scan #" << (i + 1) << ", datapoints: " << numPeaks << ": ";

        const int idx = i; //+ xvals.lower();
        /*f << "{" << QString("%1").arg(xvals.value(idx), 0, 'f', 6).toStdString()
            << "," << QString("%1").arg(yvals.value(idx), 0, 'f', 6).toStdString()
            << "," << QString("%1").arg(totalY, 0, 'f', 6).toStdString() << "}," << std::endl;*/

        f << "" << QString("%1").arg(xvals.value(idx), 0, 'f', 6).toStdString()
            << "," << QString("%1").arg(yvals.value(idx), 0, 'f', 6).toStdString()
            << "," << QString("%1").arg(totalY, 0, 'f', 6).toStdString() << "" << std::endl;
    }

    f << "Actual x-points: " << actualXVals;

    f << std::endl;

    return f.str();
}

void MSReaderAgilent::dbgDumpToFile(const QString& path)
{
    const QString filename = QString("getScanData-scan%1.txt").arg(1);
    std::ofstream f(filename.toStdString());

    // IBDAFileInformation

    CComPtr<BDA::IBDAFileInformation> bdaFileInformation;
    d->msDataReader->get_FileInformation(&bdaFileInformation);
    f << dumpIBDAFileInformation(bdaFileInformation);

    CComPtr<BDA::IBDAMSScanFileInformation> scanFileInformation;
    bdaFileInformation->get_MSScanFileInformation(&scanFileInformation);
    f << dumpIBDAMSScanFileInformation(scanFileInformation);

    // 0 - based scan number
    int scanNumber = 0;

    for (; scanNumber < 1; ++scanNumber) {
        CComPtr<BDA::IBDASpecFilter> specFilter;
        HRESULT hr = CoCreateInstance(BDA::CLSID_BDASpecFilter, NULL,
            CLSCTX_INPROC_SERVER,
            BDA::IID_IBDASpecFilter,
            (void**)&specFilter);

        /*VARIANT_BOOL extractByCycle = VARIANT_TRUE;
        specFilter->put_ExtractByCycle(extractByCycle);*/

        SpecType specType = SpecType_MassSpectrum;
        specFilter->put_SpectrumType(specType);

        CComPtr<BDA::IBDASpecData> specData;
        //SAFEARRAY* specDataArray;
        /*d->msDataReader->GetSpectrum_5(specFilter, &specDataArray);
        SafeArrayWrapper<BDA::IBDASpecData*> specDataList(specDataArray);

        if (specDataList.count() == 0) {
            continue;
        }*/

        //d->msDataReader->GetSpectrum_6(scanNumber, nullptr, nullptr, &specData);
        //d->msDataReader->GetSpectrum_7(0.03877, MSScanType_All, IonPolarity_Positive, IonizationMode_Esi, nullptr, VARIANT_FALSE, &specData);
        d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr, DesiredMSStorageType_Profile, &specData);
        //d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr, DesiredMSStorageType_PeakElseProfile, &specData);
        //d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr, DesiredMSStorageType_ProfileElsePeak, &specData);
        //d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr, DesiredMSStorageType_Unspecified, &specData);

        /*for (int i = specDataList.lower(); i <= specDataList.upper(); ++i) {
            specData = specDataList.value(i);
            f << dumpIBDASpecData(specData);
        }*/

        f << dumpIBDASpecData(specData);
    }

    CComPtr<IMSScanRecord> scanRecord;
    d->msDataReader->GetScanRecord(scanNumber, &scanRecord);
    f << dumpIMSScanRecord(scanRecord);

    CComPtr<BDA::IBDAChromData> chromData;
    d->msDataReader->GetTIC(&chromData);
    f << dumpIBDAChromData(chromData, d->msDataReader);

    f.close();
}

void MSReaderAgilent::dbgDumpAllScans(const QString& path)
{
    std::stringstream f;
    f << std::setprecision(24);

    CComPtr<BDA::IBDAFileInformation> bdaFileInformation;
    d->msDataReader->get_FileInformation(&bdaFileInformation);

    long totalNumber = 0;
    long startScan = 0;
    long endScan = 0;
    getNumberOfSpectra(&totalNumber, &startScan, &endScan);

    pmi::point2dList points;
    for (int i = startScan; i <= endScan; ++i) {
        if (kNoErr != getScanData(i, &points)) {
            continue;
        }

        f << "scan #" << i << std::endl;
        for (auto p : points) {
            f << "" << p.x() << "," << p.y() << std::endl;
        }
    }

    const QString filename = QString("getScanData-scan%1.txt").arg(1);
    std::ofstream ff(filename.toStdString());
    ff << f.str();
    ff.close();
}

void MSReaderAgilent::dbgTest()
{
    return;
    const DeviceType expectedDeviceType = DeviceType_FluorescenceDetector;
    DeviceType deviceType = DeviceType_Unknown;
    d->scanFileInformation->get_DeviceType(&deviceType);

    // chromData and specData

    long totalNumber = 0;
    long startScan = 0;
    long endScan = 0;

    getNumberOfSpectra(&totalNumber, &startScan, &endScan);
    bool all_device_types_equal = true;
    const int output_every = 500;

    for (int scanNumber = startScan; scanNumber <= endScan; scanNumber += 1) {

        HRESULT hr = S_OK;
        CComPtr<BDA::IBDASpecData> specData;

        d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr, DesiredMSStorageType_Profile,
                                       &specData);

        DeviceType dt = DeviceType_Unknown;
        specData->get_DeviceType(&dt);

        if (dt != deviceType) {

            all_device_types_equal = false;

            if (dt == DeviceType_FluorescenceDetector) {
                int great = 1;
            }
        }

        if (scanNumber % output_every == 0) {
            debugMs() << "Processed" << scanNumber << "out of" << endScan;
        }
    }
}

bool MSReaderAgilent::canOpen(const QString &filename) const
{
    return VendorPathChecker::formatAgilent(filename)
        == VendorPathChecker::ReaderAgilentFormatAcqData;
}

MSReaderBase::MSReaderClassType MSReaderAgilent::classTypeName() const
{
    return MSReaderClassTypeAgilent;
}

Err MSReaderAgilent::openFile(const QString& filename
    , MSConvertOption convert_options
    , QSharedPointer<ProgressBarInterface> progress
    )
{
    debugMs() << "Trying to open " << filename;

    if (d->msDataReader == nullptr) {
        if (!initialize()) {
            return kError;
        }
    }
    else if (d->fileOpened) {
        // please close the open file first
        return kError;
    }

    setFilename(QFileInfo(filename).fileName());

    HRESULT hr = S_OK;

    try {
        CComBSTR bstrFilePath = filename.toStdWString().c_str();

        // The return value should not be used as indication of success/failure
        // It merely means whether the file supports simultaineous reading while being acquired by
        // acquisition software.
        VARIANT_BOOL pRetVal = VARIANT_TRUE;
        hr = d->msDataReader->OpenDataFile(bstrFilePath, &pRetVal);

        if (FAILED(hr)) {
            puts("Failed to open data folder");
            throw hr;
        }

        d->fileOpened = true;

        CComBSTR version = _T("");
        hr = d->msDataReader->get_Version(&version);
        if (FAILED(hr)) {
            puts("Failed to get API version");
            throw hr;
        }

        HRESULT hr = d->msDataReader->get_MSScanFileInformation(&d->scanFileInformation);
        if (FAILED(hr)) {
            puts("Failed to get scanFileInformation");
            throw hr;
        }

        hr = d->msDataReader->get_FileInformation(&d->fileInformation);
        if (FAILED(hr)) {
            puts("Failed to get scanFileInformation");
            throw hr;
        }

        hr = d->msDataReader->QueryInterface(IID_INonmsDataReader, (void**)&d->nonMsReader);
        if (FAILED(hr)) {
            puts("Failed to get nonMsReader");
            throw hr;
        }

        return kNoErr;
    }
    catch (_com_error& e) {
        hr = e.Error();
    }
    catch (HRESULT& hr) {
        _com_error e(hr);
        hr = e.Error();
    }
    catch (...) {
        hr = E_UNEXPECTED;
    }

    return kError;
}
Err MSReaderAgilent::closeFile()
{
    if (isInitialized()) {
        if (d->fileOpened) {
            d->msDataReader->CloseDataFile();
            d->fileOpened = false;
        }
    }

    return !d->fileOpened ? kNoErr : kError;
}

Err MSReaderAgilent::getBasePeak(point2dList *points) const
{
    Q_ASSERT(points);

    const bool basePeakManual
        = as::value(QStringLiteral("MsReaderAgilent/BasePeakManualFallback"), false).toBool();

    if (basePeakManual) {
        // this will trigger manual BasePeak implemented in MSReader::_getBasePeakManual
        // this is here for testing and verifying outputs
        return kFunctionNotImplemented;
    }

    return getDataByChromatogram(ChromType_BasePeak, points);
}

Err MSReaderAgilent::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    const bool ticDataManual
        = as::value(QStringLiteral("MsReaderAgilent/TICDataManualFallback"), false).toBool();

    if (ticDataManual) {
        // this will trigger manual BasePeak implemented in MSReader::_getBasePeakManual
        // this is here for testing and verifying outputs
        return kFunctionNotImplemented;
    }

    return getDataByChromatogram(ChromType_TotalIon, points);
}

Err MSReaderAgilent::getDataByChromatogram(ChromType type, point2dList *points) const
{
    Q_ASSERT(points);
    CComPtr<BDA::IBDAChromFilter> chromFilter;
    HRESULT hr = CoCreateInstance(BDA::CLSID_BDAChromFilter, NULL, CLSCTX_INPROC_SERVER,
        BDA::IID_IBDAChromFilter, (void **)&chromFilter);

    if (S_OK != hr) {
        criticalMs() << "ERROR - CoCreateInstance for Chrom Filter failed, hr ="
            << QString("0x%1").arg(hr, 16);
        return kError;
    }

    chromFilter->put_ChromatogramType(type);
    chromFilter->put_MSLevelFilter(MSLevel_MS);
    chromFilter->put_DoCycleSum(false);

    std::vector<point2dList> pointList;
    Err e = d->getChromotograms(chromFilter, pointList); ree;

    if (pointList.size() > 0) {
        // swap
        *points = std::move(pointList[0]);
    }

    return kNoErr;
}

Err MSReaderAgilent::getScanData(long _scanNumber, point2dList *points, bool do_centroiding,
                                 PointListAsByteArrays *pointListAsByteArrays)
{
    Q_ASSERT(points);

    points->clear();
    if (!isInitialized()) {
        return kError;
    }

    long scanNumber = _scanNumber - 1;

    DeviceType deviceType = DeviceType_Unknown;
    d->scanFileInformation->get_DeviceType(&deviceType);

    // taken from pwiz but not sure if we need this because if there is no peak data (centroid)
    // then it will return full spectra (profile) in accordance with
    // DesiredMSStorageType_PeakElseProfile
    const bool canCentroid
        = deviceType != DeviceType_Quadrupole && deviceType != DeviceType_TandemQuadrupole;

    const bool peakSpectra = canCentroid && do_centroiding;

    HRESULT hr = S_OK;
    CComPtr<BDA::IBDASpecData> specData;

    if (peakSpectra) {
        hr = d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr, DesiredMSStorageType_Peak,
                                            &specData);
    } else {
        hr = d->msDataReader->GetSpectrum_8(scanNumber, nullptr, nullptr,
                                            DesiredMSStorageType_ProfileElsePeak, &specData);
    }
    if (FAILED(hr)) {
        return kError;
    }

    // Should be equal to xvals.count() ?
    long spectrumDataPoints = 0;
    hr = specData->get_TotalDataPoints(&spectrumDataPoints);
    if (FAILED(hr)) {
        return kError;
    }

    if (spectrumDataPoints == 0) {
        return kNoErr;
    }

    SpecType spectrumType = SpecType_Unspecified;
    specData->get_SpectrumType(&spectrumType);

    long totalScanCount = 0;
    specData->get_TotalScanCount(&totalScanCount);

    SAFEARRAY* psaX = nullptr;
    hr = specData->get_XArray(&psaX);
    if (FAILED(hr)) {
        return kError;
    }
    SafeArrayWrapper<double> xvals(psaX);

    SAFEARRAY* psaY = nullptr;
    hr = specData->get_YArray(&psaY);
    if (FAILED(hr)) {
        return kError;
    }
    SafeArrayWrapper<float> yvals(psaY);

    if (peakSpectra) {
        points->reserve(spectrumDataPoints);

        double x = 0.0;
        float y = 0.0;

        for (int i = xvals.lower(); i <= xvals.upper(); ++i) {
            x = xvals.value(i);
            y = yvals.value(i);
            points->push_back(QPointF(x, static_cast<double>(y)));
        }
    }
    else {
        removeZeroIntensities(xvals, yvals, points);
        spectrumDataPoints = static_cast<long>(points->size());
    }

    return kNoErr;
}

/*
* Sum of centroided intensity restricted to m/z range, per time.
*
scan=1 (0.5min)
m/z(x)   1 2 10 11 50
intensity(y)  10 20 30 40 10

XIC:
start_time,end_time = 0.5mins to 10 mins
mz_min, mz_max = 10, 15

time,centroid_intensity_sum = { (0.5, 70), (0.6, ?), ...}
*/

// TODO: verify this function.
Err MSReaderAgilent::getXICData(const XICWindow &xicWindow, point2dList *points, int msLevel) const
{
    Q_ASSERT(points);

    CComPtr<BDA::IBDAChromFilter> chromFilter;
    HRESULT hr = CoCreateInstance(BDA::CLSID_BDAChromFilter, NULL, CLSCTX_INPROC_SERVER,
                                  BDA::IID_IBDAChromFilter, (void **)&chromFilter);

    if (S_OK != hr) {
        criticalMs() << "ERROR - CoCreateInstance for Chrom Filter failed, hr ="
                     << QString("0x%1").arg(hr, 16);
        return kError;
    }

    chromFilter->put_ChromatogramType(ChromType_ExtractedIon);
    chromFilter->put_MSLevelFilter(static_cast<MSLevel>(msLevel));
    chromFilter->put_DoCycleSum(true);

    // mz range
    if (xicWindow.isMzRangeDefined()) {
        CComPtr<IMinMaxRange> mzRange;
        if (!makeMinMaxRange(xicWindow.mz_start, xicWindow.mz_end, mzRange)) {
            return kError;
        }

        SAFEARRAY *mzRangeArr = nullptr;
        putElementToSafeArray<IMinMaxRange>(mzRange, &mzRangeArr);

        hr = chromFilter->put_IncludeMassRanges(mzRangeArr);
        if (FAILED(hr)) {
            criticalMs() << "Failed to set IncludeMassRanges";
            return kError;
        }
    }

    // time range
    if (xicWindow.isTimeRangeDefined()) {
        CComPtr<IMinMaxRange> timeRange;
        if (!makeMinMaxRange(xicWindow.time_start, xicWindow.time_end, timeRange)) {
            return kError;
        }

        CComPtr<IRange> iRange;
        hr = timeRange->QueryInterface(IID_IRange, (void **)&iRange);
        if (FAILED(hr)) {
            criticalMs() << "Failed to query IID_IRange";
            return kError;
        }

        hr = chromFilter->putref_ScanRange(iRange);
        if (FAILED(hr)) {
            criticalMs() << "Failed to set ScanRange";
            return kError;
        }
    }

    std::vector<point2dList> pointList;
    Err e = d->getChromotograms(chromFilter, pointList); ree;

    if (pointList.size() > 0) {
        // swap
        *points = std::move(pointList[0]);
    }

    return kNoErr;
}

Err MSReaderAgilent::getScanInfo(long scanNumber, ScanInfo *scanInfo) const
{
    Q_ASSERT(scanInfo);

    if (!isInitialized()) {
        return kError;
    }

    static const bool useCache = true;
    if (useCache) {
        if (scanInfoCache(scanNumber, scanInfo)) {
            return kNoErr;
        }
    }

    // Quick procedure of finding most of scan properties
    Err e = d->getBasicScanInfo(scanNumber, *scanInfo);
    ree;

    // This one can be expensive so move it into a separate function
    // Find out if the scan is Peak/Profile/Mixed
    e = d->getPeakModeScanInfo(scanNumber, &scanInfo->peakMode); ree;

    if (useCache) {
        setScanInfoCache(scanNumber, *scanInfo);
    }

    return kNoErr;
}

Err MSReaderAgilent::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    if (!isInitialized()) {
        return kError;
    }

    const long scanNumberZeroBased = scanNumber - 1;

    pinfo->clear();

    Err e = d->findParentId(scanNumber, &pinfo->nScanNumber, &pinfo->nativeId); ree;

    CComPtr<BDA::IBDASpecData> specData;
    HRESULT hr = d->msDataReader->GetSpectrum_6(scanNumberZeroBased, nullptr, nullptr, &specData);

    long charge = 0;
    VARIANT_BOOL retVal = VARIANT_FALSE;
    hr = specData->GetPrecursorCharge(&charge, &retVal);
    if (retVal == VARIANT_TRUE) {
        pinfo->nChargeState = charge;
    }

    // This probably should be MSScanType_ProductIon (can it also be MSScanType_PrecursorIon or
    // MSScanType_SelectedIon?)
    MSScanType scanType = MSScanType_Unspecified;
    hr = specData->get_MSScanType(&scanType);

    long precursorCount = 0;
    SAFEARRAY *precursorIons = nullptr;
    hr = specData->GetPrecursorIon(&precursorCount, &precursorIons);
    SafeArrayWrapper<double> precursorIonList(precursorIons);

    if (precursorIonList.count() == 0) {
        warningMs() << "No data in precursor Ion list?";
        return kError;
    }

    // Use the last available data
    // See Err MSReaderBruker::getScanPrecursorInfo(long scanNumber, PrecursorInfo & pinfo) const
    // specifically this loop: for (long i = 0; i < lIsolationDataCount; i++) {
    pinfo->dIsolationMass = precursorIonList.back();
    pinfo->dMonoIsoMass = precursorIonList.back();

    return kNoErr;
}

Err MSReaderAgilent::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    if (!isInitialized()) {
        return kError;
    }

    HRESULT hr = S_OK;
    __int64 totalScansPresent = 0;

    try {

        hr = d->scanFileInformation->get_TotalScansPresent(&totalScansPresent);
        if (FAILED(hr)) {
            return kError;
        }

        if (totalNumber != nullptr) {
            *totalNumber = totalScansPresent;
            *startScan = 1;
            *endScan = totalScansPresent;
        }

        /*
        if (startScan != nullptr) {
            CComPtr<IMSScanRecord> msScanRecord;
            for (int i = 0; i < totalScansPresent; ++i) {

                hr = d->msDataReader->GetScanRecord(i, &msScanRecord);
                if (FAILED(hr)) {
                    continue;
                }

                msScanRecord->get_ScanID(&scanRecord);
                if (SUCCEEDED(hr)) {
                    *startScan = scanRecord;
                    break;
                }
            }
        }

        if (endScan != nullptr) {
            CComPtr<IMSScanRecord> msScanRecord;
            for (int i = totalScansPresent - 1; i >= 0; --i) {

                hr = d->msDataReader->GetScanRecord(i, &msScanRecord);
                if (FAILED(hr)) {
                    continue;
                }

                msScanRecord->get_ScanID(&scanRecord);
                if (SUCCEEDED(hr)) {
                    *endScan = scanRecord;
                    break;
                }
            }
        }
        */
    } catch (_com_error &e) {
        hr = e.Error();
    } catch (HRESULT &hr) {
        _com_error e(hr);
        hr = e.Error();
    } catch (...) {
        hr = E_UNEXPECTED;
    }

    if (FAILED(hr)) {
        return kError;
    }
    return kNoErr;
}

// See PWIZ_API_DECL SpectrumPtr SpectrumList_Agilent::spectrum(size_t index, DetailLevel
// detailLevel, const pwiz::util::IntegerSet& msLevelsToCentroid) const
Err MSReaderAgilent::getFragmentType(long _scanNumber, long scanLevel, QString *fragType) const
{
    Q_ASSERT(fragType);

    if (!isInitialized()) {
        return kError;
    }

    const long scanNumber = _scanNumber - 1;

    CComPtr<IMSScanRecord> scanRecord;
    d->msDataReader->GetScanRecord(scanNumber, &scanRecord);

    MSLevel msLevel = MSLevel_All;
    scanRecord->get_MSLevel(&msLevel);

    double mzOfInterest = 0.0;
    scanRecord->get_MZOfInterest(&mzOfInterest);

    double collisionEnergy = 0.0;
    scanRecord->get_CollisionEnergy(&collisionEnergy);

    //
    // See line #162 in proteowizard-code-9872-trunk\pwiz\pwiz\data\vendor_readers\Agilent\SpectrumList_Agilent.cpp
    //
    const bool condition1 = msLevel > 1 && mzOfInterest > 0.0;
    // isIonMobilityScan is always false for MHDAC implementation and true for MIDAC
    const bool condition2 = 1 == msLevel && collisionEnergy > 0.0;

    if (!condition1 && !condition2) {
        return kNoErr;
    }

    // See PWIZ_API_DECL CVID translateAsActivationType(DeviceType deviceType)
    // Is this usage correct?
    auto translateAsFragmentType = [](DeviceType deviceType) -> QString
    {
        switch (deviceType) {
            case DeviceType_Mixed:
                throw std::exception("[translateAsActivationType] Mixed device types not supported.");

            default:
            case DeviceType_Unknown:
                return "collision-induced dissociation";

            case DeviceType_IonTrap:
                return "trap-type collision-induced dissociation";

            case DeviceType_TandemQuadrupole:
            case DeviceType_Quadrupole:
            case DeviceType_QuadrupoleTimeOfFlight:
                return "beam-type collision-induced dissociation";

            case DeviceType_TimeOfFlight:
                // no collision cell, but this kind of activation is still possible
                return "in-source collision-induced dissociation";
        }
    };

    DeviceType deviceType = DeviceType_Unknown;
    d->scanFileInformation->get_DeviceType(&deviceType);

    try {
        *fragType = translateAsFragmentType(deviceType);
    }
    catch (const std::exception&) {
        return kError;
    }

    return kNoErr;
}

Err MSReaderAgilent::isCustomCentroidingPreferred(long scanNumber,
                                                  bool *customCentroidingPreferred) const
{
    if (!isInitialized()) {
        return kError;
    }

    HRESULT hr = S_OK;
    const long scanNumber_ = scanNumber - 1;

    // Find out if the scan is Peak/Profile/Mixed
    CComPtr<BDA::IBDASpecData> specData;
    hr = d->msDataReader->GetSpectrum_8(scanNumber_, nullptr, nullptr, DesiredMSStorageType_Profile,
                                        &specData);
    if (FAILED(hr)) {
        return kError;
    }

    MSStorageMode msStorageMode = MSStorageMode_Unspecified;
    specData->get_MSStorageMode(&msStorageMode);

    // Use custom centroiding <=> scan has profile data only
    *customCentroidingPreferred = msStorageMode == MSStorageMode_ProfileSpectrum;
    return kNoErr;
}

// TODO: if channelInternalName is empty, we want everything we can grab; properly populate
// ChromatogramInfo::internalChannelName
//      If channelInternalName is not empty, we parse and use that.
Err MSReaderAgilent::getChromatograms(QList<ChromatogramInfo> *chroList,
                                      const QString &internalChannelName)
{
    Q_ASSERT(chroList);

    debugMs() << "calling "
              << "MSReaderAgilent::getChromatograms";
    if (!isInitialized()) {
        return kError;
    }

    chroList->clear();

    if (internalChannelName.isEmpty()) {
        // Get all chromotograms

        for (const ChromType chromType : chromTypeOfInterest) {
            QList<ChromatogramInfo> chromInfoList;

            if (d->getChromatogramsByFilter(chromType, QString(), chromInfoList) != kNoErr) {
                continue;
            }

            *chroList += chromInfoList;
        }

        // Also add UV and FL data
        for (auto deviceType : { DeviceType_FluorescenceDetector, DeviceType_DiodeArrayDetector }) {

            QList<ChromatogramInfo> chromInfoList;
            if (kNoErr == d->getNonMsData(deviceType, chromInfoList)) {
                *chroList += chromInfoList;
            }
        }

    } else {

        // Get a specific chromatogram
        ChromType chromType = ChromType_Unspecified;
        DeviceType deviceType = DeviceType_Unknown;
        parseInternalChannelName(internalChannelName, &chromType, &deviceType);

        const bool nonMsData
            = deviceType == DeviceType_FluorescenceDetector // Fluorescence data points
            || deviceType == DeviceType_DiodeArrayDetector; // UV data points

        if (nonMsData) {
            // UV or Fluorescence data points
            return d->getNonMsData(internalChannelName, *chroList);

        } else {
            // other chromatograms
            if (d->getChromatogramsByFilter(chromType, internalChannelName, *chroList) != kNoErr) {
                return kError;
            }
        }
    }

    return kNoErr;
}

Err MSReaderAgilent::getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
{
    Q_UNUSED(msLevel);
    Q_UNUSED(scanTimeMinutes);
    Q_UNUSED(scanNumber);

    return kFunctionNotImplemented;
}

// from pwiz code:
// Agilent profile mode data returns all zero-intensity samples, so we filter out
// samples that aren't adjacent to a non-zero-intensity sample values. I.e.,
// remove all zeroes that are surrounded by zeroes. from pwiz
// proteowizard-code-9872-trunk\pwiz\pwiz\data\vendor_readers\Agilent\SpectrumList_Agilent.cpp,
// line 346, 419
void MSReaderAgilent::removeZeroIntensities(const SafeArrayWrapper<double> &xvals,
                                            const SafeArrayWrapper<float> &yvals,
                                            point2dList *points)
{
    Q_ASSERT(points);

    const float maxError = 0.0f;
    points->clear();

    const long count = xvals.count(); // == spectrumDataPoints
    Q_ASSERT(count == yvals.count());

    if (count == 0) {
        return;
    }

    double x = 0.0;
    float y = 0.0f;

    //
    // Special case: size < 3, pwiz doesn't do any filtering
    //
    if (count < 3) {
        for (int i = xvals.lower(); i <= xvals.upper(); ++i) {
            x = xvals.value(i);
            y = yvals.value(i);
            points->push_back(QPointF(x, static_cast<double>(y)));
        }
        return;
    }

    //
    // At least 3 items
    //
    points->reserve(count);

    x = xvals.value(xvals.lower());
    y = yvals.value(yvals.lower());

    const bool firstAlmostZero = almostEqual(y, 0.0f, maxError);
    const bool secondAlmostZero = almostEqual(yvals.value(yvals.lower() + 1), 0.0f, maxError);

    // Processing the first item
    if (!firstAlmostZero || !secondAlmostZero) {
        points->push_back(QPointF(x, static_cast<double>(y)));
    }

    // Processing the 'middle' items
    for (int i = xvals.lower() + 1; i < xvals.upper(); ++i) {
        x = xvals.value(i);
        y = yvals.value(i);

        const bool keepCurrentValue = !almostEqual(y, 0.0f, maxError)
            || !almostEqual(yvals.value(i - 1), 0.0f, maxError)
            || !almostEqual(yvals.value(i + 1), 0.0f, maxError);

        if (keepCurrentValue) {
            points->push_back(QPointF(x, static_cast<double>(y)));
        }
    }

    // Processing the last item
    const float lastY = yvals.value(yvals.upper());
    const bool keepLastValue
        = !almostEqual(lastY, 0.0f, maxError) || !almostEqual(y, 0.0f, maxError);

    if (keepLastValue) {
        points->push_back(QPointF(xvals.value(xvals.upper()), static_cast<double>(lastY)));
    }
}

_MSREADER_END
_PMI_END

