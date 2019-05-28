/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef MONOISOTOPE_DETERMINATOR_H
#define MONOISOTOPE_DETERMINATOR_H

#include "AbstractMonoisotopeDeterminator.h"
#include "pmi_common_core_mini_export.h"

#include <common_types.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

//! @brief Determines the monoisotope in charge cluster
class PMI_COMMON_CORE_MINI_EXPORT MonoisotopeDeterminator : public AbstractMonoisotopeDeterminator
{

public:
    MonoisotopeDeterminator();
    ~MonoisotopeDeterminator();

    int determineMonoisotopeOffset(const point2dList &scanPart, double mz, int charge,
        double *score) override;

    //! @brief defines the intersection size used by \a determineMonoisotopeOffset, expressed in mz
    static double searchRadius();

    //! @brief Converts monoisotope offset computed from \a determineMonoisotopeOffset to mz
    //
    // \a charge has to be positive number, typically in range 1 til 10
    static double fromOffsetToMz(int offset, double mz, int charge);

private:
    static double sumIntensitiesAt(const point2dList &scanPart, double mz, double mzTolerance);

    static double pearsonCorrelation(const QVector<double> &sampleX, const QVector<double> &sampleY);

    static constexpr int positiveTeethCount(int charge);
    static constexpr int totalTeethCount(int charge);

    static QVector<QVector<double>> loadAveragineRatio(const QString &filePath);

private:
    static QVector<double> buildBentCombFilter(int charge);
    QVector<double> getAveragineRatio(int mass, int size) const;

    friend class MonoisotopeDeterminatorTest;

    QVector<QVector<double>> m_averagineRatios;
};

_PMI_END

#endif // MONOISOTOPE_DETERMINATOR_H