/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "PmiMemoryInfo.h"

#include <QLocale>
#include <QtGlobal>

#ifdef Q_OS_WIN
#include <Windows.h>
#include <Psapi.h>

_PMI_BEGIN

const int PMI_PROCESS_MEMORY_UNIT = 1024; // kibibyte
const char *PMI_PROCESS_MEMORY_UNIT_SYMBOL = "K";

static inline PROCESS_MEMORY_COUNTERS memoryCounters()
{
    static PROCESS_MEMORY_COUNTERS pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc));
    return pmc;
}

size_t MemoryInfo::processMemory()
{
    PROCESS_MEMORY_COUNTERS pmc = memoryCounters();
    return pmc.WorkingSetSize;
}

QString MemoryInfo::getProcessMemoryString()
{
    return QLocale::c().toString(MemoryInfo::processMemory() / PMI_PROCESS_MEMORY_UNIT)
        + QLatin1String(PMI_PROCESS_MEMORY_UNIT_SYMBOL);
}

size_t MemoryInfo::peakProcessMemory()
{
    PROCESS_MEMORY_COUNTERS pmc = memoryCounters();
    return pmc.PeakWorkingSetSize;
}

_PMI_END

#else
#error Missing implementation of process memory functions
#endif
