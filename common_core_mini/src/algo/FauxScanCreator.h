/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */
#ifndef FAUX_SCAN_CREATOR_H
#define FAUX_SCAN_CREATOR_H

#include "AveragineIsotopeTable.h"
#include "FauxSpectraReader.h"
#include "pmi_common_core_mini_export.h"

// common_stable

#include "CommonFunctions.h"
#include "Point2dListUtils.h"
#include "pmi_common_core_mini_debug.h"
#include <common_constants.h>
#include <common_errors.h>
#include <common_math_types.h>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <random>

_PMI_BEGIN

/*!
 * @brief Takes input from FauxSpectraReader and builds faux ms file.
 *
 *  This class requires that FauxSpectraReader class is first instantiated and then
 *  initiated w/ init(parameters....) to work.
 *
 */
class PMI_COMMON_CORE_MINI_EXPORT FauxScanCreator
{
public:
    struct Scan {
        //// Essential
        double retTimeMinutes = -1;
        int scanLevel = 1;
        int scanNumber = -1;
        point2dList scanIons;

        ////Optional
        QString fragmentationType;
        int precursorScanNumber = -1;
        double dIsolationMass = 0;
        double dMonoIsoMass = 0;
        int nChargeState = 0;
        double lowerWindowOffset = 0;
        double upperWindowOffset = 0;
    };

    FauxScanCreator();

    ~FauxScanCreator();

    /*!
     * @brief After class instantiation, this method must be called to initialze class before use.
     */
    Err init(const FauxSpectraReader &fauxSpectraReader,
             const FauxSpectraReader::FauxScanCreatorParameters &parameters);

    /*!
     * @brief constructs all feature peaks in MS1 domain
     */
    Err constructMS1Scans();

    /*!
     * @brief bulids all MSn scans.  Should probably called after MS1 scans are created.
     */
    Err constructMS2Scans();

    /*!
     * @brief Returns number of scans present in built ms file.
     */
    int numberOfScans() const;

    /*!
     * @brief Returns all created scans to build ms file.
     */
    QVector<Scan> scans() const;

private:
    void addNoiseToScans();

    Err addAveragineIsotopesToMSnScan(const point2dList &inPoints, point2dList *outPoints);

    point2dList filterOutMzRange(const point2dList &inPoints);

    point2dList addChargeStatePlusTwoToScan(const point2dList &inPoints,
                                            double intensityAttenuation = 0.2);

    point2dList filterOutByIntensity(const point2dList &inPoints, double intensityThreshold);

    Err prettifyMSnScan(const point2dList &inPoints, int precursorCharge,
                        double intensityAttenuation, point2dList *outPoint);

    int convertRTToScanNumber(double rt) const;

    Err createAveragineTable();

    Err createScansArray();

private:
    int m_numberOfScans;
    QVector<Scan> m_scans;
    QVector<Eigen::RowVectorXd> m_averagineTable;
    FauxSpectraReader::FauxScanCreatorParameters m_parameters;
    FauxSpectraReader m_fauxSpectraReader;
};

_PMI_END

#endif // FAUX_SCAN_CREATOR_H