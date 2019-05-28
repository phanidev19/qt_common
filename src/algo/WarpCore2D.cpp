/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include "WarpCore2D.h"
#include <QtMath>

_PMI_BEGIN

static const double kLargeNegativeNumber = std::numeric_limits<double>::lowest();
static const double STRETCH = 0.51;
static const double MINIMUM_STRETCH = (1.0 - STRETCH);
static const double MAXIMUM_STRETCH = (1.0 + STRETCH);

static const int defaultGlobalSkew = 100;
static const double defaultStretchPenalty = 0.0;
static const double defaultMzMatchPpm = 100.0;

WarpCore2D::WarpCore2D()
    : m_stretchPenalty(defaultStretchPenalty)
    , m_globalSkew(defaultGlobalSkew)
    , m_mzMatchPpm(defaultMzMatchPpm)
{

}

WarpCore2D::~WarpCore2D()
{

}

double WarpCore2D::stretchPenalty() const
{
    return m_stretchPenalty;
}

void WarpCore2D::setStretchPenalty(double p)
{
    m_stretchPenalty = p;
}

int WarpCore2D::globalSkew() const
{
    return m_globalSkew;
}

Err WarpCore2D::setGlobalSkew(int s)
{
    Err e = kNoErr;

    const int safeMinimum = 10;
    if (m_globalSkew < safeMinimum) {
        rrr(kBadParameterError);
    }
    
    m_globalSkew = s;

    return e;
}

double WarpCore2D::mzMatchPpm() const
{
    return m_mzMatchPpm;
}

void WarpCore2D::setMzMatchPpm(double ppm)
{
    m_mzMatchPpm = ppm;
}


/*! \brief This function does the integral of product of A segment and B segment */
static double segmentMatchScore(const std::vector<WarpElement> &a, int beginA, int endA,
                                const std::vector<WarpElement> &b, int beginB, int endB,
                                double mzMatchPpm)
{
    
    // Zero score on error
    if (endB <= beginB) {
        return 0.0;
    }

    if (endA <= beginA) {
        return 0.0;
    }

    /*  Zero score if segments have very different numbers of points */
    int durationA = endA - beginA;
    int durationB = endB - beginB;
    if (durationB < durationA / 2) {
        return 0.0;
    }

    if (durationA < durationB / 2) {
        return 0.0;
    }

    //  Compute integral of product
    double stepB = static_cast<double>(durationB) / static_cast<double>(durationA);
    double integral = 0.0;
    for (int indexA = beginA; indexA < endA; indexA++) {
        const WarpElement &sampleA = a[indexA];
        double indexBDouble = beginB + (indexA - beginA) * stepB;
        int indexB = qFloor(indexBDouble);
        indexB = std::min(indexB, static_cast<int>(b.size()) - 1);
        double lerpFactor = indexBDouble - indexB;
        WarpElement sampleB = WarpElement::interpolate(b[indexB], b[indexB + 1], mzMatchPpm, lerpFactor);

        // Use quadratic penalty
        integral += WarpElement::scoreFunction(sampleA, sampleB, mzMatchPpm);
    }

    return integral;
}


namespace {
    /*!
        \brief Cell of the dynamic table.

        Definitions:
        A,B :        Reference signal (A) and signal to be warped to the reference (B)
        knotsA :    List of endpoints which define the segments of A that are to be mapped to segments of B
        knotsB :    Empty list, to be populated with indices in B space which match indices in knotsA (A space)

        The dynamic table contains one row for each of the segments in A (knotsA.size() - 1).
        Each row has B.size() cells.
        Indexing of the cells denotes corresponding END positions for each segment.
            So for example: cell[0][100] holds information of what happens if we match the ending of the first 
                segment (index 0) of A to sample 100 in B.
        The information held in each cell is:
            score:            best score achieved by this mapping, and
            segmentStart:    index (in B space) of the starting position of the segment that gave the cell's score

        So for example: cell[1][100] = (score: 1234, segmentStart: 50) means:
            If we map segment 1 of A to the range (50, 100) in B, then we get an accumulated score of 1234.
                (accumulated score includes scores from the previous segment mappings).
    */
    struct DynamicTableEntry
    {
        double score;
        int segmentStart;

        // Set defaults
        DynamicTableEntry()
            : score(kLargeNegativeNumber)
            , segmentStart(0)
        {
        }
    };
}

Err WarpCore2D::constructWarp(std::vector<WarpElement> &a, std::vector<WarpElement> &b,
                            const std::vector<int> &knotsA, std::vector<int> *knotsB) const
{
    Err e = kNoErr;

    if (knotsA.size() < 2) {
        // Consider this a nop
        *knotsB = knotsA;
        return e;
    }

    // Compressed dynamic table.
    // Each row is in the format xxxxCCCCxxxx, where 'x' is unused and 'C' is a cell we use in the
    // table. Each row is represented by its rowStart and rowEnd index and a vector of
    // DynamicTableEntry containing the used ('C') cells only.
    std::vector<std::vector<DynamicTableEntry>> dynamicTable;
    std::vector<int> rowStart;
    std::vector<int> rowEnd;
#define dtCell(i, j) (dynamicTable[i][(j)-rowStart[i]])

    const int segmentCount = static_cast<int>(knotsA.size()) - 1;
    const int nA = static_cast<int>(a.size());
    const int nB = static_cast<int>(b.size());

    //  Compute average A absolute value (used in stiffness penalty)
    double stretchPenaltyFactor = m_stretchPenalty * WarpElement::averageIntensity(a);

    // Calculate rowStart and rowEnd for compressed dynamic table
    rowStart.resize(knotsA.size());
    rowEnd.resize(knotsA.size());
    int firstSegmentDuration = knotsA[1] - knotsA[0];
    rowStart[0] = qFloor(MINIMUM_STRETCH * firstSegmentDuration);
    rowEnd[0] = static_cast<int>(ceil(MAXIMUM_STRETCH * firstSegmentDuration));
    for (int i = 1; i < segmentCount; i++) {
        int startA = knotsA[i];
        int endA = knotsA[i + 1];
        // Note (Michael Geo 2018/12/11): Maybe we should use (endA - startA) here.
        int jOffset = std::min(m_globalSkew, static_cast<int>((STRETCH)*endA));
        rowStart[i] = std::min(nB, endA - jOffset);
        rowEnd[i] = std::min(nB, endA + jOffset);
    }

    // Allocate memory for dynamic table rows
    dynamicTable.clear();
    dynamicTable.resize(knotsA.size());
    for (int i = 0; i < static_cast<int>(knotsA.size()); i++) {
        dynamicTable[i].resize(rowEnd[i] - rowStart[i]);
    }

    // Score for first segment  -- lighter penalty here?
    for (int j = rowStart[0]; j < rowEnd[0]; j++) {
        DynamicTableEntry &currentCell = dtCell(0, j);
        currentCell.score = segmentMatchScore(a, 0, firstSegmentDuration, b, 0, j, m_mzMatchPpm);
        double stretch = static_cast<double>(std::abs(j - firstSegmentDuration));
        currentCell.score -= stretchPenaltyFactor * stretch * stretch;
    }

    // Score for subsequent segments
    for (int i = 1; i < segmentCount; i++) {
        int startA = knotsA[i];
        int endA = knotsA[i + 1];
        double durationA = std::abs(endA - startA);

        // Evaluate different ending positions for the current segment in B's space
        for (int j = rowStart[i]; j < rowEnd[i]; j++) {
            // For each of the ending positions, evaluate a number of starting positions
            int maxk = std::max(0, j - qFloor(MINIMUM_STRETCH * durationA));
            int mink = std::max(0, j - qCeil(MAXIMUM_STRETCH * durationA));
            for (int k = mink; k < maxk; k++) {
                double accumulatedScore = kLargeNegativeNumber;
                if (k >= rowStart[i - 1] && k < rowEnd[i - 1]) {
                    // Cells outside our compressed table are considered to have minus infinity score.
                    accumulatedScore = dtCell(i - 1, k).score;
                }

                // calculate score for this combination of start/end for B
                double score = accumulatedScore + 
                    segmentMatchScore(a, startA, endA, b, k, j, m_mzMatchPpm);

                // calculate stretch penalty
                // (Change the following to Segments-1 to get no penalty for stretching last
                // segment)
                if (i < segmentCount) {
                    double durationB = j - k;
                    double durationDiff = std::abs(durationB - durationA);
                    score -= stretchPenaltyFactor * durationDiff * durationDiff;
                }

                // Write back score and starting position of this segment
                DynamicTableEntry &currentCell = dtCell(i, j);
                if (score > currentCell.score) {
                    currentCell.score = score;
                    currentCell.segmentStart = k;
                }
            } // end for k
        } // end for j
    } // end for i

    // Find winning warp.
    knotsB->resize(segmentCount + 1);
    (*knotsB)[segmentCount] = nB;
    int currentSegmentStart = nB - 1;
    for (int i = segmentCount - 1; i >= 0; i--) {
        currentSegmentStart = dtCell(i, currentSegmentStart).segmentStart;
        (*knotsB)[i] = currentSegmentStart;
    }

    return e;
}

_PMI_END