// By default this generates a compassxtractms.tlh (header for COM objects
// exposed by type library) and compassxtractms.tli (C++ wrappers for COM
// objects exposed by compassxtractms. The TLI file generates exceptions on
// errors so no_implementation (#import attribute) is used to supressed the TLI
// generation. When coding COM us return values of HRESULT. Place the
// exception handlng outside of the methods/functions exposing COM.
//
// When a TLI is generated the HRESULT returning non-exception methods are
// prefixed by raw_which is tedious to code with hence another reason to suppress
// the generation of the TLI file (e.g raw_Open which more simply could be Open
// if TLI generation is suppressed). The non-raw methods when a TLI is generated
// throw exceptions
//
// no_implementation: no TLI
// raw_interfaces_only: no exception generating wrappers and no _bstr_t, _variatn_t and
// _com_ptr.
//#import ".\\source\\pwiz-pmi\\compassxtractms.dll" no_implementation raw_interfaces_only
// THe *.tlh file is generated by the above #import
//#include  "compassxtractms.tlh"
//#include  "..\\pwiz-pmi\\compassxtractms.tlh"

#include "BrukerSample.h"

////////////////////////////////////////////////////////////
//  Was in header begin
////////////////////////////////////////////////////////////


HRESULT Cleanup(bool bLoadCOMServerFromManfest);

HRESULT Process(const std::wstring& kwstrDataFile);

////////////////////////////////////////////////////////////
//  Was in header end
////////////////////////////////////////////////////////////

static std::wstring gkwstrEDALSxSManifestFile = L"Interop.EDAL.SxS.manifest";

static const CComBSTR gbstrDataTypeRetentionTime(L"RetentionTime");
static const CComBSTR gbstrDataTypeSumIntensity(L"SumIntensity");

static ULONG_PTR glpCookie = 0;
static HANDLE ghActCtx = INVALID_HANDLE_VALUE;

extern "C"
{
    // #define STDAPI EXTERN_C HRESULT STDAPICALLTYPE
    typedef HRESULT (STDAPICALLTYPE *tDllGetClassObject)(
                            const CLSID& clsid,
                            const IID& iid,
                            void** ppv);
}

static const wchar_t *VARIANT_BOOLToText(VARIANT_BOOL vb)
{
    return (VARIANT_FALSE == vb) ? L"false" : L"true";
}

HRESULT Setup(bool bLoadCOMServerFromManfest, const std::wstring& wstrExecutableFolder)
{
    //HR(::CoInitialize(NULL)); // setup COM
    if (bLoadCOMServerFromManfest)
    {
        std::wstring wstrFullManifestFile;

        wstrFullManifestFile.resize(wstrExecutableFolder.length() + gkwstrEDALSxSManifestFile.length() + 4);
        (void)::PathCombine(
            const_cast<wchar_t *>(wstrFullManifestFile.c_str()),
            wstrExecutableFolder.c_str(),
            gkwstrEDALSxSManifestFile.c_str());
        wstrFullManifestFile.resize(::wcslen(wstrFullManifestFile.c_str()));
        if (FALSE == ::PathFileExists(wstrFullManifestFile.c_str()))
        {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        ACTCTX actctx = {0};

        actctx.cbSize = sizeof actctx;
        actctx.lpSource = const_cast<wchar_t *>(wstrFullManifestFile.c_str());
        ghActCtx = ::CreateActCtx(&actctx);
        if (INVALID_HANDLE_VALUE == ghActCtx)
        {
            return HRESULT_FROM_WIN32(::GetLastError());
        }

        else
        {
            if(!::ActivateActCtx(ghActCtx, &glpCookie))
            {
                return HRESULT_FROM_WIN32(::GetLastError());
            }
        }
    }

    return S_OK;
}


HRESULT PreparePathForLoadingDLL(bool bLoadCOMServerFromManfest, const std::wstring& wstrFullManifestFile)
{
    //HR(::CoInitialize(NULL)); // setup COM
    if (bLoadCOMServerFromManfest)
    {
        if (FALSE == ::PathFileExists(wstrFullManifestFile.c_str()))
        {
            return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
        }

        ACTCTX actctx = {0};

        actctx.cbSize = sizeof actctx;
        actctx.lpSource = const_cast<wchar_t *>(wstrFullManifestFile.c_str());
        ghActCtx = ::CreateActCtx(&actctx);
        if (INVALID_HANDLE_VALUE == ghActCtx)
        {
            return HRESULT_FROM_WIN32(::GetLastError());
        }

        else
        {
            if(!::ActivateActCtx(ghActCtx, &glpCookie))
            {
                return HRESULT_FROM_WIN32(::GetLastError());
            }
        }
    }

    return S_OK;
}

using namespace std;

HRESULT Cleanup(bool bLoadCOMServerFromManfest)
{
    if (bLoadCOMServerFromManfest)
    {
        if (glpCookie)
        {
            (void)::DeactivateActCtx(0, glpCookie);
            glpCookie = NULL;
        }

        if (ghActCtx != INVALID_HANDLE_VALUE)
        {
            ::ReleaseActCtx(ghActCtx);
            ghActCtx = INVALID_HANDLE_VALUE;
        }
    }

    //::CoUninitialize(); // clean up COM
    return S_OK;
}

HRESULT variantArrayToDoubleArray(VARIANT & vData, std::vector<double> & values) {
    VARTYPE vtExpected = VT_R8 | VT_ARRAY;
    CComVariant spvData;
    //double datum = std::numeric_limits<double>::min();

    values.clear();

    HR(spvData.Attach(&vData));
    if (vtExpected != spvData.vt)
    {
        std::wcout << L"Unexpected VARIANT type: " << std::hex << spvData.vt << std::endl;
        return E_FAIL;
    }

    // the CComVARIANT will no longer managed the data as this is a SAFEARRAY
    spvData.Detach(&vData);

    CComSafeArray<double> spsaData(((VARIANT)spvData).parray);

    long lower = spsaData.GetLowerBound(0);
    long upper = spsaData.GetUpperBound(0);

    if (upper - lower >= 0) {
        values.reserve(upper-lower+1);
    }

    for (long count = lower; count <= upper; count++)
    {
        double currentValue = spsaData.GetAt(count);
        values.push_back(currentValue);
    }

    return S_OK;
}

HRESULT GetAnalysisData(const CComPtr<EDAL::IMSAnalysis2>& spIMSAnalysis2, const CComBSTR& spBSTRDataCategory)
{
    VARIANT_BOOL hasData = VARIANT_FALSE;
    BSTR bstrDataCategory = spBSTRDataCategory.m_str;

    // &bstrDataCategory mean the parameter is a BSTR* which does not seem to make sense
    HR(spIMSAnalysis2->HasAnalysisData(&bstrDataCategory, &hasData));
    if (VARIANT_TRUE == hasData)
    {
        VARIANT vData = {0};
        VARTYPE vtExpected = VT_R8 | VT_ARRAY;
        CComVariant spvData;
        double datum = std::numeric_limits<double>::min();

        HR(spIMSAnalysis2->GetAnalysisData(&bstrDataCategory, &vData));
        HR(spvData.Attach(&vData));
        if (vtExpected != spvData.vt)
        {
            std::wcout << L"Unexpected VARIANT type: " << std::hex << spvData.vt << std::endl;

            return E_FAIL;
        }

        // the CComVARIANT will no longer managed the data as this is a SAFEARRAY
        spvData.Detach(&vData);

        CComSafeArray<double> spsaData(((VARIANT)spvData).parray);

        long lower = spsaData.GetLowerBound(0);
        long upper = spsaData.GetUpperBound(0);

        for (long count = lower; count <= upper; count++)
        {
            double currentValue = spsaData.GetAt(count);

            if (datum < currentValue)
            {
                datum = currentValue;
            }
        }

        std::wcout
            << L"Data Category "
            << (wchar_t *)spBSTRDataCategory
            << L" maxium value: "
            << datum
            << std::endl;
    }

    else
    {
        std::wcout
            << L"Data Category: "
            << (wchar_t *)spBSTRDataCategory
            << L" contains no analysis data"
            << std::endl;
    }

    return S_OK;
}

HRESULT ProcesAnalysisMetaData(const CComPtr<EDAL::IMSAnalysis2>& spIMSAnalysis2)
{
    BSTR bstsrData = NULL;
    CComBSTR spbstrAnalysisDescription;
    CComBSTR spbstrAnalysisName;
    CComBSTR spbstrOperatorName;
    CComBSTR spbstrAnalysisDateTimeIsoString;
    CComBSTR spbstrSampleName;
    CComBSTR spbstrMethodName;
    CComBSTR spbstrInstrumentDescription;
    DATE analysisDateTime = {0};
    EDAL::InstrumentFamily instrumentFamily;

    // IMSAnalysis properties
    HR(spIMSAnalysis2->get_AnalysisName(&bstsrData));
    spbstrAnalysisName.Attach(bstsrData);
    HR(spIMSAnalysis2->get_OperatorName(&bstsrData));
    spbstrOperatorName.Attach(bstsrData);
    HR(spIMSAnalysis2->get_AnalysisDateTimeIsoString(&bstsrData));
    spbstrAnalysisDateTimeIsoString.Attach(bstsrData);
    HR(spIMSAnalysis2->get_SampleName(&bstsrData));
    spbstrSampleName.Attach(bstsrData);
    HR(spIMSAnalysis2->get_MethodName(&bstsrData));
    spbstrMethodName.Attach(bstsrData);
    HR(spIMSAnalysis2->get_InstrumentDescription(&bstsrData));
    spbstrInstrumentDescription.Attach(bstsrData);
    HR(spIMSAnalysis2->get_AnalysisDateTime(&analysisDateTime));
    HR(spIMSAnalysis2->get_InstrumentFamily(&instrumentFamily));
    // IMSAnalysis2 properties
    HR(spIMSAnalysis2->get_AnalysisDescription(&bstsrData));
    spbstrAnalysisDescription.Attach(bstsrData);
    std::wcout << L"Analysis Name: " << (wchar_t *)spbstrAnalysisName << std::endl;
    std::wcout << L"Operator Name: " << (wchar_t *)spbstrOperatorName << std::endl;
    std::wcout << L"Analysis Date/Time: " << (wchar_t *)spbstrAnalysisDateTimeIsoString << std::endl;
    std::wcout << L"Sample Name: " << (wchar_t *)spbstrSampleName << std::endl;
    std::wcout << L"Method Name: " << (wchar_t *)spbstrMethodName << std::endl;
    std::wcout << L"Instrument Description: " << (wchar_t *)spbstrInstrumentDescription << std::endl;
    std::wcout << L"Analysis Description: " << (wchar_t *)spbstrAnalysisDescription << std::endl;
    HR(GetAnalysisData(spIMSAnalysis2, gbstrDataTypeRetentionTime));
    HR(GetAnalysisData(spIMSAnalysis2, gbstrDataTypeSumIntensity));

    return S_OK;
}

const wchar_t * SpectrumPolarityToText(EDAL::SpectrumPolarity spectrumPolarity)
{
    switch(spectrumPolarity)
    {
    case EDAL::IonPolarity_Positive:
        return L"Ion Polarity Positive";

    case EDAL::IonPolarity_Negative:
        return L"Ion Polarity Negative";

    default:
    case EDAL::IonPolarity_Unknown:
        return L"Ion Polarity Unknown";
    }
}

HRESULT ProcesSpectrumParameters(const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum)
{
    EDAL::IMSSpectrumParameterCollection *pIMSSpectrumParameterCollection;

    HR(spIMSSpectrum->get_MSSpectrumParameterCollection(&pIMSSpectrumParameterCollection));
    if (NULL != pIMSSpectrumParameterCollection)
    {
          CComPtr<EDAL::IMSSpectrumParameterCollection> spIMSSpectrumParameterCollection;
        long lNumberOfParameters = 0;

        spIMSSpectrumParameterCollection.Attach(pIMSSpectrumParameterCollection);
        HR(pIMSSpectrumParameterCollection->get_Count(&lNumberOfParameters));
        std::wcout << L"Number of spectrum parameters: " << lNumberOfParameters << std::endl;
        for (long lSlot = 1; lSlot <= lNumberOfParameters; lSlot++)
        {
            EDAL::IMSSpectrumParameter *pIMSSpectrumParameter;
            CComPtr<EDAL::IMSSpectrumParameter> spIMSSpectrumParameter;
            BSTR bstrData = NULL;
            CComBSTR spbstrGroupName;
            CComBSTR spbstrParameterName;
            CComBSTR spbstrUnitName;
            VARIANT vValue = {0};
            CComVariant spvValue;
            HRESULT hrVariantChangeType;
            CComVariant spvSlot(lSlot);
            CComBSTR spbstrValue;

            HR(pIMSSpectrumParameterCollection->get_Item(spvSlot, &pIMSSpectrumParameter));
            spIMSSpectrumParameter.Attach(pIMSSpectrumParameter);
            HR(spIMSSpectrumParameter->get_GroupName(&bstrData));
            spbstrGroupName.Attach(bstrData);
            HR(spIMSSpectrumParameter->get_ParameterName(&bstrData));
            spbstrParameterName.Attach(bstrData);
            HR(spIMSSpectrumParameter->get_ParameterUnit(&bstrData));
            spbstrUnitName.Attach(bstrData);
            HR(spIMSSpectrumParameter->get_ParameterValue(&vValue));
            HR(spvValue.Attach(&vValue));
            hrVariantChangeType = spvValue.ChangeType(VT_BSTR);
            if (S_OK == hrVariantChangeType)
            {
            }

            else
            {
                spbstrValue = L"value cannot be respresented by a string";
            }

            std::wcout
                << L"Spectrum parameter "
                << lSlot
                << L", Group Name: "
                << BSTR(spbstrGroupName)
                << L", Parameter Name: "
                << BSTR(spbstrParameterName)
                << L", Unit: "
                << BSTR(spbstrUnitName)
                << L", Value: "
                << BSTR(spvValue.bstrVal)
                << std::endl;
        }
    }

    return S_OK;
}

HRESULT ProcessSpectrumRowData(const CComVariant& spvData, ULONG ulRowCount, const wchar_t dataDescription[])
{
    LONG lLowerBound;
    LONG lUpperBound;
    long lDimensions[2];

    lDimensions[0] = ulRowCount;
    // 1 is from the DumpAnalysis.cs sample
    // for( int col = 0; col < iMassesArray.GetLength(1); ++col )
    HR(::SafeArrayGetLBound(V_ARRAY(&spvData), 1, &lLowerBound));
    HR(::SafeArrayGetUBound(V_ARRAY(&spvData), 1, &lUpperBound));
    std::wcout << dataDescription;
    for (LONG lColumn = lLowerBound; lColumn <= lUpperBound; lColumn++)
    {
        double value = 0;

        if (lColumn != lLowerBound)
        {
            std::wcout << L";";
        }

        lDimensions[1] = lColumn;
        HR(::SafeArrayGetElement(V_ARRAY(&spvData), lDimensions, &value));
        std::wcout << value;
    }

    return S_OK;
}

HRESULT RowDataToDouble(const CComVariant& spvData, ULONG ulRowCount, double * outval)
{
    LONG lLowerBound;
    LONG lUpperBound;
    long lDimensions[2];

    lDimensions[0] = ulRowCount;
    // 1 is from the DumpAnalysis.cs sample
    // for( int col = 0; col < iMassesArray.GetLength(1); ++col )
    HR(::SafeArrayGetLBound(V_ARRAY(&spvData), 1, &lLowerBound));
    HR(::SafeArrayGetUBound(V_ARRAY(&spvData), 1, &lUpperBound));
    for (LONG lColumn = lLowerBound; lColumn <= lUpperBound; lColumn++)
    {
        double value = 0;
        lDimensions[1] = lColumn;
        HR(::SafeArrayGetElement(V_ARRAY(&spvData), lDimensions, &value));
        *outval = value;
        return S_OK;
    }

    return S_FALSE;
}

HRESULT ProcesSpectrumMetaData(const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum, EDAL::IMSSpectrum2 *pIMSSpectrum2)
{
    //////////////////////////////////////////////////////////////
    // Meta Data
    double dRetentionTime = 0.0;
    EDAL::SpectrumPolarity spectrumPolarity = EDAL::IonPolarity_Unknown;

    HR(spIMSSpectrum->get_RetentionTime(&dRetentionTime));
    std::wcout << L"Retention Time: " << dRetentionTime << std::endl;
    HR(spIMSSpectrum->get_Polarity(&spectrumPolarity));
    std::wcout <<
        L"Spectrum Polarity: " <<
        SpectrumPolarityToText(spectrumPolarity) <<
        std::endl;

    if (NULL != pIMSSpectrum2)
    {
        VARIANT_BOOL vbHasSpecType;
        VARIANT_BOOL vbHasRecalibratedData;

        HR(pIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Line, &vbHasSpecType));
        std::wcout << L"Has Spec Type Line: " << VARIANT_BOOLToText(vbHasSpecType) << std::endl;
        HR(pIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Profile, &vbHasSpecType));
        std::wcout << L"Has Spec Type Profile: " << VARIANT_BOOLToText(vbHasSpecType) << std::endl;
        HR(pIMSSpectrum2->HasRecalibratedData(&vbHasRecalibratedData));
        std::wcout << L"Has Recalibrated Data: " << VARIANT_BOOLToText(vbHasRecalibratedData) << std::endl;
        if (VARIANT_TRUE == vbHasRecalibratedData)
        {
            // JDN: The above if statement is an educated guess. There is no sample
            // demonstrating this code being invoked and not file containing said data
            // could invoke this here pIMSSpectrum2->GetRecalibratedMassIntensityValues
        }
    }

    VARIANT vMassesIsolationData;
    CComVariant spvIsolationMassesData;
    SAFEARRAY *psaIsolationModi = NULL;
    CComSafeArray<int> spsaIsolationModi; // figured type out by running the code
    long lIsolationDataCount;

    HR(spIMSSpectrum->GetIsolationData(&vMassesIsolationData, &psaIsolationModi, &lIsolationDataCount));
    HR(spvIsolationMassesData.Attach(&vMassesIsolationData));
    HR(spsaIsolationModi.Attach(psaIsolationModi));

    VARIANT vMassesFragmentationData;
    CComVariant spvFragmentationMassesData;
    SAFEARRAY *psaFragmentationModi = NULL;
    CComSafeArray<int> spsaFragmentationModi; // figured type out by running the code
    long lFragmentationDataCount;

    HR(spIMSSpectrum->GetFragmentationData(&vMassesFragmentationData, &psaFragmentationModi, &lFragmentationDataCount));
    HR(spvFragmentationMassesData.Attach(&vMassesFragmentationData));
    HR(spsaFragmentationModi.Attach(psaFragmentationModi));

    ULONG ulNumberOfRows = V_ARRAY(&spvFragmentationMassesData)->cbElements;
    ULONG ulNumberOfRowsIsolationMasses = V_ARRAY(&spvIsolationMassesData)->cbElements;
    ULONG ulNumberOfRowsIsolationModi = spsaIsolationModi.GetCount(0);
    ULONG ulNumberOfRowsFragmentationModi = spsaFragmentationModi.GetCount(0);

    if ((ulNumberOfRowsIsolationMasses != ulNumberOfRows) ||
        (ulNumberOfRowsIsolationModi != ulNumberOfRows) ||
        (ulNumberOfRowsFragmentationModi != ulNumberOfRows))
    {
        std::wcout << L"Fragmentation Masses:" << ulNumberOfRows << std::endl;
        std::wcout << L"Fragmentation Modes:" << ulNumberOfRowsFragmentationModi << std::endl;
        std::wcout << L"Isolation Masses:" << ulNumberOfRowsIsolationMasses << std::endl;
        std::wcout << L"Isolation Modes:" << ulNumberOfRowsIsolationModi << std::endl;
        std::wcout << L"Error: number of fragmentation and isolation rows differs." << std::endl;

        return S_FALSE; // data error not programming error
    }

    if (V_ARRAY(&spvFragmentationMassesData)->cDims != V_ARRAY(&spvIsolationMassesData)->cDims)
    {
        std::wcout << L"Invalid dimension of array.";

        return S_FALSE; // data error not programming error
    }

    for (ULONG ulRowCount = 0; ulRowCount < ulNumberOfRows; ulRowCount++)
    {
        std::wcout
            << L"  Iso Mode "
            << spsaIsolationModi.GetAt(ulRowCount)
            << L"Frag Modi "
            << spsaFragmentationModi.GetAt(ulRowCount);
        HR(ProcessSpectrumRowData(spvIsolationMassesData, ulRowCount, L" Iso Mass"));
        HR(ProcessSpectrumRowData(spvFragmentationMassesData, ulRowCount, L" Frag Mass"));
    }

    HR(ProcesSpectrumParameters(spIMSSpectrum));

    return S_OK;
}

/*
This code is based on C# sample DumpAnalysis.cs but there is a bug in the C# sample
            // check if the cast was successful, then use HasSpectrumType
            if (spectrum2 != null)
            {
                // check whether there are Line Spectra
                if (spectrum2.HasSpecType(EDAL.SpectrumTypes.SpectrumType_Line))
                {
                    spectrum.GetMassIntensityValues(EDAL.SpectrumTypes.SpectrumType_Line, out masses, out intensities);
                    System.Array massesLine = (System.Array)masses;
                    System.Array intensitiesLine = (System.Array)intensities;

                    Console.WriteLine("   Line spectrum   : {0,6:d} masses and {1,6:d} intensities.", massesLine.Length, intensitiesLine.Length);
                }
-----> BUG should be SpectrumType_Profile not SpectrumType_Line <----
                // check whether there are Profile Spectra
                if (spectrum2.HasSpecType(EDAL.SpectrumTypes.SpectrumType_Line))
                {
                    spectrum.GetMassIntensityValues(EDAL.SpectrumTypes.SpectrumType_Profile, out masses, out intensities);
                    System.Array massesProfile = (System.Array)masses;
                    System.Array intensitiesProfile = (System.Array)intensities;

                    Console.WriteLine("   Profile spectrum: {0,6:d} masses and {1,6:d} intensities.", massesProfile.Length, intensitiesProfile.Length);
                }
            }

 */
HRESULT ProcesSpectrumTypeData(
           const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum,
           EDAL::SpectrumTypes spectrumType)
{
//    if (EDAL::SpectrumType_Line == spectrumType)
//    {
//        std::wcout << L"Line spectrum: ";
//    }

//    else if (EDAL::SpectrumType_Profile == spectrumType)
//    {
//        std::wcout << L"Profile spectrum: ";
//    }

    VARIANT vMasses = {0};
    VARIANT vIntensities = {0};
    CComSafeArray<double> spsaMasses;
    CComSafeArray<double> spsaIntensities;
    long lElementCount = 0;

    HR(spIMSSpectrum->GetMassIntensityValues(
        spectrumType,
        &vMasses,
        &vIntensities,
        &lElementCount));
    spsaMasses.Attach(V_ARRAY(&vMasses));
    spsaIntensities.Attach(V_ARRAY(&vIntensities));
    std::wcout
        << L"Masses dimensions: "
        << spsaMasses.GetDimensions()
        << L", Intensity dimensions: "
        << spsaIntensities.GetDimensions()
        << std::endl;
    std::wcout
        << spsaMasses.GetCount(0)
        << L" masses and "
        << spsaIntensities.GetCount(0)
        << L" intensities"
        << std::endl;
    std::wcout
        << L"-- Masses --"
        << std::endl;
    for (int iMassCount = spsaMasses.GetLowerBound(0);
         iMassCount <= spsaMasses.GetUpperBound(0);
         iMassCount++)
    {
//        std::wcout
//            << iMassCount
//            << L": "
//            << spsaMasses.GetAt(iMassCount)
//            << std::endl;
    }

    std::wcout
        << L"-- Intensities --"
        << std::endl;
    for (int iIntensitiesCount = spsaIntensities.GetLowerBound(0);
         iIntensitiesCount <= spsaIntensities.GetUpperBound(0);
         iIntensitiesCount++)
    {
//        std::wcout
//            << iIntensitiesCount
//            << L": "
//            << spsaMasses.GetAt(iIntensitiesCount)
//            << std::endl;
    }

    return S_OK;
}


void safeArrayToVectorDouble(CComSafeArray<double> & array, std::vector<double> & list)
{
    int iLower = array.GetLowerBound(0);
    int iUpper = array.GetUpperBound(0);

    list.clear();
    list.reserve(array.GetCount());
    for (int i = iLower; i <= iUpper; i++)
    {
        list.push_back(array.GetAt(i));
    }
}

HRESULT GetSpectrumPoints(
    const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum,
    EDAL::SpectrumTypes spectrumType,
    std::vector<double> &xlist, std::vector<double> & ylist)
{
//    if (EDAL::SpectrumType_Line == spectrumType)
//    {
//        std::wcout << L"Line spectrum: ";
//    }

//    else if (EDAL::SpectrumType_Profile == spectrumType)
//    {
//        std::wcout << L"Profile spectrum: ";
//    }

    VARIANT vMasses = { 0 };
    VARIANT vIntensities = { 0 };
    CComSafeArray<double> spsaMasses;
    CComSafeArray<double> spsaIntensities;
    long lElementCount = 0;

    HR(spIMSSpectrum->GetMassIntensityValues(
        spectrumType,
        &vMasses,
        &vIntensities,
        &lElementCount));
    spsaMasses.Attach(V_ARRAY(&vMasses));
    spsaIntensities.Attach(V_ARRAY(&vIntensities));
    /*
    std::wcout
        << L"Masses dimensions: "
        << spsaMasses.GetDimensions()
        << L", Intensity dimensions: "
        << spsaIntensities.GetDimensions()
        << std::endl;
    std::wcout
        << spsaMasses.GetCount(0)
        << L" masses and "
        << spsaIntensities.GetCount(0)
        << L" intensities"
        << std::endl;
    std::wcout
        << L"-- Masses --"
        << std::endl;
    */

    int imass_lower = spsaMasses.GetLowerBound(0);
    int imass_upper = spsaMasses.GetUpperBound(0);
    int imass_lower2 = spsaIntensities.GetLowerBound(0);
    int imass_upper2 = spsaIntensities.GetUpperBound(0);

    if (imass_lower != imass_lower2 ||
        imass_upper != imass_upper2 )
    {
        std::wcerr << "x and y sizes do not match" << std::endl;
        std::wcerr << "mass lower, mass upper, inten lower, inten upper=" << imass_lower << "," << imass_upper << "," << imass_lower2 << "," << imass_upper2 << std::endl;
        return S_FALSE;
    }

    safeArrayToVectorDouble(spsaMasses, xlist);
    safeArrayToVectorDouble(spsaIntensities, ylist);

    return S_OK;
}


HRESULT ProcesSpectrumData(const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum, EDAL::IMSSpectrum2 *pIMSSpectrum2)
{
    // check if pIMSSpectrum2 exists, then use HasSpectrumType
    if (NULL != pIMSSpectrum2)
    {
        VARIANT_BOOL vbHasSpecTypeLine;
        VARIANT_BOOL vbHasSpecTypeProfile;

        HR(pIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Line, &vbHasSpecTypeLine));
        HR(pIMSSpectrum2->HasSpecType(EDAL::SpectrumType_Profile, &vbHasSpecTypeProfile));
        if (VARIANT_TRUE == vbHasSpecTypeLine)
        {
            HR(ProcesSpectrumTypeData(spIMSSpectrum, EDAL::SpectrumType_Line));
        }

        else if (VARIANT_TRUE == vbHasSpecTypeProfile)
        {
            HR(ProcesSpectrumTypeData(spIMSSpectrum, EDAL::SpectrumType_Profile));
        }

        else
        {
            std::wcout << L"No spectrum type found with which to retrieve data" << std::endl;

            return E_FAIL;
        }
    }

    else
    {
        // we may get Line and Profile Spectra
        HR(ProcesSpectrumTypeData(spIMSSpectrum, EDAL::SpectrumType_Line));
        HR(ProcesSpectrumTypeData(spIMSSpectrum, EDAL::SpectrumType_Profile));
    }

    return S_OK;
}

HRESULT ProcesSpectrum(const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum)
{
    EDAL::IMSSpectrum2 *pIMSSpectrum2 = NULL;
    CComPtr<EDAL::IMSSpectrum2> spIMSSpectrum2;

    // IMSSpectrum2 may or may not be implemented so we ignore the return
    // value and check to see NULL!=pIMSSpectrum2
    (void)spIMSSpectrum->QueryInterface(
        __uuidof(EDAL::IMSSpectrum2),
        reinterpret_cast<void **>(&pIMSSpectrum2));
    if (NULL != pIMSSpectrum2)
    {
        spIMSSpectrum2.Attach(pIMSSpectrum2);
    }

    HR(ProcesSpectrumMetaData(spIMSSpectrum, pIMSSpectrum2));
    HR(ProcesSpectrumData(spIMSSpectrum, pIMSSpectrum2));
    return S_OK;
}

HRESULT Process(const std::wstring& kwstrDataFile)
{
    // This check is likely superflous but Open was returning an error
    // during debugging and this check insures the eror is not "file not found"
    if (FALSE == ::PathFileExists(kwstrDataFile.c_str()))
    {
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    }

    CComPtr<EDAL::IMSAnalysis2> spIMSAnalysis2;
    // use CComBSTR as it does not generate exceptions on error
    CComBSTR spbstrDataFile = kwstrDataFile.c_str();
    long spectrumItems = 0;

    std::wcout << L"Proessing Data File: " << kwstrDataFile << std::endl;
    HR(spIMSAnalysis2.CoCreateInstance(__uuidof(EDAL::MSAnalysis)));
    // The interface was generated with a TLI which mean calling Open throws an exception
    // an error so fall back on raw_ calls which is real form of Open for the interface
    HR(spIMSAnalysis2->Open(spbstrDataFile));
    HR(ProcesAnalysisMetaData(spIMSAnalysis2));

    EDAL::IMSSpectrumCollection *pSpectrumCollection = NULL;

    HR(spIMSAnalysis2->get_MSSpectrumCollection(&pSpectrumCollection));
    if (NULL != pSpectrumCollection)
    {
          CComPtr<EDAL::IMSSpectrumCollection> spSpectrumCollection;

        spSpectrumCollection.Attach(pSpectrumCollection);
        // JDN: this code is untested because get_MSSpectrumCollection returns a NULL
        // value of pSpectrumCollection
        HR(spSpectrumCollection->get_Count(&spectrumItems));
        // Vendor provided C# code showed index starting at 1 and max value <= applied.
        for (long spectrumCount = 1; spectrumCount <= spectrumItems; spectrumCount++)
        {
            EDAL::IMSSpectrum *pIMSSpectrum = NULL;
            CComPtr<EDAL::IMSSpectrum> spIMSSpectrum;

            std::wcout << L"Spectrum:" << spectrumCount << std::endl;
            HR(spSpectrumCollection->get_Item(spectrumCount, &pIMSSpectrum));
            spIMSSpectrum.Attach(pIMSSpectrum);
            HR(ProcesSpectrum(spIMSSpectrum));
        }
    }

    return S_OK;
}
