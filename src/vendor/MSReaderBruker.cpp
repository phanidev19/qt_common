#include "MSReaderBruker.h"

#include "BrukerSample.h"
#include "Centroid.h"
#include "pmi_common_ms_debug.h"
#include "pmi_common_ms_export.h"
#include "Reader.h"
#include "safearraywrapper.h"
#include "vendor_api/Bruker/BdalReaderWrapperInterface.h"

#include <math_utils.h>
#include <qt_string_utils.h>
#include <qt_utils.h>
#include <VendorPathChecker.h>

#include <QApplication>
#include <QDirIterator>
#include <QLibrary>
#include <QVector>

using namespace readerwrappers;

//NOTE: Remember that this requires VS2008 Redist installed and .NET (3.5 or higher)
//Microsoft Visual C++ 2008 Redistributable Package (x86)
//https://www.microsoft.com/en-us/download/details.aspx?id=29

#define hr_eee { \
if (FAILED(hr)) { \
    TRACEHR(hr); \
    e = kError; eee; \
} \
}

_PMI_BEGIN
_MSREADER_BEGIN

#ifdef _DEBUG
#define BDAL_READER_WRAPPER_DLL "BdalReaderWrapper_d.dll"
#else
#define BDAL_READER_WRAPPER_DLL "BdalReaderWrapper.dll"
#endif

static Err getNumberOfSpectra(const CComPtr<EDAL::IMSAnalysis2> &spIMSAnalysis2, long *startScan,
                              long *endScan)
{
    Err e = kNoErr;
    HRESULT hr = 0;

    EDAL::IMSSpectrumCollection *pSpectrumCollection = NULL;
    long spectrumItems = 0;

    hr = spIMSAnalysis2->get_MSSpectrumCollection(&pSpectrumCollection);
    if (hr != S_OK) {
        TRACEHR(hr);
        e = kBadParameterError; eee;
    }

    if (NULL != pSpectrumCollection)
    {
        CComPtr<EDAL::IMSSpectrumCollection> spSpectrumCollection;

        spSpectrumCollection.Attach(pSpectrumCollection);
        // JDN: this code is untested because get_MSSpectrumCollection returns a NULL
        // value of pSpectrumCollection
        hr = spSpectrumCollection->get_Count(&spectrumItems);
        if (hr != S_OK) {
            TRACEHR(hr);
            e = kBadParameterError; eee;
        }
        *startScan = 1;
        *endScan = spectrumItems;
    }

error:
    return e;
}

Err _makeIMSSpectrum(CComPtr<EDAL::IMSAnalysis2> &spIMSAnalysis2, long scanNumber,
                     CComPtr<EDAL::IMSSpectrum> &spIMSSpectrum)
{
    Err e = kNoErr;
    HRESULT hr = 0;

    //commonMsDebug() << "_makeIMSSpectrum scanNumber=" << scanNumber;

    EDAL::IMSSpectrumCollection *pSpectrumCollection = NULL;
    hr = spIMSAnalysis2->get_MSSpectrumCollection(&pSpectrumCollection);
    if (hr != S_OK) {
        TRACEHR(hr);
        e = kBadParameterError; eee;
    }
    if (pSpectrumCollection == NULL) {
        e = kError; eee;
    }

    {
        CComPtr<EDAL::IMSSpectrumCollection> spSpectrumCollection;

        spSpectrumCollection.Attach(pSpectrumCollection);
        // JDN: this code is untested because get_MSSpectrumCollection returns a NULL
        // value of pSpectrumCollection
        EDAL::IMSSpectrum *pIMSSpectrum = NULL;
        hr = spSpectrumCollection->get_Item(scanNumber, &pIMSSpectrum); hr_eee;

        if (pIMSSpectrum == NULL) {
            e = kBadParameterError; eee;
        }
        spIMSSpectrum.Attach(pIMSSpectrum);
    }
    //commonMsDebug() << "done _makeIMSSpectrum scanNumber=" << scanNumber;

error:
    return e;
}

Err _makeIMSSpectrum2(const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum,
                      CComPtr<EDAL::IMSSpectrum2> & spIMSSpectrum2)
{
    Err e = kNoErr;
    EDAL::IMSSpectrum2 *pIMSSpectrum2 = NULL;
    // IMSSpectrum2 may or may not be implemented so we ignore the return
    // value and check to see NULL!=pIMSSpectrum2
    (void)spIMSSpectrum->QueryInterface(
        __uuidof(EDAL::IMSSpectrum2),
        reinterpret_cast<void **>(&pIMSSpectrum2));
    if (NULL != pIMSSpectrum2)
    {
        spIMSSpectrum2.Attach(pIMSSpectrum2);
    } else {
        debugMs() << "Could not attach to spIMSSpectrum2";
        e = kBadParameterError; eee;
    }

error:
    return e;
}

class MSReaderBruker::PrivateData
{
public:
    PrivateData()
    {
        createInstance();
    }

    ~PrivateData() { releaseInstance(); }

    bool valid() const { return m_valid; }

    CComPtr<EDAL::IMSAnalysis2> spIMSAnalysis2;

public:
    Err createInstance()
    {
        m_valid = false;

        const HRESULT hr = spIMSAnalysis2.CoCreateInstance(__uuidof(EDAL::MSAnalysis));
        if (FAILED(hr)) {
            TRACEHR(hr);
            std::cerr << "Could not load Bruker libraries"
                      << " hr=" << hr << std::endl;
            rrr(kBadParameterError);
        }

        m_valid = true;

        return kNoErr;
    }

    Err createBdalReaderWrapperInstance()
    {
        // The parent function re-initializes and so do we
        releaseBdalWrapperInstance();

        typedef void*(*InstanceCreator)();
        InstanceCreator instanceCreator = reinterpret_cast<InstanceCreator>(
            QLibrary::resolve(BDAL_READER_WRAPPER_DLL, "CreateBdalReaderWrapperInstance"));

        if (!instanceCreator) {
            rrr(kBadParameterError);
        }

        m_bdalReaderWrapper.reset(
            reinterpret_cast<BdalReaderWrapperInterface *>(instanceCreator()));

        if (m_bdalReaderWrapper.isNull()) {
            rrr(kBadParameterError);
        }

        return kNoErr;
    }

    bool hasBdalReader()
    {
        return bdalReaderWrapper() != nullptr;
    }

    bool hasAnalysisData()
    {
        return bdalReaderWrapper() != nullptr && bdalReaderWrapper()->hasAnalysisData();
    }

    Err openFileByEdal(const QString &filename)
    {
        if (!valid()) {
            debugMs()
                << "MSReaderBruker instance does not exist.  Attempting to create new instance...";
            createInstance();
            if (valid()) {
                debugMs() << "...created instance properly.";
            } else {
                debugMs() << "...failed to create instance.";
                rrr(kBadParameterError);
            }
        }

        try {
            debugMs() << "About to open " << filename;
            const HRESULT hr = spIMSAnalysis2->Open(_bstr_t(filename.toStdWString().c_str()));
            if (hr != S_OK) {
                TRACEHR(hr);
                debugMs() << "Could not open filename=" << filename;
                rrr(kBadParameterError);
            }
            debugMs() << "Done opening Bruker file";
        } catch (_com_error &ex) {
            QString msg = (LPSTR)ex.ErrorMessage();
            debugMs() << "MSReaderBruker::openFile error with ex=" << msg;
            rrr(kBadParameterError);
        }
        return kNoErr;
    }

    Err openFileByBdal(const QString &filename)
    {
        if (bdalReaderWrapper() == nullptr) {
            rrr(kBadParameterError);
        }

        if (S_OK != bdalReaderWrapper()->openFile(filename.toStdString())) {
            debugMs() << "Could not open filename=" << filename << endl;
            debugMs() << "    Error: " << bdalReaderWrapper()->getLastErrorMessage().c_str()
                      << endl;
            rrr(kBadParameterError);
        }

        if (!bdalReaderWrapper()->hasAnalysisData()) {
            // Not an error
            infoMs() << filename << "apparently contains no Bruker analysis data";
        }

        return kNoErr;
    }

    BdalReaderWrapperInterface *bdalReaderWrapper()
    {
        if (m_bdalReaderWrapper.isNull()) {
            createBdalReaderWrapperInstance();
        }
        return m_bdalReaderWrapper.data();
    }

    void releaseBdalWrapperInstance()
    {
        m_bdalReaderWrapper.reset();
    }

    void releaseInstance()
    {
        releaseBdalWrapperInstance();

        // HRESULT hr = S_OK;
        if (m_valid) {
            try {
                if (spIMSAnalysis2.p) {
                    spIMSAnalysis2.Release();
                } else {
                    // Why is it NULL?  This should be not NULL, but something is deallocating
                    // memory  This is a bug as I'm assuming this pointer exists.  std::cout << "Note:
                    // spIMSAnalysis2.p is NULL.";  //better not call this until ML-284 gets fixed.
                }
            } catch (...) {
                std::cout << "spIMSAnalysis2.Release() threw an execption; ignored.";
            }

            debugMs() << "releaseInstance()..."; // calling this causes crash in closing due to Log
                                                 // class See ML-254
            //            hr = ::Cleanup(m_bLoadCOMServerFromManfest);
            //            commonMsDebug() << "cleanup called";
            //            if (FAILED(hr)) {
            //                TRACEHR(hr);
            //                commonMsDebug() << "Could not Cleanup hr=" << hr;
            //            }
        }
        m_valid = false;
    }

private:
    QString m_filename;

    bool m_valid = false;
    bool m_bLoadCOMServerFromManfest = true;

    // Bruker LC API
    QScopedPointer<BdalReaderWrapperInterface> m_bdalReaderWrapper;
};

MSReaderBruker::MSReaderBruker()
{
    d = new PrivateData;
}

MSReaderBruker::~MSReaderBruker()
{
    delete d;
}

pmi::MSReaderBase::MSReaderClassType pmi::msreader::MSReaderBruker::classTypeName() const
{
    return MSReaderClassTypeBruker;
}

bool MSReaderBruker::canOpen(const QString &path) const
{
    bool can_open = false;
    if (VendorPathChecker::isBrukerPath(path)) {
        VendorPathChecker::ReaderBrukerFormat bruker_format = VendorPathChecker::formatBruker(path);
        if (bruker_format == VendorPathChecker::ReaderBrukerFormatTims) {
            can_open = false;
        } else if (bruker_format != VendorPathChecker::ReaderBrukerFormatUnknown) {
            can_open = true;
        }
    }
    return can_open;
}

Err MSReaderBruker::openFile(const QString &filename, MSConvertOption convert_options,
                             QSharedPointer<ProgressBarInterface> progress)
{
    Q_UNUSED(convert_options);
    Q_UNUSED(progress);

    Err e = d->openFileByEdal(filename);
    if (e == kNoErr) {
        setFilename(filename);

        if (hasLsData(filename) && d->hasBdalReader()) {
            d->openFileByBdal(filename);
        }
    }
    return e;
}

Err MSReaderBruker::closeFile()
{
    d->releaseInstance();
    return kNoErr;
}

Err MSReaderBruker::getBasePeak(point2dList *points) const
{
    Q_UNUSED(points);

    return kFunctionNotImplemented;
}

Err MSReaderBruker::getTICData(point2dList *points) const
{
    Q_ASSERT(points);

    Err e = kNoErr;
    points->clear();
    HRESULT hr = 0;

    if (!d->valid()) {
        e = kBadParameterError; ree;
    }

    //Note: The Bruker function calls made below doesn't appear to produce any results.
    //But it's OK as the caller (MSReader) has its own implementation
    //for extracting TIC based on sum of centroided XIC.  We might actually
    //decide to use that for all instruments, to make the XIC definition
    //concise.

    BSTR sumIntensityStr = SysAllocString(L"SumIntensity");
    BSTR retentionTimeStr = SysAllocString(L"RetentionTime");
    VARIANT_BOOL hasDataIntent = VARIANT_FALSE;
    VARIANT_BOOL hasDataTime = VARIANT_FALSE;

    HRESULT has_intent = d->spIMSAnalysis2->HasAnalysisData(&sumIntensityStr, &hasDataIntent);
    HRESULT has_retent = d->spIMSAnalysis2->HasAnalysisData(&retentionTimeStr, &hasDataTime);

    if (has_intent && has_retent) {
        VARIANT intenData = { 0 };
        VARIANT timeData = { 0 };
        d->spIMSAnalysis2->GetAnalysisData(&sumIntensityStr, &intenData);
        d->spIMSAnalysis2->GetAnalysisData(&retentionTimeStr, &timeData);

        std::vector<double> tlist, ilist;

        hr = variantArrayToDoubleArray(intenData, ilist);
        if (hr != S_OK) {
            TRACEHR(hr);
            e = kBadParameterError; eee;
        }

        hr = variantArrayToDoubleArray(timeData, tlist); hr_eee;

        if (ilist.size() != tlist.size()) {
            e = kBadParameterError; eee;
        }
        points->resize(tlist.size());
        for (unsigned int i = 0; i < points->size(); i++) {
            (*points)[i].rx() = tlist[i];
            (*points)[i].ry() = ilist[i];
        }
    }

    if (points->size()) {
        debugMs() << "Note, MSReaderBruker::getTICData did not construct TIC.  Caller will make "
                     "TIC instead.";
    }

error:
    /// free allocation of BSTR strings
    SysFreeString(sumIntensityStr);
    SysFreeString(retentionTimeStr);
    return e;
}

Err MSReaderBruker::getScanData(long scanNumber, point2dList *points, bool do_centroiding,
                                PointListAsByteArrays *pointListAsByteArrays)
{
    Q_ASSERT(points);

    Err e = kNoErr;
    HRESULT hr = 0;
    CComPtr<EDAL::IMSSpectrum> spIMSSpectrum;
    EDAL::SpectrumTypes spectrumType = EDAL::SpectrumType_Profile;
    std::vector<double> xlist, ylist;
    bool apply_manual_centroiding = false;
    point2d p;

    points->clear();

    e = _makeIMSSpectrum(d->spIMSAnalysis2, scanNumber, spIMSSpectrum); eee;

    // Find out if Bruker contains proifle,centroid,or both.
    // Use this and do_centroiding to decide how to get desired output points.
    if (do_centroiding) {
        CComPtr<EDAL::IMSSpectrum2> spIMSSpectrum2;
        VARIANT_BOOL vbHasSpecTypeLine;
        e = _makeIMSSpectrum2(spIMSSpectrum, spIMSSpectrum2); eee;
        hr = spIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Line, &vbHasSpecTypeLine); hr_eee;

        // Note: calling EDAL::SpectrumType_Profile appears to really slow down the system.
        // So, we must be careful not to call this unless we must.
        // VARIANT_BOOL vbHasSpecTypeProfile;
        // hr = spIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Profile, &vbHasSpecTypeProfile);
        // hr_eee;
        {
            // if data contains centroid, use that. otherwise, use our own custom centroiding.
            if (vbHasSpecTypeLine == VARIANT_TRUE) {
                spectrumType = EDAL::SpectrumType_Line;
            } else {
                static int count = 0;
                if (count++ < 5) {
                    debugMs() << "requires manual centroiding in MSReaderBruker::getScanData";
                }
                apply_manual_centroiding = true;
            }
        }

        //Get centroid, if it exists, otherwise, get profile and then manually centroid.
        hr = GetSpectrumPoints(
            spIMSSpectrum,
            spectrumType,
            xlist, ylist); hr_eee;

        if (xlist.size() != ylist.size()) {
            e = kBadParameterError; eee;
        }

        if (apply_manual_centroiding) {
            DEBUG_WARNING_LIMIT(debugMs() << "MSReaderBruker::getScanData using manual centroiding",
                                20);

            pico::Centroid cent;
            std::vector<std::pair<double, double>> data, centroided_out;
            std::pair<double, double> pair_point;
            for (unsigned int i = 0; i < xlist.size(); i++) {
                pair_point.first = xlist[i];
                pair_point.second = ylist[i];
                data.push_back(pair_point);
            }

            int top_k; // = -1;
            double uncert_val; // = 0.01;
            double merge_radius; // = 4*uncert_val
            double end_uncert_val; // = uncert_val
            pico::UncertaintyScaling uncert_type; // = ConstantUncertainty;
            top_k = -1;
            uncert_val = 0.01;
            end_uncert_val = uncert_val;
            merge_radius = 4.0 * uncert_val;
            uncert_type = pico::ConstantUncertainty;
            bool sort_mz = true;
            bool adjust_intensity = true;

            cent.smooth_and_centroid(data, uncert_val, end_uncert_val, uncert_type, top_k,
                                     merge_radius, adjust_intensity, sort_mz, centroided_out);
            points->reserve(centroided_out.size());
            for (unsigned int i = 0; i < centroided_out.size(); i++) {
                p.rx() = centroided_out[i].first;
                p.ry() = centroided_out[i].second;
                points->push_back(p);
            }
        } else {
            points->reserve(xlist.size());
            for (unsigned int i = 0; i < xlist.size(); i++) {
                p.rx() = xlist[i];
                p.ry() = ylist[i];
                points->push_back(p);
            }
        }
    } else {
        // Note: calling spIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Profile,...) appears to
        // really slow down the system.  So, we are instead directly accessing the data as a proxy for
        // this information.

        // Get profile first
        hr = GetSpectrumPoints(spIMSSpectrum, spectrumType, xlist, ylist); hr_eee;

        // If no profile, get centroid
        if (xlist.size() <= 0) {
            spectrumType = EDAL::SpectrumType_Line;
            hr = GetSpectrumPoints(spIMSSpectrum, spectrumType, xlist, ylist); hr_eee;
        }

        if (xlist.size() != ylist.size()) {
            e = kBadParameterError; ree;
        }

        points->reserve(xlist.size());
        for (unsigned int i = 0; i < xlist.size(); i++) {
            p.rx() = xlist[i];
            p.ry() = ylist[i];
            points->push_back(p);
        }
    }

    if (pointListAsByteArrays) {
        static int count = 0;
        if (count++ < 30) {
            debugMs() << "MSReaderBruker::getScanData compression not handled yet";
        }
    }

error:
    return e;
}

Err MSReaderBruker::getXICData(const XICWindow &win, point2dList *points, int ms_level) const
{
    Q_UNUSED(win);
    Q_UNUSED(ms_level);

    Q_ASSERT(points);

    // Let MSReader::getXICData handle this since Bruker doesn't provide proper API
    points->clear();
    return kFunctionNotImplemented;
}

Err MSReaderBruker::getScanInfo(long scanNumber, ScanInfo *obj) const
{
    Q_ASSERT(obj);

    Err e = kNoErr;
    HRESULT hr = 0;
    CComPtr<EDAL::IMSSpectrum> spIMSSpectrum;
    double dRetentionTime = 0.0;
    long level = 0;

    e = _makeIMSSpectrum(d->spIMSAnalysis2, scanNumber, spIMSSpectrum); eee;
    hr = spIMSSpectrum->get_RetentionTime(&dRetentionTime); hr_eee;
    hr = spIMSSpectrum->get_MSMSStage(&level); hr_eee;

    obj->retTimeMinutes = dRetentionTime / 60.0;
    obj->scanLevel = level;
    obj->nativeId = QString("scan=%1").arg(scanNumber);
    obj->scanMethod = ScanMethodFullScan;
    obj->peakMode = PeakPickingModeUnknown;
    {
        EDAL::IMSSpectrum2 *pIMSSpectrum2 = NULL;
        CComPtr<EDAL::IMSSpectrum2> spIMSSpectrum2;
        VARIANT_BOOL vbHasSpecTypeLine;
        VARIANT_BOOL vbHasSpecTypeProfile;

        // IMSSpectrum2 may or may not be implemented so we ignore the return
        // value and check to see NULL!=pIMSSpectrum2
        (void)spIMSSpectrum->QueryInterface(__uuidof(EDAL::IMSSpectrum2),
                                            reinterpret_cast<void **>(&pIMSSpectrum2));
        if (NULL != pIMSSpectrum2) {
            spIMSSpectrum2.Attach(pIMSSpectrum2);

            hr = pIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Line, &vbHasSpecTypeLine); hr_eee;
            hr = pIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Profile, &vbHasSpecTypeProfile); hr_eee;
            if (vbHasSpecTypeProfile == VARIANT_TRUE) {
                obj->peakMode = PeakPickingProfile;
            } else if (vbHasSpecTypeLine == VARIANT_TRUE) {
                obj->peakMode = PeakPickingCentroid;
            }
        }
    }

error:
    return e;
}

Err MSReaderBruker::getScanPrecursorInfo(long scanNumber, PrecursorInfo *pinfo) const
{
    Q_ASSERT(pinfo);

    Err e = kNoErr;
    HRESULT hr = 0;
    pinfo->clear();

    ScanInfo thisScanInfo;
    e = getScanInfo(scanNumber, &thisScanInfo); eee;
    if (thisScanInfo.scanLevel <= 1) {
        debugMs()
            << "MSReaderBruker::getScanPrecursorInfo cannot get precursor as the scan level is "
            << thisScanInfo.scanLevel;
        e = kBadParameterError; eee;
    }
    for (long scan = scanNumber - 1; scan >= 0; scan--) {
        ScanInfo parentScanInfo;
        e = getScanInfo(scan, &parentScanInfo); eee;
        if (parentScanInfo.scanLevel < thisScanInfo.scanLevel) {
            pinfo->nScanNumber = scan;
            pinfo->nativeId
                = QString("scan=%1").arg(scan); // This should also be parentScanInfo.nativeId
            break;
        }
    }

    if (thisScanInfo.scanLevel > 1) {
        VARIANT vMassesIsolationData;
        CComVariant spvIsolationMassesData;
        SAFEARRAY *psaIsolationModi = NULL;
        CComSafeArray<int> spsaIsolationModi; // figured type out by running the code
        long lIsolationDataCount = 0;

        CComPtr<EDAL::IMSSpectrum> spIMSSpectrum;
        e = _makeIMSSpectrum(d->spIMSAnalysis2, scanNumber, spIMSSpectrum); eee;

        hr = spIMSSpectrum->GetIsolationData(&vMassesIsolationData, &psaIsolationModi,
                                             &lIsolationDataCount); hr_eee;
        hr = spvIsolationMassesData.Attach(&vMassesIsolationData); hr_eee;
        hr = spsaIsolationModi.Attach(psaIsolationModi); hr_eee;

        // ULONG ulNumberOfRowsIsolationMasses = V_ARRAY(&spvIsolationMassesData)->cbElements;
        // ULONG ulNumberOfRowsFragmentationModi = spsaFragmentationModi.GetCount(0);
        for (long i = 0; i < lIsolationDataCount; i++) {
            double iso_mass = 0;
            hr = RowDataToDouble(spvIsolationMassesData, i, &iso_mass); hr_eee;
            pinfo->dIsolationMass = iso_mass;
            pinfo->dMonoIsoMass = iso_mass;
            // pinfo.nChargeState = 0;
        }
    }

error:
    return e;
}

Err MSReaderBruker::getNumberOfSpectra(long *totalNumber, long *startScan, long *endScan) const
{
    Err e = kNoErr;

    e = pmi::msreader::getNumberOfSpectra(d->spIMSAnalysis2, startScan, endScan); eee;
    *totalNumber = *endScan + 1;

error:
    return e;
}

enum Bruker_FragmentationMode
{
     FragMode_Off = 0
    ,FragMode_CID = 1
    ,FragMode_ETD = 2
    ,FragMode_CIDETD_CID = 3
    ,FragMode_CIDETD_ETD = 4
    ,FragMode_ISCID = 5
    ,FragMode_ECD = 6
    ,FragMode_IRMPD = 7
    ,FragMode_Unknown = 255
};

Err MSReaderBruker::getFragmentType(long scanNumber, long scanLevel, QString *fragType) const
{
    Q_ASSERT(fragType);

    Err e = kNoErr;
    HRESULT hr = 0;

    //Bruker doesn't need scanLevel for their API
    Q_UNUSED(scanLevel);
    /* scanLevel
    if (scanLevel <= 1) {
        commonMsDebug() << "MSReaderBruker::getFragmentType called on level that cannot have fragment type, scanLevel=" << scanLevel;
        fragType = "unknown";
        e = kBadParameterError; ree;
    }
    */

    //double precursorMz=0;
    CComPtr<EDAL::IMSSpectrum> spIMSSpectrum;

    VARIANT vMassesFragmentationData;
    CComVariant spvFragmentationMassesData;
    SAFEARRAY *psaFragmentationModi = NULL;
    CComSafeArray<int> spsaFragmentationModi; // figured type out by running the code
    long lFragmentationDataCount = 0;
    ULONG ulNumberOfRows = 0;
    ULONG ulNumberOfRowsFragmentationModi = 0;

    e = _makeIMSSpectrum(d->spIMSAnalysis2, scanNumber, spIMSSpectrum); eee;

    hr = spIMSSpectrum->GetFragmentationData(&vMassesFragmentationData, &psaFragmentationModi,
                                             &lFragmentationDataCount); hr_eee;

    hr = spvFragmentationMassesData.Attach(&vMassesFragmentationData); hr_eee;
    hr = spsaFragmentationModi.Attach(psaFragmentationModi); hr_eee;

    ulNumberOfRows = V_ARRAY(&spvFragmentationMassesData)->cbElements;
    ulNumberOfRowsFragmentationModi = spsaFragmentationModi.GetCount(0);

    if (ulNumberOfRowsFragmentationModi == 0) {
        *fragType = "not_available";
    }

    // ECD  "electron capture dissociation" is similar to ETD but slower.  
    //   It's used on FTICR instruments.  For our purposes, it's the same as ETD.
    // IRMPD "infrared multi-photon dissociation" is similar to low-energy CID but it's not resonant excitation, 
    //  so it keeps fragmenting the fragments.  For our purposes, it's the same as CID, but it really looks more like overlaid MS^n.
    // CIDETD_CID is probably the CID half of alternating CID/ETD
    // CIDETD_ETD is probably the ETD half.
    // PTR is proton-transfer reaction.  Looks like it's for ambient small molecules.
    // http://en.wikipedia.org/wiki/Proton-transfer-reaction_mass_spectrometry
    // ISCID is in-source CID.  The spectra will probably look most like HCD since it's higher energy, I think.

    for (ULONG ulRowCount = 0; ulRowCount < ulNumberOfRowsFragmentationModi; ulRowCount++)
    {
        int frag_mode = spsaFragmentationModi.GetAt(ulRowCount);
        switch (frag_mode)
        {
            case FragMode_CID: *fragType = "cid"; break;
            case FragMode_ETD: *fragType = "etd"; break;
            case FragMode_CIDETD_CID: *fragType = "cid"; break;
            case FragMode_CIDETD_ETD: *fragType = "etd"; break;
            case FragMode_ISCID: *fragType = "cid"; break;
            case FragMode_ECD: *fragType = "etd"; break;
            case FragMode_IRMPD: *fragType = "irmpd"; break;
            default: *fragType="unknown"; break;
        }
        if (1) break;
    }

error:
    return e;
}

Err MSReaderBruker::extractChromatogramData(long traceId, point2dList *points) const
{
    Q_ASSERT(points != nullptr);

    std::vector<double> times;
    std::vector<double> intensities;

    BdalReaderWrapperInterface *const reader = d->bdalReaderWrapper();

    if (S_OK != reader->getTraceData(traceId, &times, &intensities)) {
        debugMs() << "Could not get Trace Data." << endl;
        debugMs() << "    Error: " << d->bdalReaderWrapper()->getLastErrorMessage().c_str()
            << endl;
        rrr(kBadParameterError);
    }
    Q_ASSERT(times.size() == intensities.size());
    points->reserve(times.size());
    for (size_t i = 0; i < times.size(); ++i) {
        points->push_back(point2d(times[i], intensities[i]));
    }

    return kNoErr;
}


// NOTE: next 2 functions and method is preparation for VendorPathChecker refactoring
// check* functions will be moved to VendorPathChecker public interface.
// hasLsData method will be used by VendorPathChecker.
static bool checkIfDirectory(const QString &filename)
{
    return QFileInfo(filename).isDir();
}

static bool checkIfFileExist(const QString &filename)
{
    if (filename.isEmpty()) {
        return false;
    }

    QFileInfo fileInfo(filename);
    return fileInfo.isFile() && fileInfo.exists();
}

bool MSReaderBruker::hasLsData(const QString &filename)
{
    const QString sourcePath(filename);
    if (!checkIfDirectory(sourcePath)) {
        if (!checkIfFileExist(sourcePath)) {
            return false;
        }
    }
    QDirIterator it(filename, QDirIterator::FollowSymlinks);
    while (it.hasNext()) {
        const QString next = it.next();
        if (next.endsWith("/.") || next.endsWith("/..")) {
            continue;
        }
        const QString extension = QFileInfo(next).suffix().toLower();

        if (extension == "unt") {
            return true;
        }
    }
    return false;
}

Err MSReaderBruker::getChromatograms(QList<ChromatogramInfo> *chroList,
                                     const QString &channelInternalName)
{
    Q_ASSERT(chroList);

    Err e = kNoErr;

    // for thread-safe and perfomance we fill local variable and then swap it with chroList
    QList<ChromatogramInfo> result;

    if (!d->hasAnalysisData()) {
        // Assume now that we have no registered Bruker DLLs
        // So we fallback to the original non-implemented state
        return kNoErr;
    }

    BdalReaderWrapperInterface* reader = d->bdalReaderWrapper();
    std::vector<BdalReaderWrapperInterface::TraceInfo> traces;
    if (S_OK != reader->getTraces(&traces)) {
        rrr(kBadParameterError);
    }
    if (channelInternalName.isEmpty()) {
        result.reserve(static_cast<int>(traces.size()));
        for (BdalReaderWrapperInterface::TraceInfo trace : traces) {
            auto resultIterator = result.insert(result.end(), ChromatogramInfo());
            if (extractChromatogramData(trace.id, &(resultIterator->points)) != kNoErr) {
                result.erase(resultIterator);
                continue;
            }
            resultIterator->channelName = QString::fromStdString(trace.name);
            resultIterator->internalChannelName = QString::fromStdString(trace.internalName);
        }
    } else {
        const std::string channelInternalNameStd = channelInternalName.toStdString();
        const auto traceIterator = std::find_if(
            std::cbegin(traces), std::cend(traces),
            [&channelInternalNameStd](const BdalReaderWrapperInterface::TraceInfo &trace) {
                return trace.internalName == channelInternalNameStd;
            });

        if (traceIterator != std::cend(traces)) {
            auto resultIterator = result.insert(result.end(), ChromatogramInfo());
            if (extractChromatogramData(traceIterator->id, &(resultIterator->points)) != kNoErr) {
                result.erase(resultIterator);
            } else {
                resultIterator->channelName = QString::fromStdString(traceIterator->name);
                resultIterator->internalChannelName = QString::fromStdString(traceIterator->internalName);
            }
        }
    }

    chroList->swap(result);

    return e;
}

Err MSReaderBruker::getBestScanNumber(int msLevel, double scanTimeMinutes, long *scanNumber) const
{
    Q_UNUSED(msLevel);
    Q_UNUSED(scanTimeMinutes);
    Q_UNUSED(scanNumber);
    return kFunctionNotImplemented;
}

_MSREADER_END
_PMI_END
