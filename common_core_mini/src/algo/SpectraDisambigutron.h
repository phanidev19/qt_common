/*
* Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Drew Nichols anichols@proteinmetrics.com
*/

#ifndef SPECTRA_DISAMBIGUTRON_H
#define SPECTRA_DISAMBIGUTRON_H

#include <common_constants.h>
#include <common_errors.h>
#include "FeatureFinderParameters.h"
#include "pmi_common_core_mini_export.h"
#include <pmi_core_defs.h>


#include <Eigen/Core>
#include <Eigen/Sparse>

_PMI_BEGIN

/*!
* \brief Attempts to disambiguate charge clusters that overlap.
*
* Class works by attempting to match theoretical distances w/ observed 
* in order to pick out the correct peaks for processing.
* This is done in place of using a combfilter w/ the distance typically
* set to 0.02 (ImmutableFeatureFinderParameters errorRange).
*/
class PMI_COMMON_CORE_MINI_EXPORT SpectraDisambigutron
{

public:
    SpectraDisambigutron();
    ~SpectraDisambigutron();
    Err init();

    /*!
    * \brief Returns disambiguated scanSegment.
    *
    *  If two charge clusters overlap, this method will isoloate the ions for the one of interest.
    */
    Err removeOverlappingIonsFromScan(const Eigen::SparseVector<double> &inputScan, int charge,
                                      Eigen::SparseVector<double> *outputScan);

private:
    /*!
    * \brief Builds combfilters used to extract as well as theoretical location of the spikes.
    */
    void buildSuccessiveCombFilters();

private:
    ImmutableFeatureFinderParameters m_ffParams;
    std::vector<Eigen::SparseMatrix<double>> m_successiveCombFilters;
    std::vector<std::vector<int>> m_combSpikes;
};

_PMI_END

#endif // SPECTRA_DISAMBIGUTRON_H