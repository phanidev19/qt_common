#if !defined (__BRUKER_SAMPLE_H__)
#define __BRUKER_SAMPLE_H__

#include "CommonVendor.h"

// Windows.h define min/max so this macro supresses so that numeric_limits can expose min/max
#define NOMINMAX
#include <limits> // numeric_limits
#include <iostream> // wcout, etc.
#include <string> // string/wstring
#include <vector>
#include <windows.h> // HRESULT, CreateActCtx, CoInitialize, etc.
#include <Shlwapi.h> // Path manipulation functions

// This provides access to CComPtr, CComBSTR and CComVariant wrappers
// that DO NOT throw exception like those generated in a *.tli
#include <atlbase.h>
#include <atlsafe.h> // CComSafeArray

#import <vendor_api\Bruker\CompassXtractMS.dll> no_implementation raw_interfaces_only

// Start of excerpt from https://www.sellsbrothers.com/writing/a_young_person.htm
// This section is basically identical to code JDN wrote in the late 1990's
#pragma once

#define lengthof(rg) (sizeof(rg)/sizeof(*rg))

inline const char* StringFromError(char* szErr, long nSize, long nErr)
{
    _ASSERTE(szErr);
    *szErr = 0;
    DWORD cb = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, 0, nErr, 0, szErr, nSize, 0);
    char szUnk[] = "<unknown>";
    if( !cb && nSize >= lengthof(szUnk) ) lstrcpyA(szErr, szUnk);
    return szErr;
}

inline HRESULT TraceHR(const char* pszFile, long nLine, HRESULT hr)
{
    if( FAILED(hr) )
    {
        char szErr[128];
        char sz[_MAX_PATH + lengthof(szErr) + 2048];
        wsprintfA(sz, "%s(%d) : error 0x%x: %s\n", pszFile, nLine, hr,
            StringFromError(szErr, lengthof(szErr), hr));
        //OutputDebugStringA(sz);
        std::cout << "TraceHR Error: " << szErr << std::endl;
        std::cout << "TraceHR File: " << pszFile << ", Line: " << nLine << std::endl;

    }
    return hr;
}

#ifdef _DEBUG
#define TRACEHR(_hr) TraceHR(__FILE__, __LINE__, _hr)
#else
#define TRACEHR(_hr) _hr
#endif

#define HR(ex) { HRESULT _hr = ex; if(FAILED(_hr)) return TRACEHR(_hr); }

// End of excerpt from https://www.sellsbrothers.com/writing/a_young_person.htm


HRESULT PreparePathForLoadingDLL(bool bLoadCOMServerFromManfest, const std::wstring& wstrFullManifestFile);

HRESULT Cleanup(bool bLoadCOMServerFromManfest);

HRESULT variantArrayToDoubleArray(VARIANT & vData, std::vector<double> & values);

HRESULT GetSpectrumPoints(
    const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum,
    EDAL::SpectrumTypes spectrumType,
    std::vector<double> &xlist, std::vector<double> & ylist);


HRESULT ProcesSpectrumParameters(const CComPtr<EDAL::IMSSpectrum>& spIMSSpectrum);

HRESULT RowDataToDouble(const CComVariant& spvData, ULONG ulRowCount, double * outval);

#endif
