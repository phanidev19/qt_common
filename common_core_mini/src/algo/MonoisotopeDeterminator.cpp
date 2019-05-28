/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "MonoisotopeDeterminator.h"

#include "CsvReader.h"
#include "pmi_common_core_mini_debug.h"

#include <QFile>

#include <functional>

#define SHOW_DEBUG_LOG false
#define MonoisotopeDeterminatorDebug if (!SHOW_DEBUG_LOG) {} else debugCoreMini

inline void initMyResource()
{
    Q_INIT_RESOURCE(common_core_mini);
}

_PMI_BEGIN

// same constants as for class ChargeDeterminator but they should not be shared,
// as they need to be de-coupled and tweaked eventually

const double APEX_CHARGE_CLUSTERING = 4.0; // in mz(Dalton)
const double ERROR_TOLERANCE_DALTONS = 0.02; // in mz(Dalton)
const int MAX_CHARGE_STATE = 10;
const int BENT_TEETH_COUNT = 1;

MonoisotopeDeterminator::MonoisotopeDeterminator()
{
    initMyResource();

    // load it as resource
    static const QVector<QVector<double>> averagineRatios
        = loadAveragineRatio(QString(":/averagine_normalized.csv"));
    m_averagineRatios = averagineRatios;

    if (m_averagineRatios.isEmpty()) {
        warningCoreMini() << "Failed to initialize averagine ratios!";
    }
}

MonoisotopeDeterminator::~MonoisotopeDeterminator()
{
}

int MonoisotopeDeterminator::determineMonoisotopeOffset(const point2dList &scanPart, double mz,
                                                        int charge, double *score)
{
    Q_ASSERT(charge > 0);

    double nominalMassWeight = mz * charge;

    QVector<double> combFilter = buildBentCombFilter(charge);

    int mass = qRound(nominalMassWeight);
    QVector<double> averagineRatio = getAveragineRatio(mass, combFilter.size());

    MonoisotopeDeterminatorDebug() << "averagine ratios" << averagineRatio;

    int firstShift = -(combFilter.size() - 1); // -9 for charge 4
    int lastShift = (combFilter.size() - 1) - 1; // +8 for charge 4
    int shiftRange = lastShift - firstShift + 1;

    QVector<double> intensitiesOfInterest;
    intensitiesOfInterest.reserve(shiftRange);

    double chargeDistance = 1.0 / charge;

    const double mzTolerance = 0.02;

    MonoisotopeDeterminatorDebug() << "mz"
                                   << ","
                                   << "intensity table:";
    // first build the vector of intensities we are interested in
    for (int shift = firstShift; shift <= lastShift; shift++) {
        double currentMz = mz + (shift * chargeDistance);
        double sum = sumIntensitiesAt(scanPart, currentMz, mzTolerance);
        MonoisotopeDeterminatorDebug() << "mz" << currentMz << "," << sum;
        intensitiesOfInterest.push_back(sum);
    }

    // normalize
    auto maxElem = std::max_element(intensitiesOfInterest.begin(), intensitiesOfInterest.end());
    double maxElementValue = *maxElem;
    if (!qFuzzyIsNull(maxElementValue)) {
        for (int i = 0; i < intensitiesOfInterest.size(); ++i) {
            intensitiesOfInterest[i] /= (maxElementValue);
        }
    }


    MonoisotopeDeterminatorDebug() << "normalized scan" << intensitiesOfInterest;

    // multiply the teeths with intensities
    QVector<double> convolutionResult;
    convolutionResult.resize(combFilter.size());

    bool scoreInitialized = false;
    double bestScore = 0.0;
    int bestIndex = 0;
    MonoisotopeDeterminatorDebug()
        << "Score table (isotope  offset, intensity, score, teeth applied)";
    // roll
    for (int i = lastShift; i >= 0; i--) {

        std::transform(combFilter.begin(), combFilter.end(),
                       std::next(intensitiesOfInterest.begin(), i), convolutionResult.begin(),
                       std::multiplies<double>());

        // pearson correlation with averagine ratios
        double pearsonScore = pearsonCorrelation(convolutionResult, averagineRatio);

        double centeredMz = *std::next(intensitiesOfInterest.begin(), (i + 1));

        if (!scoreInitialized) {
            bestScore = pearsonScore;
            bestIndex = i;
            scoreInitialized = true;
        } else {
            if (bestScore < pearsonScore) {
                bestScore = pearsonScore;
                bestIndex = i;
            }
        }

        MonoisotopeDeterminatorDebug() << lastShift - i << "," << centeredMz * maxElementValue
                                       << "|" << pearsonScore << "|" << convolutionResult;
    }

    if (score) {
        *score = bestScore;
    }

    int shift = lastShift - bestIndex;
    return shift;
}

double MonoisotopeDeterminator::searchRadius()
{
    return APEX_CHARGE_CLUSTERING + ERROR_TOLERANCE_DALTONS;
}

QVector<QVector<double>> MonoisotopeDeterminator::loadAveragineRatio(const QString &filePath)
{
    QVector<QVector<double>> averagineRatios;

    QFile file(filePath);
    CsvReader reader(&file);
    if (!reader.open()) {
        return averagineRatios;
    }

    bool ok = true;

    const int maxAveragineRatioValues = totalTeethCount(MAX_CHARGE_STATE);

    // skip header
    if (reader.hasMoreRows()) {
        reader.readRow();
    }

    while (reader.hasMoreRows()) {

        QStringList row = reader.readRow();
        if (row.isEmpty()) {
            continue;
        }

        int maxItems = std::min(row.size(), maxAveragineRatioValues);

        QVector<double> values;
        values.reserve(maxItems);

        // skip first index
        for (int i = 1; i < maxItems; ++i) {
            double result = row.at(i).toDouble(&ok);
            if (ok) {
                values.push_back(result);
            }
        }
        if (values.isEmpty()) {
            continue;
        }

        averagineRatios.push_back(values);
    }

    return averagineRatios;
}

double MonoisotopeDeterminator::fromOffsetToMz(int offset, double mz, int charge)
{
    Q_ASSERT(charge > 0);
    double chargeDistance = 1.0 / charge;
    // to the left of the inspected mz
    return mz - offset * chargeDistance;
}

QVector<double> MonoisotopeDeterminator::getAveragineRatio(int mass, int size) const
{
    // TODO: use AveragineDistribution from QAL
    QVector<double> result;

    int index = qRound(mass / 100.0);
    if (index > m_averagineRatios.size()) {
        return result;
    }

    result.reserve(size);
    // prepand zero
    result.push_back(0.0);

    // append ratios
    QVector<double> ratios = m_averagineRatios.at(index);
    // final size includes the prefix zero, thus size - 1
    ratios.resize(size - 1);
    result.append(ratios);

    return result;
}

double MonoisotopeDeterminator::sumIntensitiesAt(const point2dList &scanPart, double mz,
                                                 double mzTolerance)
{
    double result = 0.0;

    point2d startPoint(mz - mzTolerance, 0);
    point2d endPoint(mz + mzTolerance, 0);

    auto lower = std::lower_bound(scanPart.cbegin(), scanPart.cend(), startPoint, point2d_less_x);
    if (lower == scanPart.cend()) {
        return result;
    }

    auto upper = std::upper_bound(scanPart.cbegin(), scanPart.cend(), endPoint, point2d_less_x);

    // sum intensities
    result = std::accumulate(lower, upper, double(0.0), [](double first, const point2d &second) {
        return first + second.y();
    });

    return result;
}

double MonoisotopeDeterminator::pearsonCorrelation(const QVector<double> &x,
                                                   const QVector<double> &y)
{
    Q_ASSERT(x.size() == y.size());
    // https://en.wikipedia.org/wiki/Pearson_correlation_coefficient

    double n = x.size();
    // sum xi * yi
    double productXY = std::inner_product(x.begin(), x.end(), y.begin(), 0.0);

    // sum xi
    double sumX = std::accumulate(x.begin(), x.end(), 0.0);

    // sum yi
    double sumY = std::accumulate(y.begin(), y.end(), 0.0);

    double numerator = n * productXY - sumX * sumY;

    // denominator calculations
    // sum (xi * xi)
    double sumSquaredXes = std::inner_product(x.begin(), x.end(), x.begin(), 0.0);

    // (sum xi)^2
    double sumXSquared = sumX * sumX;

    // sum (yi * yi)
    double sumSquaredYs = std::inner_product(y.begin(), y.end(), y.begin(), 0.0);

    // (sum yi)^2
    double sumYSquared = sumY * sumY;

    double denominator
        = std::sqrt(n * sumSquaredXes - sumXSquared) * std::sqrt(n * sumSquaredYs - sumYSquared);

    double r = 0.0;
    if (denominator != 0.0) {
        r = numerator / denominator;
    }

    return r;
}

constexpr int MonoisotopeDeterminator::positiveTeethCount(int charge)
{
    return charge * 2 + 1;
}

constexpr int MonoisotopeDeterminator::totalTeethCount(int charge)
{
    return positiveTeethCount(charge) + BENT_TEETH_COUNT;
}

QVector<double> MonoisotopeDeterminator::buildBentCombFilter(int charge)
{
    const double positiveTeethValue = 1.0;
    const double bentTeethValue = -4.0;

    QVector<double> result(totalTeethCount(charge), positiveTeethValue);

    // first teeth is bent
    result[0] = bentTeethValue;

    return result;
}

_PMI_END
