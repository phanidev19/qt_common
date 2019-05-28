/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include "ThermoMSFileReaderUtils.h"

#include <Windows.h>
#include <sdkddkver.h>
#include <stdio.h>
#include <tchar.h>

#include <iostream>
#include <algorithm>
#include <set>

#include "sqlite_utils.h"
//#include "common/PlotBase.h"
#include <QStringList>
#include <qt_utils.h>
//#include "QtSqlRowNodeUtils.h"
#include <math_utils_chemistry.h>
#include "ThermoAPITools.h"
#include "MSReaderBase.h"
#include "pmi_common_ms_debug.h"

#include <QCoreApplication>
#include <QDir>
#include <QStringList>

/////////////////////////////////
// Note on compile error (by Yong)
/////////////////////////////////
//The CL="/MP" allows visual studio compile using multiple cores.
//However, that's somehow causing the #import link to fail
//under visual studio that imports the qt project (or others, maybe).
//To resolve this issue, rebuild a second time; this works well enough.
//Some people ran into similar problem.
//http://stackoverflow.com/questions/230298/strategies-for-multicore-builds-mp-that-use-import
//http://msdn.microsoft.com/en-us/library/vstudio/bb385193.aspx
/////////////////////////////////

//#import "libid:F0C5F3E3-4F2A-443E-A74D-0AABE3237494" named_guids no_namespace
const CLSID CLSID_XRAW = {0x1d23188d, 0x53fe, 0x4c25, {0xb0, 0x32, 0xdc, 0x70, 0xac, 0xdb, 0xdc,  0x02}};
const IID IID_IXRAW5 ={0x06F53853, 0xE43C, 0x4F30, {0x9E, 0x5F, 0xD1, 0xB3, 0x66, 0x8F, 0x0C, 0x3C}};
//#import "C:\\Users\\kily\\zdev\\panda\\libraries\\thermo\\XRawfile2.dll" no_namespace
//const IID IID_IXRAW = {0x11B488A0, 0x69B1, 0x41FC, {0xA6, 0x60, 0xFE, 0x8D, 0xF2, 0xA3, 0x1F, 0x5B}};
//const IID IID_IXRAW3 ={0x19a00b1e, 0x1559, 0x42b1, {0x9a, 0x46, 0x08, 0xa5, 0xe5, 0x99, 0xed, 0xee}};  //raw3

// Note: generate .tlh and .tli file by first using #import "pwiz-pmi\\xx.dll"
//#import "pwiz-pmi\\msfilereader.xrawfile2.dll" no_namespace

#pragma warning(disable:4067)
#include <vendor_api\Thermo\msfilereader.xrawfile2.tlh>

using namespace std;

static int open_count = 0;
static int close_count = 0;

struct FullMSOrderPrecursorInfo {
    double dPrecursorMass;
    double dIsolationWidth;
    double dCollisionEnergy;
    UINT uiCollisionEnergyValid; // set to 1 to use in scan filtering. High-order bits hold the
    // activation type enum bits 0xffe and the flag for multiple
    // activation (bit 0x1000). You can implement yhese features
    // individually with the new access functions or as a UINT
    // with the new CollisionEnergyValueEx function.
    BOOL bRangeIsValid; // If TRUE, dPrecursorMass is still the center mass, but
    // dFirstPrecursorMass and dLastPrecursorMass are also
    // valid.
    double dFirstPrecursorMass; // If bRangeIsValid == TRUE, this value defines the start of
    // the precursor isolation range.
    double dLastPrecursorMass; // If bRangeIsValid == TRUE, this value defines the end of the
    // precursor isolation range.
    double dIsolationWidthOffset;
};

_PMI_BEGIN
_MSREADER_BEGIN

//For RAW reading; need to understand this better later.
//const long ticControllerType=0, ticControllerNumber=1;
const long ticControllerType = 0;
const long ticControllerNumber = 1;
const long uvControllerType = 4;
const long uvControllerNumber = 1;

// example for GetFullMSOrderPrecursorDataFromScanNum
Err OnOpenParentScansOcx(IXRawfile5 * praw, long nScanNumber, FullMSOrderPrecursorInfo *info);


inline void safe_SysFreeString(BSTR & bstrName)
{
    if (bstrName) {
        SysFreeString(bstrName);
        bstrName = NULL;
    }
}

/*
Err getRAWInfo(const char * _fileName, TableEntry & machineFiles) {
    Err e = kNoErr;
    IXRawfile5 * praw=NULL;
    HRESULT hr;
    string fileName = trim(string(_fileName));

    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_XRAW, NULL, CLSCTX_INPROC_SERVER, IID_IXRAW5, (void **) &praw);
    if (hr != 0) {
        cerr << "Could not open file: " << fileName << endl;
        e = pmi::kFileOpenError; eee;
    }

    hr = praw->Open(fileName.c_str());
    if (hr==0){
        //std::cout<<"Successfully opened raw file!\n";

        long nControllerType =0;
        long nControllerNumber =1;
        long nRet = praw->SetCurrentController(nControllerType, nControllerNumber);

        //NumSpectra
        long num_scans = 0;
        nRet = praw->GetNumSpectra(&num_scans);
        //std::cout<<"Number of scans is: "<<num_scans<<std::endl;
        machineFiles.setEntry("NumSpectra", num_scans);

        //VersionNumber
        long pnVersion = 0;
        nRet = praw->GetVersionNumber(&pnVersion);
        machineFiles.setEntry("VersionNumber", pnVersion);

        //GetFileName
        BSTR filename=NULL;
        nRet = praw->GetFileName(&filename);
        char newstring [MAX_PATH];
        WideCharToMultiByte (CP_ACP, WC_COMPOSITECHECK, filename, -1, newstring, sizeof(newstring), NULL, NULL);
        std::cout<<newstring<<endl;
        //error with this: if (filename) free(filename);

        //411 Get these info? Possible?
        //machineFiles.setEntryApostrophe("DynamicExclusion", string("On"));
        //machineFiles.setEntryApostrophe("Fragmentation", "CID(hard-coded)");
    }

error:
    if (praw) {
        praw->Close();
        praw->Release();
        CoUninitialize();
    }
    return e;
}
*/
Err extractMassListAtScanNumber_FromLabeledData(IXRawfile5 * praw, long scan_number, std::vector<point2d> & pointList, bool sortByX) {
    Err e = kNoErr;
    pointList.clear();
    if (scan_number <= 0)
        return e;

    _variant_t labels;
    //VARIANT varFlags; VariantInit(&varFlags);

    long nRet = praw->GetLabelData(&labels, NULL, &scan_number);
    if( nRet != 0 ) {
        std::cerr << "Could not read GetLabelData at scan_number=" << scan_number << ". nRet message=" << nRet << std::endl;
        e = kRawReadingError; eee;
    }

    if (labels.vt != (VT_ARRAY | VT_R8)) {
        std::cerr << "Invalid label values" << endl;
        e = kRawReadingError; eee;
    }

    if (1) {
        _variant_t labels2(labels, false);
        long size_ = (long) labels2.parray->rgsabound[0].cElements;
        pointList.reserve(size_);
        double* pdval = (double*) labels2.parray->pvData;
        point2d point;
        for(long i=0; i < size_; ++i)
        {
            point.rx() = (double) pdval[(i*6)+0];
            point.ry() = (double) pdval[(i*6)+1];
            pointList.push_back(point);
        }
    }
    if (sortByX) {
        sort(pointList.begin(), pointList.end(), point2d_less_x);
    }

error:
    return e;
}


Err extractMassListAtScanNumber(IXRawfile5 * praw, long scan_number, std::vector<point2d> & pointList, bool sortByX, int do_centroid) {
    Err e = kNoErr;
    typedef struct _datapeak {
        double dMass;
        double dIntensity;
    } DataPeak;

    pointList.clear();
    if (scan_number <= 0)
        return e;

    VARIANT varMassList; VariantInit(&varMassList);
    VARIANT varPeakFlags; VariantInit(&varPeakFlags);
    long nArraySize = 0;
    //http://www.codeguru.com/forum/archive/index.php/t-122576.html
    //Initially I found some code that converted a long to a _variant_t and then from a _variant_t to _bstr_t:
    //int nNumber = 1234;
    //_bstr_t mylong = (_variant_t)(long)nNumber;
    //However, when I removed (_variant_t), the code still compiled. I realize that MSDN's description of _bstr_t does NOT include long, and MSDN's description of _variant_t does include long, but the code compiles and executes. Is it better to code it the other way?
    //_bstr_t funky_compile_issue = (_bstr_t)(_variant_t)(0); //error C2664: 'IXRawfile::GetMassListFromRT' : cannot convert parameter 2 from 'int' to '_bstr_t'
    BSTR bstrFilter = 0;
    double centroidPeakWidth = 0;

    long nRet = praw->GetMassListFromScanNum(&scan_number,
                                        bstrFilter, // no filter
                                        0, // no cutoff
                                        0, // no cutoff
                                        0, // all peaksreturned
                                        do_centroid, // do not centroid
                                        &centroidPeakWidth, //pdCentroidPeakWidth
                                        &varMassList, // mass list data
                                        &varPeakFlags, // peak flags data
                                        &nArraySize ); // size of mass list array

    if( nRet != 0 ) {
        std::cerr << "Could not read GetMassListFromScanNum at scan_number=" << scan_number << ". nRet message=" << nRet << std::endl;
        e = kRawReadingError; eee;
    }

    //cout << "nArraySize = " << nArraySize << endl;

    if( nArraySize >= 0) {
        // Get a pointer to the SafeArray
        SAFEARRAY FAR* psa = varMassList.parray;
        DataPeak* pDataPeaks = NULL;
        SafeArrayAccessData( psa, (void**)(&pDataPeaks) );
        point2d p;
        pointList.resize(nArraySize);
        for( long j=0; j<nArraySize; j++ ) {
            p.rx() = pDataPeaks[j].dMass;
            p.ry() = pDataPeaks[j].dIntensity;
            pointList[j] = p;
        }
        // Release the data handle
        SafeArrayUnaccessData( psa );
    }

    if (sortByX) {
        bool isSortedByX = std::is_sorted(pointList.begin(), pointList.end(), point2d_less_x);
        if (!isSortedByX) {
            sort(pointList.begin(), pointList.end(), point2d_less_x);
        }
    }

error:
    if( varMassList.vt != VT_EMPTY ) {
        SAFEARRAY FAR* psa = varMassList.parray;
        varMassList.parray = NULL;
        // Delete the SafeArray
        SafeArrayDestroy( psa );
    }
    if(varPeakFlags.vt != VT_EMPTY ) {
        SAFEARRAY FAR* psa = varPeakFlags.parray;
        varPeakFlags.parray = NULL;
        // Delete the SafeArray
        SafeArrayDestroy( psa );
    }
    return e;
}

Err
getRawScanNumberToPointListSortByX(RawFileHandle rawHandle, long scanNumber, std::vector<point2d> & pointList) {
    Err e = kNoErr;
    IXRawfile5 * praw=(IXRawfile5*)rawHandle;

    pointList.clear();
    if (praw == NULL) {
        e = kBadParameterError; eee;
    }
    e = extractMassListAtScanNumber(praw, scanNumber, pointList, true, FALSE); eee;

error:
    return e;
}

Err
getRawScanNumberToPointListSortByX_centroid(RawFileHandle rawHandle, thermo::InstrumentModelType modelType, long scanNumber, std::vector<point2d> & pointList) {
    Err e = kNoErr;
    IXRawfile5 * praw=(IXRawfile5*)rawHandle;
    long nRet = 0;
    thermo::MassAnalyzerType matype = thermo::MassAnalyzerType_Unknown;
    long nType = -1;

    pointList.clear();
    if (praw == NULL) {
        e = kBadParameterError; eee;
    }
    nRet = praw->GetMassAnalyzerTypeForScanNum(scanNumber, &nType);
    if (nRet != 0) {
    }
    matype = thermo::convertScanFilterMassAnalyzer((thermo::ScanFilterMassAnalyzerType) nType, modelType);

    if (matype == thermo::MassAnalyzerType_Orbitrap || matype == thermo::MassAnalyzerType_FTICR) {
        //cout << "calling LabeledData" << endl;
        e = extractMassListAtScanNumber_FromLabeledData(praw, scanNumber, pointList, true); eee;
    } else {
        //cout << "calling ScanNumber" << endl;
        e = extractMassListAtScanNumber(praw, scanNumber, pointList, true, TRUE); eee;
    }

error:
    return e;
}

Err SetRawCurrentController(RawFileHandle rawHandle, long nControllerType, long nControllerNumber) {
    IXRawfile5 * praw=(IXRawfile5*)rawHandle;
    if (praw) {
        long nRet = praw->SetCurrentController(nControllerType, nControllerNumber);
        if (nRet != 0) {
            debugMs() << "Could not set raw current controller to:" << nControllerType << " " << nControllerNumber;
            return kBadParameterError;
        }
    } else {
        return kBadParameterError;
    }
    return kNoErr;
}

//http://stackoverflow.com/questions/27220/how-to-convert-stdstring-to-lpcwstr-in-c-unicode
static std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

Err CreateRawInstanceAndOpenFile(const string & _fileName, RawFileHandle * rawHandle, long nControllerType, long nControllerNumber) {
    Err e = kNoErr;
    IXRawfile5 * praw=NULL;
    HRESULT hr = 0;
    *rawHandle = NULL;
    ULONG_PTR lpCookie = 0;
    const wstring fileName = QString::fromStdString(pmi::trim(_fileName)).toStdWString();
    HANDLE pActCtx = INVALID_HANDLE_VALUE;

    //commonMsDebug() << "About to open handle for: [" << fileName.c_str() << "]" << endl;

    //Note: CoIntialize_RAII does work.  But have to be careful when this function is called through a seperate dll,
    //such as the new_project.dll.  That also requires CoIntialize_RAII instance.
    //CoInitialize(NULL);

    //Try and load the manifest relative to the application executable.
    //http://blogs.msdn.com/b/gpalem/archive/2007/03/26/avoid-registration-free-com-manifest-problems-with-activation-context-api.aspx
    ACTCTX actctx;

    ZeroMemory(&actctx, sizeof(actctx));

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString dll_manifest_path = appDir.absoluteFilePath("MSFileReader.XRawfile2.SxS.manifest");

    if (dll_manifest_path.size() > 0) {
        actctx.cbSize = sizeof(actctx);
        std::wstring wstr = s2ws(dll_manifest_path.toStdString());
        actctx.lpSource = wstr.c_str(); //Note: once wstr is out of scope, so is this lpSource
        //actctx.lpSource = L"C:\\Users\\ykil\\dev\\qt_viewers\\byomap\\debug\\pwiz-pmi\\MSFileReader.XRawfile2.SxS.manifest";
        pActCtx = CreateActCtx(&actctx);
        if(pActCtx != INVALID_HANDLE_VALUE) {
            //Try loading with using ACTCTX
            if(ActivateActCtx(pActCtx, &lpCookie)) {
                hr = CoCreateInstance(CLSID_XRAW, NULL, CLSCTX_INPROC_SERVER, IID_IXRAW5, (void **) &praw);
                if (FAILED(hr)) {
                    qCritical().noquote().nospace()
                        << "CoCreateInstance failed: 0x" << QString::number(hr, 16);
                    // continue
                }
            }
        }
        if (lpCookie) {
            DeactivateActCtx(0, lpCookie);
            lpCookie = NULL;
        }
        if (pActCtx != INVALID_HANDLE_VALUE) {
            ReleaseActCtx(pActCtx);
            pActCtx = INVALID_HANDLE_VALUE;
        }

    }

    //Otherwise, try and load it the previous way, from the installed version (via registery)
    if (praw == NULL) {
        static int count = 0;
        if (count++ <= 5) {
            debugMs() << "Could not use ACTCTX. Attempting to load instance without it.";
        }
        //Try loading without using ACTCTX
        hr = CoCreateInstance(CLSID_XRAW, NULL, CLSCTX_INPROC_SERVER, IID_IXRAW5, (void **) &praw);
    }
    if (hr != 0) {
        static int count = 0;
        if (count++ < 5) {
            if (count >= 5) {
                warningMs() << "Last warning:";
            }
            warningMs() << "Could not create RAW file reading instance." << endl;
            warningMs() << "You may need to install MSFileReader (32-bit) ." << endl;
            warningMs() << "Try downloading from http://sjsupport.thermofinnigan.com/public/detail.asp?id=703" << endl;
            //e = pmi::kFileOpenError; eee;  not really an error.
        }
        goto error;
    }


    open_count++;
    if (open_count < 1000) {
        cout << "CreateRawInstanceAndOpenFile: praw, open_count, close_count=" << praw << "," << open_count << "," << close_count << " diff=" << open_count - close_count << endl;
    }

    hr = praw->Open(fileName.c_str());
    if (hr != 0) {
        std::wcerr << "Could not open RAW file: [" << fileName << "], result: 0x" << std::hex << hr << std::endl;
        BSTR pbstrErrorMessage = NULL;
        praw->GetErrorMessage(&pbstrErrorMessage);
        if (pbstrErrorMessage != NULL) {
            std::wcerr << "Error message=" << pbstrErrorMessage << std::endl ;
            SysFreeString(pbstrErrorMessage);
        }
        e = pmi::kFileOpenError; eee;
    }
//    long nControllerType =0;
//    long nControllerNumber =1;
    long nRet = praw->SetCurrentController(nControllerType, nControllerNumber);
    if (nRet != 0) {
        warningMs() << "Could not set RAW controller to: controler_type=" << nControllerType << " controller_number=" << nControllerNumber << endl;
        e = pmi::kError; eee;
    }

    *rawHandle = (void*)praw;

error:
    if (lpCookie) {
        DeactivateActCtx(0, lpCookie);
    }
    if (pActCtx != INVALID_HANDLE_VALUE) {
        ReleaseActCtx(pActCtx);
    }
    if (e != kNoErr) {
        if (praw) {
            praw->Close();
            praw->Release();
            //CoUninitialize();  //keep this to pair CoInitialize
        }
        *rawHandle = NULL;
    }
    return e;
}

Err CloseAndDestroyRawInstance(RawFileHandle & rawHandle) {
    Err e = kNoErr;
    IXRawfile5 * praw=NULL;
    praw = (IXRawfile5 *) rawHandle;
    close_count++;

    if (praw) {
        praw->Close();
        praw->Release();
        //CoUninitialize(); //keep this to pair CoInitialize
    }
    rawHandle = NULL;

//error:
    return e;
}

static Err
get_trailing_value_as_double(IXRawfile5 * praw, long scan_num, const string & key, double * outval) {
    Err e = kNoErr;
    _variant_t v;
    *outval = 0;

    long nRet = praw->GetTrailerExtraValueForScanNum(scan_num, key.c_str(), &v);
    if( nRet != 0 ) {
        e = kBadParameterError; eee;
    }
    switch (v.vt)
    {
        case VT_R4: *outval = v.fltVal; break;
        case VT_R8: *outval = v.dblVal; break;
        default: e = kBadParameterError; eee; break;
    }
error:
    return e;
}


static Err
get_trailing_value_as_long(IXRawfile5 * praw, long scan_num, const string & key, long * outval) {
    Err e = kNoErr;
    _variant_t v;

    long nRet = praw->GetTrailerExtraValueForScanNum(scan_num, key.c_str(), &v);
    if( nRet != 0 ) {
        e = kBadParameterError; nee;
    }
    switch (v.vt)
    {
        case VT_I1: *outval = v.cVal; break;
        case VT_I2: *outval = v.iVal; break;
        case VT_I4: *outval = v.lVal; break;
        case VT_UI1: *outval = v.bVal; break;
        case VT_UI2: *outval = v.uiVal; break;
        case VT_UI4: *outval = v.ulVal; break;
        case VT_INT: *outval = v.intVal; break;
        case VT_UINT: *outval = v.uintVal; break;
        default: e = kBadParameterError; eee; break;
    }
error:
    return e;
}

Err getPrecursorInfoViaFilterAndTrailerExtraValueString(RawFileHandle rawHandle, long scan_num, PrecursorInfo & xpreInfo) {
    Err e = kNoErr;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    long parent_scannumber = -1;
    string precursorMassStr, peakModeStr, scanMethodStr;
    xpreInfo.clear();

    //Find this ms level and precursor mass
    //TODO: for precursor mass, also check via GetTrailerExtraValueForScanNum's Mono m/z; which is better? I think the mono m/z is more accurate
    int this_mslevel = 1;
    e = getMSLevel(rawHandle, scan_num, &this_mslevel, &precursorMassStr, &peakModeStr, &scanMethodStr); eee;
    if (this_mslevel <= 1) {
        return e;
    }
    xpreInfo.dIsolationMass = toDouble(precursorMassStr);
    xpreInfo.dMonoIsoMass = xpreInfo.dIsolationMass; //TODO, find via mono m/z

    //Find scan number (of MS2) as the most previous ms level (e.g. MS1)
    for (long scan = scan_num-1; scan >= 1; scan--) {
        int level=1;
        e = getMSLevel(rawHandle, scan, &level, NULL, &peakModeStr, &scanMethodStr); eee;
        if (level == (this_mslevel-1)) {
            parent_scannumber = scan;
            break;
        }
    }
    if (parent_scannumber >= 0) {
        xpreInfo.nScanNumber = parent_scannumber;
        xpreInfo.nativeId = makeThermoNativeId(xpreInfo.nScanNumber);
    }

    e = get_trailing_value_as_long(praw, scan_num, "Charge State:", &xpreInfo.nChargeState);

error:
    return e;
}

//Note: do not change this struct as it's intimately linked to the Thermo API call below.
struct PrecursorInfo_RAW
{
    double dIsolationMass;
    double dMonoIsoMass;
    long nChargeState;
    long nScanNumber;
};

Err
getPrecursorInfo(RawFileHandle rawHandle, long scan_num, PrecursorInfo & xpreInfo) {
    Err e = kNoErr;
    VARIANT vPrecursorInfos;
    vPrecursorInfos.vt = VT_EMPTY;
    VariantInit(&vPrecursorInfos);
    PrecursorInfo_RAW preInfo;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    xpreInfo.clear();
    int has_precursor_info = 0;
//    if (xhas_precursor_info)
//        *xhas_precursor_info = 0;

    try
    {
        long nPrecursorInfos = 0;
        // Get the precursor scan information
        praw->GetPrecursorInfoFromScanNum(scan_num,
            &vPrecursorInfos,
            &nPrecursorInfos);
        // Access the safearray buffer
        BYTE* pData;
        SafeArrayAccessData(vPrecursorInfos.parray, (void**)&pData);
        if (nPrecursorInfos > 1) {
            cerr << "Warning: nPrecursorInfos = " << nPrecursorInfos << endl;
            nPrecursorInfos = 1;
        }
        for (int i=0; i < nPrecursorInfos; ++i)
        {
            // Copy the scan information from the safearray buffer
            memcpy(&preInfo,
                pData + i * sizeof(MS_PrecursorInfo),
                sizeof(PrecursorInfo_RAW));
            has_precursor_info = 1;
            // Process the paraent scan information ...
        }
        SafeArrayUnaccessData(vPrecursorInfos.parray);
    }
    catch (...)
    {
        e = pmi::kRawReadingError; eee;
    }

    if (has_precursor_info) {
        xpreInfo.dIsolationMass = preInfo.dIsolationMass;
        xpreInfo.dMonoIsoMass = preInfo.dMonoIsoMass;
        xpreInfo.nChargeState = preInfo.nChargeState;
        xpreInfo.nScanNumber = preInfo.nScanNumber;
        xpreInfo.nativeId = makeThermoNativeId(preInfo.nScanNumber);
    } else {
        e = getPrecursorInfoViaFilterAndTrailerExtraValueString(rawHandle, scan_num, xpreInfo); eee;
    }

//    if (xhas_precursor_info)
//        *xhas_precursor_info = has_precursor_info;

error:
    if(vPrecursorInfos.vt != VT_EMPTY ) {
        SAFEARRAY FAR* psa = vPrecursorInfos.parray;
        vPrecursorInfos.parray = NULL;
        // Delete the SafeArray
        SafeArrayDestroy( psa );
    }
    return e;
}

Err
getRT(RawFileHandle rawHandle, long scan_num, double* dRT) {
    Err e = kNoErr;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;

    long nRet = praw->RTFromScanNum( scan_num, dRT);
    if( nRet != 0 ) {
        e = kRawReadingError; eee;
    }
error:
    return e;
}

Err
getScanNumberFromRT(RawFileHandle rawHandle, double retTime, long * scanNumber) {
    Err e = kNoErr;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;

    long nRet = praw->ScanNumFromRT( retTime, scanNumber);
    if( nRet != 0 ) {
        std::cerr << "ScanNumFromRT retTime=" << retTime << std::endl;
        e = kRawReadingError; eee;
    }
error:
    return e;
}

static std::string toString(BSTR bstr_string) {
    char newstring[1028];
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, bstr_string, -1, newstring, sizeof(newstring), NULL, NULL);
    return string(newstring);
}

Err
getMSLevel(RawFileHandle rawHandle, long scan_num, int * level, std::string * precursorMassStr, std::string * peakModeStr, std::string * scanMethodStr)
{
    Err e = kNoErr;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    if (peakModeStr)
        peakModeStr->clear();
    if (scanMethodStr)
        scanMethodStr->clear();
    BSTR filter_scan=NULL;

    *level = 1;
    try {
        int ret = praw->GetFilterForScanNum(scan_num, &filter_scan);
        string line = toString(filter_scan);
        if (ret != 0) {
            cerr << "Error for scan_num,line=" << scan_num << "," << line << endl;
            e = kError; eee;
        }
        //"FTMS + p ESI Full ms [300.00-2000.00]"
        //"ITMS + c ESI d Full ms2 624.33@cid35.00 [160.00-1885.00]"
        //ITMS + p NSI d Z ms [324.00-334.00]
        //ITMS + p NSI d Full ms2 413.19@cid35.00 [100.00-840.00]
        //ITMS + c NSI SIM ms [428.00-438.00,646-651.50, 669.50-684.50]
        //FTMS + p ESI sid=0.50  d Full msx ms2 462.24@hcd27.50 165.60@hcd27.50 [50.00-490.00]

        bool found_ms_level = false;
        int found_precursor_mass = 0;
        vecstr list = split(line, " ");
        unsigned int i = 0;
        //Find 'ms' 'ms2' 'ms10'
        //Also find 'p' 'c' 'Full' 'Z' scan mode and method
        for (; i < list.size(); i++) {
            const string & str = list[i];
            if (str == "p" && peakModeStr) {
                *peakModeStr = "p";
            } else if (str == "c" && peakModeStr) {
                *peakModeStr = "c";
            } else if (str == "Full" && scanMethodStr) {
                *scanMethodStr = "Full";
            } else if (str == "Z" && scanMethodStr) {
                *scanMethodStr = "Z";
            } else if (startsWith(str, "ms")) {
                //Note str can be "ms2" or "ms" or "msx".  If "msx", we want to continue our search.
                if (str.size() == 2) { //if "ms" it's level 1.
                    //ms level 1 found.
                    found_ms_level = true;
                    break; //break here is OK since we expect to find p/c Full/Z before hitting the "ms" string.
                }
                string number = str.substr(2);
                if (isInteger(number)) {
                    found_ms_level = true;
                    *level = atoi(number.c_str());
                    //Note: break here is OK since we expect to find p/c Full/Z before hitting the "ms" string.
                    break;
                }
                //Likely it found "msx", which we will ignore.
            }
        }
        if (!found_ms_level) {
            e = pmi::kRawReadingError; eee;
        }

        //Find '624.33' in '624.33@cid35.00'
        if (precursorMassStr) {
            precursorMassStr->clear();
            for (; i < list.size(); i++) {
                const string & str = list[i];
                if (contains(str, "@")) {
                    vecstr listx = split(str, "@");
                    if (listx.size() > 0) {
                        *precursorMassStr = listx.at(0);
                        found_precursor_mass = 1;
                    }
                    break;
                }
            }
            if (!found_precursor_mass) {
                debugMs() << "precursor mass not found for scan number=" << scan_num << "with line content: " << line.c_str();
                e = pmi::kRawReadingError; eee;
            }
        }
    } catch (...) {
        e = pmi::kRawReadingError; eee;
        //throw new Exception(ex.ToString());
    }
error:
    safe_SysFreeString(filter_scan);  //This was causing a lot of memory leak
    return e;
}

//const TableExtract & t_ext,
Err getChroPoints(RawFileHandle rawHandle, long channel, vector<point2d> & pointList, bool sortByX) {
    Err e = kNoErr;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    point2d point;

    pointList.clear();

    if (1) {
        // example for GetChroData to return the UV trace
        typedef struct _datapeak
        {
            double dTime;
            double dIntensity;
        } ChroDataPeak;
        //praw->SetCurrentController ( 4, 1 ); // first UV controller
        VARIANT varChroData;
        VariantInit(&varChroData);
        VARIANT varPeakFlags;
        VariantInit(&varPeakFlags);
        long nArraySize = 0;
        double dStartTime = 0.0;
        double dEndTime = 0.0;
        BSTR bstrStrZero1 = 0;
        BSTR bstrStrZero2 = 0;
        BSTR bstrStrZero3 = 0;
        long nRet = praw->GetChroData ( channel, // Chro Type 1: channel A
            0, //?
            0, //Chro Type 2: Channel B
            bstrStrZero1,
            bstrStrZero2,
            bstrStrZero3,
            0.0,
            &dStartTime,
            &dEndTime,
            0,
            0,
            &varChroData,
            &varPeakFlags,
            &nArraySize );
            if( nRet != 0 ) {
                debugMs() << "Error reading praw->GetChroData with channel = " << channel;
                e = kRawReadingError; eee;
            }
            if( nArraySize )
            {
                // Get a pointer to the SafeArray
                SAFEARRAY FAR* psa = varChroData.parray;
                ChroDataPeak* pDataPeaks = NULL;
                SafeArrayAccessData( psa, (void**)(&pDataPeaks) );
                //double dTime;
                //double dIntensity;
                for( long j=0; j<nArraySize; j++ )
                {
                    point.rx() = pDataPeaks[j].dTime;
                    point.ry() = pDataPeaks[j].dIntensity;
                    pointList.push_back(point);
                    // Do something with time intensity values
                }
                // Release the data handle
                SafeArrayUnaccessData( psa );
            }
            if(varChroData.vt != VT_EMPTY )
            {
                SAFEARRAY FAR* psa = varChroData.parray;
                varChroData.parray = NULL;
                // Delete the SafeArray
                SafeArrayDestroy( psa );
            }
            if(varPeakFlags.vt != VT_EMPTY )
            {
                SAFEARRAY FAR* psa = varPeakFlags.parray;
                varPeakFlags.parray = NULL;
                // Delete the SafeArray
                SafeArrayDestroy( psa );
            }
    }

    if (sortByX) {
        sort(pointList.begin(), pointList.end(), point2d_less_x);
    }

error:
    return e;
}


//const TableExtract & t_ext,
Err getTICPointList_AutoSetCurrentController(RawFileHandle rawHandle, vector<point2d> & pointList, bool sortByX) {
    Err e = kNoErr;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    point2d point;

    pointList.clear();

    if (1) {
        // example for GetChroData to return the MS TIC trace
        typedef struct _datapeak
        {
            double dTime;
            double dIntensity;
        } ChroDataPeak;
        praw->SetCurrentController ( 0, 1 ); // first MS controller
        VARIANT varChroData;
        VariantInit(&varChroData);
        VARIANT varPeakFlags;
        VariantInit(&varPeakFlags);
        long nArraySize = 0;
        double dStartTime = 0.0;
        double dEndTime = 0.0;
        BSTR bstrStr_szFilter = SysAllocString(L"Full ms");  //Need this to make sure it doesn't pick up garbage/low-intensive ms1
        BSTR bstrStrZero2 = 0;
        BSTR bstrStrZero3 = 0;
        long nRet = praw->GetChroData ( 1, // TIC trace
            0,
            0, //0 or 1 didn't make a difference
            bstrStr_szFilter,
            bstrStrZero2,
            bstrStrZero3,
            0.0,
            &dStartTime,
            &dEndTime,
            0,
            0,
            &varChroData,
            &varPeakFlags,
            &nArraySize );

            /// free allocation of BSTR string
            SysFreeString(bstrStr_szFilter);

            if( nRet != 0 ) {
                e = kRawReadingError; eee;
            }
            if( nArraySize )
            {
                // Get a pointer to the SafeArray
                SAFEARRAY FAR* psa = varChroData.parray;
                ChroDataPeak* pDataPeaks = NULL;
                SafeArrayAccessData( psa, (void**)(&pDataPeaks) );
                //double dTime;
                //double dIntensity;
                for( long j=0; j<nArraySize; j++ )
                {
                    point.rx() = pDataPeaks[j].dTime;
                    point.ry() = pDataPeaks[j].dIntensity;
                    pointList.push_back(point);
                    // Do something with time intensity values
                }
                // Release the data handle
                SafeArrayUnaccessData( psa );
            }
            if(varChroData.vt != VT_EMPTY )
            {
                SAFEARRAY FAR* psa = varChroData.parray;
                varChroData.parray = NULL;
                // Delete the SafeArray
                SafeArrayDestroy( psa );
            }
            if(varPeakFlags.vt != VT_EMPTY )
            {
                SAFEARRAY FAR* psa = varPeakFlags.parray;
                varPeakFlags.parray = NULL;
                // Delete the SafeArray
                SafeArrayDestroy( psa );
            }
    }

    if (sortByX) {
        sort(pointList.begin(), pointList.end(), point2d_less_x);
    }

error:
    return e;
}

Err extractRAWXIC(RawFileHandle rawHandle, const XICWindow & xnfo, vector<point2d> & pointList) {
    Err e = kNoErr;
    IXRawfile5 * praw = (IXRawfile5 *)rawHandle;

    typedef struct _datapeak {
        double dTime;
        double dIntensity;
    } ChroDataPeak;

    point2d pdouble;
    VARIANT varChroData;
    VariantInit(&varChroData);
    VARIANT varPeakFlags;
    VariantInit(&varPeakFlags);
    long nArraySize = 0;

    double dStartTime = xnfo.time_start;
    double dEndTime = xnfo.time_end;
    int precision = 6; //6 decimal places after the dot is sufficient; using 15 causes it to return nArraySize of zero in GetChroData()
    string massRange = pmi::toString(xnfo.mz_start,precision)+"-"+pmi::toString(xnfo.mz_end,precision);

    long nRet = praw->GetChroData(
        0,                   //long nChroType1,
        0,                     //long nChroOperator,
        0,                     //long nChroType2,
        "Full ms ",             //_bstr_t bstrFilter,
        massRange.c_str(),     //_bstr_t bstrMassRanges1,
        "",                     //_bstr_t bstrMassRanges2,
        0.0,                 //double dDelay,
        &dStartTime,         //double * pdStartTime,
        &dEndTime,             //double * pdEndTime,
        0,                     //long nSmoothingType,
        0,                     //long nSmoothingValue,
        &varChroData,         //VARIANT * pvarChroData,
        &varPeakFlags,         //VARIANT * pvarPeakFlags,
        &nArraySize );         //long * pnArraySize

    if (nArraySize <= 0) {
        cout << "Warning, GetChroData() did not provide any data for massRange: " << massRange << " and time range of: " << dStartTime <<"-" <<dEndTime << endl;
    }

    if( nRet != 0 ) {
        cout << "Error getting chro data. nRet = " << nRet << endl;
        e = pmi::kRawReadingError; eee;
    }

    pointList.clear();

    if( nArraySize ) {
        // Get a pointer to the SafeArray
        SAFEARRAY FAR* psa = varChroData.parray;
        ChroDataPeak* pDataPeaks = NULL;
        SafeArrayAccessData( psa, (void**)(&pDataPeaks) );
        for( long j=0; j<nArraySize; j++ )
        {
            pdouble.rx() = pDataPeaks[j].dTime;
            pdouble.ry() = pDataPeaks[j].dIntensity;
            pointList.push_back(pdouble);
        }
        // Release the data handle
        SafeArrayUnaccessData( psa );
    }

error:
    if(varChroData.vt != VT_EMPTY ) {
        SAFEARRAY FAR* psa = varChroData.parray;
        varChroData.parray = NULL;
        // Delete the SafeArray
        SafeArrayDestroy( psa );
    }
    if(varPeakFlags.vt != VT_EMPTY ) {
        SAFEARRAY FAR* psa = varPeakFlags.parray;
        varPeakFlags.parray = NULL;
        // Delete the SafeArray
        SafeArrayDestroy( psa );
    }
    return e;
}

Err RAW_getNumberOfSpectra(RawFileHandle rawHandle, long * num_scans, long * first_scan, long * last_scan) {
    Err e = kNoErr;
    long nRet = 0;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;

    nRet += praw->GetNumSpectra(num_scans);
    nRet += praw->GetFirstSpectrumNumber(first_scan);
    nRet += praw->GetLastSpectrumNumber(last_scan);

    if (nRet != 0) {
        e = kError; eee;
    }
error:
    return e;
}

Err RAW_getFragmentType(RawFileHandle rawHandle, long scan_num, long level, QString &fragmentTypeStr) {
    Err e = kNoErr;
    BSTR bstr = NULL;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    fragmentTypeStr.clear();
    long activationType = -1;
    long nRet = praw->GetActivationTypeForScanNum( scan_num, level, &activationType);
    if( nRet != 0 ) {
        e = kRawReadingError; eee;
    }

    switch (activationType) {
        case 0 : fragmentTypeStr = "cid"; break;
        case 1 : fragmentTypeStr = "mpd"; break;
        case 2 : fragmentTypeStr = "ecd"; break;
        case 3 : fragmentTypeStr = "pqd"; break;
        case 4 : fragmentTypeStr = "etd"; break;
        case 5 : fragmentTypeStr = "hcd"; break;
        case 6 : fragmentTypeStr = "any"; break;
        case 7 : fragmentTypeStr = "sa"; break;
        case 8 : fragmentTypeStr = "ptr"; break;
        case 9 : fragmentTypeStr = "netd"; break;
        case 10: fragmentTypeStr = "nptr"; break;
        default: fragmentTypeStr = "unknown"; break;
    }

    if (fragmentTypeStr == "etd")
    {
        // Check for EThcD
        // Examples:
        //   FTMS + c NSI d sa Full ms2 400.0119@etd30.00@hcd32.00 [150.0000-2000.0000]
        //   FTMS + c NSI d sa Full ms2 376.7569@etd50.00 376.7569@hcd35.00 [120.0000-764.0000]
        nRet = praw->GetFilterForScanNum(scan_num, &bstr);
        if (nRet != 0) {
            e = kRawReadingError; eee;
        }
        QString filterString = QString::fromStdString(toString(bstr));
        QStringList tokens = filterString.split(' ', QString::SkipEmptyParts);
        for (int i = 0; i < tokens.length(); i++) {
            QString token = tokens[i];
            int indexOfAtEtd = token.indexOf("@etd", 0, Qt::CaseInsensitive);
            if (indexOfAtEtd >= 0) {
                // Check for something similar to first example
                int indexOfAtHcd = token.indexOf("@hcd", indexOfAtEtd, Qt::CaseInsensitive);
                if (indexOfAtHcd >= 0) {
                    fragmentTypeStr = "ethcd";
                    break;
                }

                // Check for something similar to second example
                if (i + 1 < tokens.length()) {
                    QString token2 = tokens[i + 1];
                    int indexOfAtHcd2 = token2.indexOf("@hcd", 0, Qt::CaseInsensitive);
                    if (indexOfAtHcd2 >= 0 && token.mid(0, indexOfAtEtd) == token2.mid(0, indexOfAtHcd2)) {
                        fragmentTypeStr = "ethcd";
                        break;
                    }
                }
            }
        }
    }

error:
    safe_SysFreeString(bstr);
    return e;
}

Err GetNumberOfControllersOfType(RawFileHandle rawHandle, long nControllerType, long * nControllers) {
    Err e = kNoErr;
    *nControllers= 0;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    long nRet = praw->GetNumberOfControllersOfType ( nControllerType, nControllers );
    if( nRet != 0 ) {
        *nControllers= 0;
        e = kRawReadingError; eee;
    }
error:
    return e;
}


QString
makeThermoNativeId(long scanNumber)
{
    QString str("controllerType=0 controllerNumber=1 scan=%1");
    if (scanNumber <= 0)
        return QString();

    return str.arg(scanNumber);
}

Err getInstrumentModel(RawFileHandle rawHandle, thermo::InstrumentModelType & modelType, std::string & instModel)
{
    Err e = kNoErr;

    long nRet = 0;
    modelType = thermo::InstrumentModelType_Unknown;
    instModel.clear();
    std::string instName;

    BSTR bstrName = NULL;
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    e = SetRawCurrentController(rawHandle, thermo::Controller_MS, 1); eee;

    nRet = praw->GetInstModel(&bstrName);
    if (nRet != 0) {
        e = kRawReadingError; eee;
    }
    instModel = toString(bstrName);
    safe_SysFreeString(bstrName);

    nRet = praw->GetInstName(&bstrName);
    if (nRet != 0) {
        e = kRawReadingError; eee;
    }
    instName = toString(bstrName);

    if (instModel == "LTQ Velos") // HACK: disambiguate LTQ Velos and Orbitrap Velos
    {
        instModel = instName;
    }
    else if (instModel == "LTQ Orbitrap" &&
             instName.size() <= 0) // HACK: disambiguate LTQ Orbitrap and some broken Exactive files
    {
        std::vector<LabelValueString> values;
        e = GetTuneData(rawHandle, values); eee;
        for (unsigned int i=0; i < values.size(); ++i)
            if (values[i].label == "Model")
            {
                instModel = values[i].value;
                break;
            }
    }

    modelType = thermo::parseInstrumentModelType(instModel);

error:
    safe_SysFreeString(bstrName);
    return e;
}

Err GetTuneData(RawFileHandle rawHandle, std::vector<LabelValueString> & values)
{
    Err e = kNoErr;

    LabelValueString obj;
    values.clear();
    // example for GetTuneData
    long nSegment = 1; // first tune record
    VARIANT varLabels;
    VariantInit(&varLabels);
    VARIANT varValues;
    VariantInit(&varValues);
    long nArraySize = 0;
    long nRet = 0;

    if (rawHandle == NULL) {
        e = kBadParameterError; ree;
    }
    IXRawfile5 * praw= (IXRawfile5 *)rawHandle;
    nRet = praw->GetTuneData(nSegment, &varLabels, &varValues, &nArraySize);
    if( nRet != 0 ) {
        e = kBadParameterError; eee;
    }
    // Get a pointer to the SafeArray
    SAFEARRAY FAR* psaLabels = varLabels.parray;
    varLabels.parray = NULL;
    SAFEARRAY FAR* psaValues = varValues.parray;
    varValues.parray = NULL;
    BSTR* pbstrLabels = NULL;
    BSTR* pbstrValues = NULL;
    if( FAILED(SafeArrayAccessData( psaLabels, (void**)(&pbstrLabels) ) ) )
    {
        SafeArrayUnaccessData( psaLabels );
        SafeArrayDestroy( psaLabels );
        e = kBadParameterError; eee;
    }
    if( FAILED(SafeArrayAccessData( psaValues, (void**)(&pbstrValues) ) ) )
    {
        SafeArrayUnaccessData( psaLabels );
        SafeArrayDestroy( psaLabels );
        SafeArrayUnaccessData( psaValues );
        SafeArrayDestroy( psaValues );
        e = kBadParameterError; eee;
    }

    for( long i=0; i<nArraySize; i++ )
    {
        obj.label = toString(pbstrLabels[i]);
        obj.value = toString(pbstrValues[i]);
    }
    SafeArrayUnaccessData( psaLabels );
    SafeArrayDestroy( psaLabels );
    SafeArrayUnaccessData( psaValues );
    SafeArrayDestroy( psaValues );

error:
    return e;
}

class TestThermoMemory {
public:
    TestThermoMemory() {
        test1();
    }

    Err test1() {
        Err e = kNoErr;
        std::string filename = "E:\\Dropbox_save\\PMI_Share_Data\\Data2015\\MSBioworks\\Remicade_August2015\\MSB21026D.raw";
        RawFileHandle rawHandle = NULL;
        for (int i = 0; i < 1000; i++) {
            e = CreateRawInstanceAndOpenFile(filename, &rawHandle, 0, 1); eee;
            e = CloseAndDestroyRawInstance(rawHandle); eee;
        }
    error:
        return e;
    }
};

//static TestThermoMemory obj;

_MSREADER_END
_PMI_END
