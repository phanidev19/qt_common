/*
* Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef PMI_MEMORY_INFO_H
#define PMI_MEMORY_INFO_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

class QString;

_PMI_BEGIN

//! Unit used to measure process memory 1024 bytes or 1KiB
extern const int PMI_PROCESS_MEMORY_UNIT;

//! Symbol used to display process memory: "K" for KiB
extern const char *PMI_PROCESS_MEMORY_UNIT_SYMBOL;

//! A set of functions providing memory information
class PMI_COMMON_CORE_MINI_EXPORT MemoryInfo
{
public:
    //!\brief @return the process current working memory set size, in bytes.
    static size_t processMemory();

    //!\brief @return the process current peak working memory set size, in bytes.
    static size_t peakProcessMemory();

    //!\brief @return current working memory set with PMI_PROCESS_MEMORY_UNIT_SYMBOL, unit is KiB
    //! Example: "546548K"
    static QString getProcessMemoryString();
};

_PMI_END

#endif // PMI_MEMORY_INFO_H
