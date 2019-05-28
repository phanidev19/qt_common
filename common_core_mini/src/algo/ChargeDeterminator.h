/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef CHARGE_DETERMINATOR_H
#define CHARGE_DETERMINATOR_H

#include "pmi_common_core_mini_export.h"

#include "AbstractChargeDeterminator.h"
#include "MzInterval.h"

#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <QVector>
#include <QCache>

_PMI_BEGIN

/*!
* \brief Determines the charge for given part of the MS1 scan
*
* The algorithm is based on hints from Jira ticket LT-2599;
* This algo counts the number of matched isotopes and counts sum of their intensities
* for each charge to be determined. Takes highest score as charge result.
* It does not normalize the scan, nor it does the uniform distribution of the scan
*/
class PMI_COMMON_CORE_MINI_EXPORT ChargeDeterminator : public AbstractChargeDeterminator
{

public:
    ChargeDeterminator(double isotopeSpacing = 1.0);
    ~ChargeDeterminator() {}

    int determineCharge(const point2dList &scanPart, double mz) override;

    double evaluateCharge(int charge, double mz, const point2dList &scanPart);

    QVector<double> generateMzForCharge(double mz, int charge, const Interval<int> &searchInterval);

    void setIsotopeSpacing(double spacing) { m_isotopeSpacing = spacing; }
    double isotopeSpacing() const { return m_isotopeSpacing; }

    /*!
     * \brief Determines how many charge states are evaluated in \see
     * ChargeDeterminator::determineCharge. By default it is 10.
     */
    void setMaxChargeState(int maxChargeState) { m_maxChargeState = maxChargeState; }
    int maxChargeState() { return m_maxChargeState; }

    static double searchRadius();

    /*!
    * \brief Calculates next higher/lower charge state expressed in m/z.
    *
    * Computed against \a charge parameter, \a nextCharge is the desired charge value
    * Valid \a charge is in range [1..N), so is \a nextCharge
    *
    * Note: Defined in Feature finder algorithm as Calculation 1 and Calculation 2.
    */
    static double nextChargeState(int charge, int nextCharge, double mostAbundantIonMz);

private:
    // Build comb filter w/ 7 teeth for charge state being evaluated
    QVector<double> buildCombFilter(int charge);
    QVector<double> getCombFilter(int charge);

private:
    int m_maxChargeState;
    double m_isotopeSpacing;
    QCache<int, QVector<double>> m_chargeTeeths;
};

_PMI_END

#endif // CHARGE_DETERMINATOR_H