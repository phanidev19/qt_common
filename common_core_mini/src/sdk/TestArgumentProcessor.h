/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Jaros³aw Staniek jstaniek@milosolutions.com
*/

#ifndef TEST_ARGUMENT_PROCESSOR_H
#define TEST_ARGUMENT_PROCESSOR_H

#include "pmi_core_defs.h"
#include "pmi_common_core_mini_export.h"

class QStringList;

_PMI_BEGIN

namespace TestArgumentProcessor {
    //! Helper to process arguments and Qt Test options.
    //! @return true on success
    PMI_COMMON_CORE_MINI_EXPORT bool processArguments(int argc, char *argv[], const QStringList &privateArgNames,
        int *realArgc, QStringList *privateArgs);

};

_PMI_END

#endif // TEST_ARGUMENT_PROCESSOR_H