/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#ifndef BOTTOM_UP_CONVOLUTION_H
#define BOTTOM_UP_CONVOLUTION_H

// common_stable
#include "pmi_common_core_mini_debug.h"
#include "pmi_common_core_mini_export.h"
#include <common_errors.h>
#include <common_math_types.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

/*!
 * @brief Builds polynomial expansions and their origins using a stochastic simulation.
 *
 */
class PMI_COMMON_CORE_MINI_EXPORT BottomUpConvolution
{
public:

    static const int NOT_SET = -1;
    struct DegradationID {
        QString id;
        QString position;
        int proteinID = NOT_SET;
        int chainCount = NOT_SET;
        int index = NOT_SET;

        DegradationID() {}
        DegradationID(QString position, int proteinID, int chainCount)
            : position(position)
            , proteinID(proteinID)
            , chainCount(chainCount)
        {
            id = QString::number(proteinID) + ":" + QString::number(chainCount) + ":" + position;
        }
    };

    struct DistributionInput {
        DegradationID degradationID;
        double xValue = 0;
        double yFraction = 0;

        DistributionInput() {}
        DistributionInput(const DegradationID &degradationID, double xValue, double yFraction)
            : degradationID(degradationID)
            , xValue(xValue)
            , yFraction(yFraction)
        {
        }
    };

    struct DegradationConstituents {
        double percentContribution = 0;
        QVector<DistributionInput> distributionInputs;
    };

    struct DeltaMassStick {
        double deltaMass = 0;
        double intensityFraction = 0;
        QVector<DegradationConstituents> degradationConstituents;

        DeltaMassStick() {}
    };

    BottomUpConvolution();
    ~BottomUpConvolution();

    /*!
     * @brief Initializes the input and computes bottom up reconstruction
     *
     *  Default experiment count is set to 500K.  This a bit overkill to acheive stochastically
     * accurate results
     */
    Err init(const QList<DistributionInput> &distributionInputs, double pruning);

    /*!
     * @brief Here is the business.
     *
     *  TODO: make a better comment.
     */
    QVector<DeltaMassStick> deltaMassSticks() const;

private:
    DegradationConstituents degradationConstituentSplitter(const DistributionInput &distributionInput);
    Err buildDistribution(double prunePercentage);
    void clear();

private:
    QHash<QString, QVector<DistributionInput>> m_distributionInputsByID;
    QVector<DistributionInput> m_distributionInputIndex;
    QVector<DeltaMassStick> m_deltaMassSticks;
    double m_granularity = 1000.0;
};

_PMI_END

#endif // BOTTOM_UP_CONVOLUTION_H
