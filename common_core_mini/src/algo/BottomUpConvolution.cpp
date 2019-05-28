/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpConvolution.h"
#include "BottomUpToIntactDataReader.h"

_PMI_BEGIN

BottomUpConvolution::BottomUpConvolution()
{
    int numberOfTheBeast = 666;
    srand(numberOfTheBeast);
}

BottomUpConvolution::~BottomUpConvolution()
{
}

void BottomUpConvolution::clear()
{
    m_distributionInputsByID.clear();
    m_distributionInputIndex.clear();
    m_deltaMassSticks.clear();
}

Err BottomUpConvolution::init(const QList<DistributionInput> &distributionInputs, double pruning)
{
    Err e = kNoErr;
  
    clear();

    if (distributionInputs.empty()) {
        warningCoreMini() << "distributionInputs is empty";
        rrr(kError);
    }

    int indexCounter = 0;
    //// purposely not const.  distributionInput is modified below.
    // distributionInput.degradationID.index = indexCounter;
    for (const DistributionInput &distributionInput : distributionInputs) {

        if (distributionInput.yFraction < pruning) {
            continue;
        }

        DistributionInput distributionInputUpdated = distributionInput;
        distributionInputUpdated.degradationID.index = indexCounter++; 
        m_distributionInputsByID[distributionInputUpdated.degradationID.id].push_back(distributionInputUpdated);
        m_distributionInputIndex.push_back(distributionInputUpdated);

    }

    //// Add WildTypes to m_distributionInputsByID
    for (auto it = m_distributionInputsByID.begin(); it != m_distributionInputsByID.end(); ++it) {

        const QVector<DistributionInput> &distributionInputsInLoop = it.value();

        double locationSum = std::accumulate(
            distributionInputsInLoop.begin(), distributionInputsInLoop.end(), 0.0,
            [](double sum, const DistributionInput &curr) { return sum + curr.yFraction; });

        double wildTypePercentage = 1.0 - locationSum;
        if (wildTypePercentage > 1.0 || wildTypePercentage < 0.0) {
            rrr(kError);
        }

        DistributionInput distributionInputWildType = distributionInputsInLoop[0];
        distributionInputWildType.xValue = 0.0;
        distributionInputWildType.yFraction = wildTypePercentage;
        distributionInputWildType.degradationID.index = indexCounter;
        it->push_back( distributionInputWildType);
        m_distributionInputIndex.push_back(distributionInputWildType);
        ++indexCounter;
    }

    e = buildDistribution(pruning); ree;
    return e;
}

QVector<BottomUpConvolution::DeltaMassStick> BottomUpConvolution::deltaMassSticks() const
{
    if (m_deltaMassSticks.isEmpty()) {
        warningCoreMini() << "Must init class before calling this method";
    }

    return m_deltaMassSticks;
}

BottomUpConvolution::DegradationConstituents
BottomUpConvolution::degradationConstituentSplitter(const DistributionInput &distributionInput)
{
    BottomUpConvolution::DegradationConstituents degradationConstituentsNew;
    degradationConstituentsNew.percentContribution = distributionInput.yFraction;
    const QStringList items = distributionInput.degradationID.id.split("/");
    bool ok = true;
    for (const QString &splitId : items) {
        int distributionInputIndexID = splitId.toInt(&ok);
        Q_ASSERT(ok);
        if (m_distributionInputIndex.at(distributionInputIndexID).xValue == 0) {
            continue;
        }
        degradationConstituentsNew.distributionInputs.push_back(
            m_distributionInputIndex.value(distributionInputIndexID));
    }
    return degradationConstituentsNew;
}


Err BottomUpConvolution::buildDistribution(double prunePercentage)
{
    Err e = kNoErr;

    const QList<QString> &distributionInputsByIDKeys = m_distributionInputsByID.keys();

    if (distributionInputsByIDKeys.size() == 1) {
        const QVector<DistributionInput> &distributionInputs
            = m_distributionInputsByID[distributionInputsByIDKeys[0]];

        for (const DistributionInput &distributionInput : distributionInputs) {
            DeltaMassStick deltaMassstick;
            deltaMassstick.deltaMass = distributionInput.xValue;
            deltaMassstick.intensityFraction = distributionInput.yFraction;
            m_deltaMassSticks.push_back(deltaMassstick);
        }

        return e;
    } else if (distributionInputsByIDKeys.isEmpty()) {
        warningCoreMini() << "distributionInputsByIDKeys is empty";
        rrr(kError);
    }

    QStringList present;
    QVector<DistributionInput> distributionInputsConvolved;

    const QVector<DistributionInput> &distributionInputAs
        = m_distributionInputsByID[distributionInputsByIDKeys[0]];
    const QVector<DistributionInput> &distributionInputBs
        = m_distributionInputsByID[distributionInputsByIDKeys[1]];

    //// Initialize distributionInputsConvolved w/ first two DistributionInputs so that subsequent
    /// ones can be convolved.
    for (const DistributionInput &distributionInputA : distributionInputAs) {
        for (const DistributionInput &distributionInputB : distributionInputBs) {

            double intensityFraction = distributionInputA.yFraction * distributionInputB.yFraction;
            if (intensityFraction < prunePercentage) {
                continue;
            }

            QStringList residueID({ QString::number(distributionInputA.degradationID.index),
                                    QString::number(distributionInputB.degradationID.index) });
            std::sort(residueID.begin(), residueID.end());
            const QString residueIDConcat = residueID.join("/");

            if (present.contains(residueIDConcat)) {
                continue;
            }

            present.push_back(residueIDConcat);

            DegradationID degradationIDNew;
            degradationIDNew.id = residueIDConcat;
            DistributionInput distributionInputNew(
                degradationIDNew, distributionInputA.xValue + distributionInputB.xValue,
                intensityFraction);

            distributionInputsConvolved.push_back(distributionInputNew);
        }
    }

    //// Convolve the rest of the DistributionInputs
    for (int i = 2; i < distributionInputsByIDKeys.size(); ++i) {

        QVector<DistributionInput> distributionInputsConvolvedNew;
        const QVector<DistributionInput> &distributionInputToConvolves
            = m_distributionInputsByID[distributionInputsByIDKeys[i]];

        for (const DistributionInput &distributionInputConvolveOrigin : distributionInputsConvolved) {
            for (const DistributionInput &distributionInputToConvolve :
                 distributionInputToConvolves) {

                const double intensityFraction
                    = distributionInputConvolveOrigin.yFraction * distributionInputToConvolve.yFraction;
                if (intensityFraction < prunePercentage) {
                    continue;
                }

                QStringList residueID(
                    { distributionInputConvolveOrigin.degradationID.id,
                      QString::number(distributionInputToConvolve.degradationID.index) });
                std::sort(residueID.begin(), residueID.end());
                QString residueIDConcat = residueID.join("/");

                if (present.contains(residueIDConcat)) {
                    continue;
                }

                DegradationID degradationIDNew;
                degradationIDNew.id = residueIDConcat;
                DistributionInput distributionInputNew(degradationIDNew,
                    distributionInputConvolveOrigin.xValue
                                                           + distributionInputToConvolve.xValue,
                                                       intensityFraction);

                distributionInputsConvolvedNew.push_back(distributionInputNew);
            }
        }

        distributionInputsConvolved = distributionInputsConvolvedNew;
    }

    //// Collate by massDeltas.
    QHash<int, DeltaMassStick> deltaMassStickCollator;
    for (DistributionInput &distributionInput : distributionInputsConvolved) {

        const int insertIndex = qRound(distributionInput.xValue * m_granularity);

        bool ok = true;
        if (!deltaMassStickCollator.keys().contains(insertIndex)) {
            deltaMassStickCollator[insertIndex].deltaMass = distributionInput.xValue;
            deltaMassStickCollator[insertIndex].intensityFraction = distributionInput.yFraction;
            DegradationConstituents degradationConstituentsNew = degradationConstituentSplitter(distributionInput);
            deltaMassStickCollator[insertIndex].degradationConstituents.push_back(
                degradationConstituentsNew);
        } else {
            deltaMassStickCollator[insertIndex].intensityFraction += distributionInput.yFraction;
            DegradationConstituents degradationConstituentsNew = degradationConstituentSplitter(distributionInput);
            deltaMassStickCollator[insertIndex].degradationConstituents.push_back(
                degradationConstituentsNew);
        }
    }

    //// Fill in DegradationConstituents percentContribution and push to m_deltaMassSticks
    for (auto it = deltaMassStickCollator.begin(); it != deltaMassStickCollator.end(); ++it) {
        QVector<DegradationConstituents> &degradationConstituents
            = it.value().degradationConstituents;
        //// Purposely not const.  degradationConstituent is modified below.
        for (DegradationConstituents &degradationConstituent : degradationConstituents) {
            double intensityFraction
                = it.value().intensityFraction <= 0 ? 1 : it.value().intensityFraction;
            degradationConstituent.percentContribution
                = degradationConstituent.percentContribution / intensityFraction;
        }
        m_deltaMassSticks.push_back(it.value());
    }

    return e;
}

_PMI_END
