/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/


#include "MSDataIterator.h"
#include "common_math_types.h"

#include "MSReader.h"
#include "PlotBase.h"
#include "Point2dListUtils.h"

_PMI_BEGIN

const int SCAN_NUMBERS_CACHE_SIZE = 128;

MSDataIterator::MSDataIterator(MSReader * reader, bool doCentroiding, const ScanIndexNumberConverter &converter)
    : m_reader(reader, SCAN_NUMBERS_CACHE_SIZE, doCentroiding),
      m_converter(converter)
{
}

void MSDataIterator::fetchTileData(double mzStart, double mzEnd, int scanIndexStart, int scanIndexEnd, QVector<point2dList> * data)
{
    if (!data) {
        return;
    }

    data->clear();

    int scanIndexEndBounded = scanIndexEnd;
    int scanIndexCount = m_converter.size();

    int yPadding = 0;
    // compute padding
    if (scanIndexEnd > scanIndexCount) {
        yPadding = scanIndexEnd - scanIndexCount;
        scanIndexEndBounded = scanIndexCount;
    }

    point2dList * scanDataRow = nullptr;
    for (int y = scanIndexStart; y < scanIndexEndBounded; ++y) {
        int scanIndex = y;
        int scanNumber = m_converter.toScanNumber(scanIndex);
        Q_ASSERT(scanNumber != -1);

        m_reader.getScanData(scanNumber, &scanDataRow);

        //fetch data between mzStart, mzEnd
        point2dList filteredMz = Point2dListUtils::extractPoints(*scanDataRow, mzStart, mzEnd);
        data->push_back(std::move(filteredMz));
    }

    // padding of the tile: we can have less scan indexes compared to dimension of the tiles
    // e.g. tile system has regular grid of scans with size of 16. If we have just 9 scan numbers, we 
    // will pad the tile with 7 empty vectors
    int paddingStart = scanIndexEndBounded;
    int paddingEnd = scanIndexEndBounded + yPadding;
    point2dList empty;
    for (int y = paddingStart; y < paddingEnd; ++y) {
        data->push_back(empty);
    }
}



_PMI_END


