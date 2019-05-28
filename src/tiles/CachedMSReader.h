/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef CACHED_MSREADER_H
#define CACHED_MSREADER_H

#include "pmi_core_defs.h"
#include <QCache>

#include "common_math_types.h"
#include "common_errors.h"

_PMI_BEGIN

class MSReader;

class CachedMSReader {

public:    
    CachedMSReader(MSReader * reader, int cacheSize, bool doCentroiding);

    Err getScanData(long scanNumber, point2dList ** points);

private:
    MSReader * m_reader;
    QCache<long, point2dList> m_cache;
    bool m_doCentroiding;
};

_PMI_END

#endif // CACHED_MSREADER_H