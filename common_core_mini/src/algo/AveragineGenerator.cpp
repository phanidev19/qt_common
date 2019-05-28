/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 * Author: Lukas Tvrdy ltvrdy@milosolutions.com
 */

#include "AveragineGenerator.h"
#include "AveragineIsotopeTable.h"

_PMI_BEGIN

class Q_DECL_HIDDEN AveragineGenerator::Private
{
public:
    Private() {}

    double mass = 0.0;
    coreMini::AveragineIsotopeTable averagineGenerator;
};

AveragineGenerator::AveragineGenerator(double mass)
    : d(new Private)
{
    setMass(mass);
}

AveragineGenerator::~AveragineGenerator()
{
}

void AveragineGenerator::setMass(double mass)
{
    d->mass = mass;
}

double AveragineGenerator::mass() const
{
    return d->mass;
}

point2dList AveragineGenerator::generateSignal(int charge) const
{
    point2dList result;
    if (charge == 0) {
        return result;
    }
    
    double monoIsotopeMz = computeMonoisotopeMz(d->mass, charge);
    double chargeDistance = 1.0 / charge;

    const std::vector<double> &averagineRatio
        = d->averagineGenerator.isotopeIntensityFractions(d->mass);

    
    result.reserve(averagineRatio.size());
    for (size_t i = 0; i < averagineRatio.size(); ++i) {
        result.push_back(point2d(monoIsotopeMz + (chargeDistance * i), averagineRatio.at(i)));
    }

    return result;
}

void AveragineGenerator::getMaxMzAndMonoisotopeOffset(int charge, double *maxMz,
                                                      int *monoisotopeOffset)
{
    if (charge == 0) {
        return;
    }

    const std::vector<double> &averagineRatio
        = d->averagineGenerator.isotopeIntensityFractions(d->mass);

    const double monoIsotopeMz = computeMonoisotopeMz(d->mass, charge);
    const double chargeDistance = 1.0 / charge;

    double maxIndex = -1;
    *monoisotopeOffset = -1;

    for (size_t i = 0; i < averagineRatio.size(); ++i) {
        if (maxIndex < averagineRatio.at(i)) {
            maxIndex = averagineRatio.at(i);
            *monoisotopeOffset = static_cast<int>(i);
            *maxMz = monoIsotopeMz + (chargeDistance * i);
        }
    }
}

double AveragineGenerator::computeMonoisotopeMz(double mass, int charge)
{
    if (charge == 0) {
        return 0.0;
    }

    return (mass + charge) / charge;
}

_PMI_END
