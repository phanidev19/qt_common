/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MS_DATA_ITERATOR_H
#define MS_DATA_ITERATOR_H

#include "pmi_core_defs.h"
#include <QtGlobal>
#include "common_math_types.h"
#include "ScanIndexNumberConverter.h"
#include "CachedMSReader.h"

_PMI_BEGIN
class ScanIndexNumberConverter;
class MSReader;

class PMI_COMMON_MS_EXPORT MSDataIterator {
    Q_DISABLE_COPY(MSDataIterator);
public:
    MSDataIterator(MSReader * reader, bool doCentroiding, const ScanIndexNumberConverter &converter);
    void fetchTileData(double mzStart, double mzEnd, int scanIndexStart, int scanIndexEnd, QVector<point2dList> * data);

private:
    CachedMSReader m_reader;
    ScanIndexNumberConverter m_converter;
};

_PMI_END

#endif // MS_DATA_ITERATOR_H