/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */
#ifndef FAUX_SPECTRA_READER_H
#define FAUX_SPECTRA_READER_H

// common_stable
#include "AveragineIsotopeTable.h"
#include "CommonFunctions.h"

#include "Point2dListUtils.h"
#include "pmi_common_core_mini_debug.h"
#include "pmi_common_core_mini_export.h"
#include <common_constants.h>
#include <common_errors.h>
#include <common_math_types.h>

_PMI_BEGIN

/*!
 * @brief Reads *.msfaux file w/ features that should be created.
 *
 *  This class must first be instantiated and called before faux scans
 *  can be created.  The fauxPeptideRow data structure is required
 *  by FauxScanCreator.h/cpp to ultimately build faux data.
 *
 */
class PMI_COMMON_CORE_MINI_EXPORT FauxSpectraReader
{
public:
    static const int NOT_SET = -1;

    struct FauxPeptideRow {
        QString massObject;
        double retTimeMinutes = NOT_SET;
        double intensity = NOT_SET;
        double peakWidthMinutes = NOT_SET;
        int msLevel = NOT_SET;
        QHash<int, double> chargeOrderAndRatios;

        ////MS2 Centric Columns
        point2dList ExtraMzIons;
        QString fragmentationType;
        QHash<int, QString> modificationPositionName;

        void clear()
        {
            massObject.clear();
            retTimeMinutes = NOT_SET;
            intensity = NOT_SET;
            peakWidthMinutes = NOT_SET;
            msLevel = NOT_SET;
            chargeOrderAndRatios.clear();

            ////MS2 Centric Columns
            ExtraMzIons.clear();
            fragmentationType.clear();
            modificationPositionName.clear();
        }
    };

    struct FauxScanCreatorParameters {
        int scansPerSecond = 10;
        double medianNoiseLevelMS1 = 100.0;
        double minMz = 200.0;
        double maxMz = 2000.0;
        double lowerWindowOffset = 1.0;
        double upperWindowOffset = 1.0;
    };

public:
    FauxSpectraReader();
    ~FauxSpectraReader();

    /*!
     * @brief Reads *.msfaux file w/ features that should be created
     *
     *  Depending on what kind of scan is being created, various fields
     *  can be omitted which is explained in more detail in FauxScanCreator.h
     *
     *  Header must have the following columns in the following order.
     *  columns are case insensitive.  Some columns also support multiple
     *  entries that use ";" as a separator.
     *
     *  MassObject,
     *  RetTimeMinutes,
     *  Intensity,
     *  PeakWidthMinutes,
     *  MSLevel,
     *  ChargesOrder,
     *  ChargesRatios,
     *  ExtraMzIons,
     *  FragmentationType,
     *  ModificationPositions,
     *  ModificationNames
     *
     *  Details of Column and their inputs:
     *
     * MassObject (Required for MS1, optional for MSn if ExtraMzIons is defined)
     *       can accept either a QString peptide sequence, or a double monoisotopic mass.
     *       in cases where a peptide sequence is entered, mass is cacluated by methods.
     *
     *  RetTimeMinutes(Single entry type double required by all MS1, MSn)
     *
     *  Intensity(Single entry type double required by all MS1, MSn)
     *
     *  PeakWidthMinutes(Single entry type double required by all MS1)
     *
     * MSLevel  (Required by all scans)
     *       i.e. MS1, MS2, MSn scan.  Input is single integer.
     *
     * ChargesOrder (Required by all scans but must be single contain single integer for MSn scans)
     *       can be single integer or multi integer, i.e., 3;2 ordered in decreasing intensity
     *
     * ChargesRatios (Required by MS1 only)
     *       input size must match ChargesOrder and is the ratio of ChargeOrder above
     *       Concretely in Charges Order example, 3 is most abundant charge found
     *       so that will get a value of 1, and 2 any decimal value below.
     *       i.e., if ChargesOrder is 3;2 and ChargesRatios is 1;0.5 charge 3 will be basepeak
     *       and charge 2 will have an intensity half of charge 3.
     *
     * ExtraMzIons(Only used by MSn scans)
     *       Must be in point2d format using "/" to separate mz from intensity.
     *       Can also except multi entries.
     *       Concretely 146.1/1e4; 247.2/1e3 would add two ions to the MSMS scan
     *       @ 146.1 mz w/ intensity 10000 and 247.2 w/ intensity 1000, etc...
     *
     * FragmentationType (Only used by MSn scans)
     *       i.e., hcd, etd, cid, uvpd
     *
     * ModificationPositions(Used by all scans if set.  Is not required by either)
     *       accepts single or multi integer values separated by ";"  Values are 1 based
     *       as to match w/ output from our software.  This column specifies the position/s
     *       of modification in the peptide.  a value of 1;3 means there are mods on the first
     *       and third residue of the peptide.
     *
     * ModificationNames
     *       Input size must match that of ModificationPositions above
     *       i.e., if there are two entries in ModificationPositions, there must be two entries
     *       in ModificationNames as well.  Format is that of Byologic:
     *
     *       Deamidated/0.9840; Dethiomethyl/-48.0034
     *
     *       Concretely, if Modification Positions is set to 1;3 this means that position 1
     *       is deamidated and the residue has 0.9840 added to it in the MSMS as well as
     *       overall monoisotopic mass.  Position 3 is modified -48.0034 respectively.
     */
    Err readMSFauxCSV(const QString &msFauxCSVFile);

    /*!
     * @brief Reads parameters file which is the filename of the *.msfaux w/ .params added to it.
     *
     *  Concretely the parameteres file for filename.msfaux must be filename.msfaux.params
     *  if the file is not present, the default parameters in struct FauxScanCreatorParameters
     *  will be used.
     *
     *  Header must have the following columns in the following order.
     *  columns are case insensitive.
     *
     * fauxScanParameter
     * fauxScanParameterValue
     *
     *   See struct FauxScanCreatorParameters variables that can be set.
     *       ScansPerSecond
     *       MedianNoiseLevelMS1
     *       MinMz
     *       MaxMz
     *       LowerWindowOffset
     *       UpperWindowOffset
     */
    Err readMSFauxParametersCSV(const QString &msFauxCSVParametersFile);

    /*!
     * @brief Returns a a vector or what peptides and scans need to be created
     */
    QVector<FauxPeptideRow> fauxPeptidesToCreate() const;

    /*!
     * @brief Returns parameters Required by FauxScanCreator for building scans.
     */
    FauxScanCreatorParameters parameters() const;

    /*!
     * @brief sets m_fauxPeptidesToCreate when testing
     */
    void setFauxPeptidesToCreate(const QVector<FauxPeptideRow> &fauxPeptides);

    /*!
     * @brief builds randomly generated data for testing purposes.
     *
     *  NOTE:  Data is completely synthetic and may not look like anything found in real maps.
     */
    QVector<FauxPeptideRow> buildRandomPeptideDataMS1Only(int numberOfPeptides, double maxRtTime,
                                                          int seed);

private:
    // TODO (Drew Nichols 2019-04-11)  Add helper faux generators here.

    // BUild this so that if a blank *.msfaux file is provided and *.msfaux.params has
    // key/val pair specifying which helper to use, helpers will write to msfaux file provided
    // as well as build all requirements in memory. Also can use key/val pair to not write if
    // wanted.

    // TODO (Drew Nichols 2019-04-11)  faux features completely random generator
    // TODO (Drew Nichols 2019-04-11)  faux features generator from fasta

    bool mzRangeCheck(double mw, int charge);

private:
    QVector<FauxPeptideRow> m_fauxPeptidesToCreate;
    FauxScanCreatorParameters m_parameters;
};

_PMI_END

#endif // FAUX_SPECTRA_READER_H