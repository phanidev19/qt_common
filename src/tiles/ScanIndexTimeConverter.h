/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef SCAN_INDEX_TIME_CONVERTER_H
#define SCAN_INDEX_TIME_CONVERTER_H

#include "pmi_core_defs.h"
#include "pmi_common_ms_export.h"

#include <vector>

// Fix C4251 warnings
// from https://support.microsoft.com/en-us/kb/168958

// Provide the storage class specifier (extern for an .exe file, null
// for DLL) and the __declspec specifier (dllimport for .an .exe file,
// dllexport for DLL).
// dllexport for dll, dllimport for exe

#define FORCE_EXPORT_ONLY __declspec(dllexport)

#ifdef pmi_common_ms_EXPORTS
#  define PMI_COMMON_MS_EXPORT_TEMPLATE
#else
#  define PMI_COMMON_MS_EXPORT_TEMPLATE extern
#endif

// fix warning C4910: 'std::vector<double,std::allocator<_Ty>>' : '__declspec(dllexport)' and 'extern' are incompatible on an explicit instantiation
#ifdef FORCE_EXPORT_ONLY 
#  undef PMI_COMMON_MS_EXPORT_TEMPLATE
#  define PMI_COMMON_MS_EXPORT_TEMPLATE
#endif

// we have to export the allocator too
// http://www.unknownroad.com/rtfm/VisualStudio/warningC4251.html but it does not help :(
// PMI_COMMON_MS_EXPORT_TEMPLATE template class FORCE_EXPORT_ONLY std::allocator< double >;
// PMI_COMMON_MS_EXPORT_TEMPLATE template class FORCE_EXPORT_ONLY std::vector< double, std::allocator< double > >;

_PMI_BEGIN

class MSReader;
#if defined(_MSC_VER)
#pragma warning ( push )
#pragma warning (disable : 4251)
#endif

class PMI_COMMON_MS_EXPORT ScanIndexTimeConverter
{
public:
    //TODO: Check more of TimeWarp
    ScanIndexTimeConverter();
    ScanIndexTimeConverter(const std::vector<double> &time, const std::vector<double> &scanIndex);
    double timeToScanIndex(double time) const;
    double scanIndexToTime(double scanIndex) const;

    std::vector<double> times() const;

    static ScanIndexTimeConverter fromMSReader(MSReader * reader);

private:
    std::vector<double> m_time;
    std::vector<double> m_scanIndex;
};

#if defined(_MSC_VER)
#pragma warning ( pop )
#endif

_PMI_END

#endif // SCAN_INDEX_TIME_CONVERTER_H
