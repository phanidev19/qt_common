/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#ifndef BOTTOM_UP_QUERY_BUILDER_H
#define BOTTOM_UP_QUERY_BUILDER_H

#include "BottomUpConvolution.h"
#include "BottomUpToIntactDataReader.h"
#include "pmi_common_core_mini_export.h"
#include <common_errors.h>
#include <common_math_types.h>
#include <pmi_common_core_mini_debug.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

/*!
 * @brief Brings all elements together for BottomUp Reconstruction of Peptide Map data to Intact.
 *
 * Given a csv file that has been currated by user, class uses a reader to read it and then
 * reconstruct Intact mod deltas and their relative percentages using BottomUpConvolution class
 * This class has the option of outputing the calculation just for reduced/alkylated or Intact mAb.
 */
class PMI_COMMON_CORE_MINI_EXPORT BottomUpQueryBuilder
{
public:
    struct ProteinQuery {
        QHash<BottomUpToIntactDataReader::ProteinId, int> proteinIDCount;

        ProteinQuery() {}

        ProteinQuery(QHash<BottomUpToIntactDataReader::ProteinId, int> proteinIDCount)
            : proteinIDCount(proteinIDCount)
        {
        }
    };

    BottomUpQueryBuilder();

    ~BottomUpQueryBuilder();

    /*!
    * @brief Iterates constituents of a given DegradationConstituents and presents them in readable
    * format.
    */
    void degradationConstituentsReader(
        const BottomUpConvolution::DegradationConstituents &degradationConstituent);

    /*!
     * @brief initializes m_bottomUpToIntactDataReader
     */
    Err init(const BottomUpToIntactDataReader &bottomUpToIntactDataReader);

    Err runConvolutionQuery();

    Err setProteinQuery(const ProteinQuery &proteinQuery);

    ProteinQuery proteinQuery() const;

    QVector<BottomUpConvolution::DeltaMassStick> getDeltaMassSticks() const;

private:
    BottomUpToIntactDataReader m_bottomUpToIntactDataReader;
    BottomUpConvolution m_bottomUpConvolutionResult;
    ProteinQuery m_proteinQuery;
};

_PMI_END

#endif // BOTTOM_UP_QUERY_BUILDER_H
