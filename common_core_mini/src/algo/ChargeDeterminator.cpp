/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "ChargeDeterminator.h"
#include "MzInterval.h"
#include "Point2dListUtils.h"
#include "pmi_common_core_mini_debug.h"

// common_stable
#include <common_constants.h>

#include "pmi_common_core_mini_debug.h"

#define SHOW_DEBUG_LOG false
#define ChargeDeterminatorDebug if (!SHOW_DEBUG_LOG) {} else debugCoreMini

_PMI_BEGIN

const double APEX_CHARGE_CLUSTERING = 3.0; // in mz(Dalton)
const double ERROR_TOLERANCE_DALTONS = 0.02; // in mz(Dalton)
const int PRECACHED_CHARGE_TEETHS_COUNT = 10;

const int DEFAULT_TEETH_COUNT_LEFT = -3;
const int DEFAULT_TEETH_COUNT_RIGHT = 3;

ChargeDeterminator::ChargeDeterminator(double isotopeSpacing /*= 1.0*/)
    : m_maxChargeState(10),
    m_isotopeSpacing(isotopeSpacing),
    m_chargeTeeths(PRECACHED_CHARGE_TEETHS_COUNT)
{
}

int ChargeDeterminator::determineCharge(const point2dList &scanPart, double mz)
{
    // for each charge
    double maxScore = 0.0;
    int bestCharge = 0;
    for (int charge = 1; charge <= m_maxChargeState; ++charge) {
        double score = evaluateCharge(charge, mz, scanPart);
        if (score > maxScore) {
            maxScore = score;
            bestCharge = charge;
        }
    }

    return bestCharge;
}

double ChargeDeterminator::evaluateCharge(int charge, double mz, const point2dList &scanPart)
{
    QVector<double> chargeDistances = getCombFilter(charge);

    double score = 0.0;
    int matchedIons = 0;
    for (int i = 0; i < chargeDistances.size(); ++i) {

        double mzCentered = mz + chargeDistances.at(i);
        double mzStart = mzCentered - ERROR_TOLERANCE_DALTONS;
        double mzEnd = mzCentered + ERROR_TOLERANCE_DALTONS;

        // count matched ions from scan part
        point2dList subPart = Point2dListUtils::extractPointsIncluding(scanPart, mzStart, mzEnd);

        double weight = 1.0; // every teeth has the same weight

        double ionScore = 0.0;
        for (const point2d &pt : subPart) {
            ionScore += weight * pt.y();
        }
        score += ionScore;

        if (ionScore > 0) {
            matchedIons++;
        }
    }
    double finalScore = score * matchedIons;
    ChargeDeterminatorDebug() << "charge" << charge << "score" << score << "matched ion" << matchedIons;
    return finalScore;
}

QVector<double> ChargeDeterminator::generateMzForCharge(double mz, int charge, const Interval<int> &searchInterval)
{
    QVector<double> result;
    Q_ASSERT(charge > 0);

    double chargeDistance = m_isotopeSpacing / charge;
    for (int i = searchInterval.start(); i <= searchInterval.end(); i++) {
        if (i != 0) {
            result.push_back(mz + i * chargeDistance);
        }
    }

    return result;
}

double ChargeDeterminator::searchRadius()
{
    const double radius = APEX_CHARGE_CLUSTERING + ERROR_TOLERANCE_DALTONS;
    return radius;
}

double ChargeDeterminator::nextChargeState(int charge, int nextCharge, double mostAbundantIonMz)
{
    if (charge <= 0) {
        warningCoreMini() << "Invalid charge provided:" << charge;
        return 0.0;
    }

    if (nextCharge <= 0) {
        warningCoreMini() << "Invalid next charge provided:" << nextCharge;
        return 0.0;
    }

    int increment = nextCharge - charge;
    double mass = (mostAbundantIonMz * charge) + (increment * HYDROGEN);
    double newMz = mass / nextCharge;
    return newMz;
}

QVector<double> ChargeDeterminator::buildCombFilter(int charge)
{
    QVector<double> result;

    double chargeDistance = m_isotopeSpacing / charge;
    // Build comb filter w/ 7 teeth for charge state being evaluated
    int first = DEFAULT_TEETH_COUNT_LEFT;
    int last = DEFAULT_TEETH_COUNT_RIGHT;
    int size = last - first + 1;
    result.reserve(size);
    for (int i = first; i <= last; ++i) {
        // we push here mz distances
        result.push_back(i * chargeDistance);
    }

    return result;
}


QVector<double> ChargeDeterminator::getCombFilter(int charge)
{
    QVector<double> result;
    if (!m_chargeTeeths.contains(charge)) {
        QVector<double> *cachedTeeth = new QVector<double>();
        *cachedTeeth = buildCombFilter(charge);
        m_chargeTeeths.insert(charge, cachedTeeth);
    }

    result = *m_chargeTeeths.object(charge);
    return result;
}


_PMI_END
