/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */
#ifndef BYOPHYSICAL_PARAMETERS_H
#define BYOPHYSICAL_PARAMETERS_H

#include "Point2dListUtils.h"
#include "common_constants.h"
#include "pmi_common_core_mini_debug.h"
#include <common_errors.h>
#include <common_math_types.h>

namespace byophysicaConsts
{

//// Monoisotopic masses of amino acids.
//// These should be replaced by the ones byonic uses.
const double monoA = 71.037114;
const double monoC = 103.00919;
const double monoD = 115.02694;
const double monoE = 129.04259;
const double monoF = 147.06841;
const double monoG = 57.02146;
const double monoH = 137.05891;
const double monoI = 113.08406;
const double monoK = 128.09496;
const double monoL = 113.08406;
const double monoM = 131.04049;
const double monoN = 114.04293;
const double monoP = 97.05276;
const double monoQ = 128.05858;
const double monoR = 156.10111;
const double monoS = 87.03203;
const double monoT = 101.04768;
const double monoV = 99.06841;
const double monoW = 186.07931;
const double monoY = 163.06333;

//// Hydrophobicities of amino acids.
////
///https://www.cambridge.org/core/books/water-in-biological-and-chemical-processes/hydrophobic-effect/E621BF61FF94647D30C90D3C1720CEBD
const double hydrophobicityA = 1.8;
const double hydrophobicityC = 2.5;
const double hydrophobicityD = -3.5;
const double hydrophobicityE = -3.5;
const double hydrophobicityF = 2.8;
const double hydrophobicityG = -0.4;
const double hydrophobicityH = -3.2;
const double hydrophobicityI = 4.5;
const double hydrophobicityK = -3.9;
const double hydrophobicityL = 3.8;
const double hydrophobicityM = 1.9;
const double hydrophobicityN = -3.5;
const double hydrophobicityP = -1.6;
const double hydrophobicityQ = -3.5;
const double hydrophobicityR = -4.5;
const double hydrophobicityS = -0.8;
const double hydrophobicityT = -0.7;
const double hydrophobicityV = 4.2;
const double hydrophobicityW = -0.9;
const double hydrophobicityY = -1.3;

// Prediction of Molar Extinction Coefficients of Proteins and Peptides Using UV Absorption of the
// Constituent Amino Acids at 214 nm  To Enable Quantitative Reverse Phase High Performance Liquid
// Chromatography Mass Spectrometry Analysis  J.Agric.Food Chem. 2007, 55, 5445 5451
const double extinction214nmA = 32;
const double extinction214nmC = 225;
const double extinction214nmD = 58;
const double extinction214nmE = 78;
const double extinction214nmF = 5200;
const double extinction214nmG = 21;
const double extinction214nmH = 5125;
const double extinction214nmI = 45;
const double extinction214nmK = 41;
const double extinction214nmL = 45;
const double extinction214nmM = 980;
const double extinction214nmN = 136;
const double extinction214nmP = 2675;
const double extinction214nmQ = 142;
const double extinction214nmR = 102;
const double extinction214nmS = 34;
const double extinction214nmT = 41;
const double extinction214nmV = 43;
const double extinction214nmW = 29050;
const double extinction214nmY = 5375;
const double extinction214nmPeptideBond = 923;

QHash<QString, double> buildHydrophobicityTable();

QHash<QString, double> buildAAExtinctionAt214nm();

QHash<QString, double> buildMasses();

// TODO (Drew Nichols 2019-04-11) Comment this
static const QHash<QString, double> HYDROPHOBICITIES = buildHydrophobicityTable();

// TODO (Drew Nichols 2019-04-11) Comment this
static const QHash<QString, double> EXTINCTION_214nmCOEFFS = buildAAExtinctionAt214nm();

// TODO (Drew Nichols 2019-04-11) Comment this
static const QHash<QString, double> MONOISOTOPIC_MASSES_AMINO_ACIDS = buildMasses();
}

namespace byophysica
{

/*!
 * @brief Extracts value from modificationName field format.
 *  CONCRETELY returns double 0.9840 from (Deamidated/0.9840)
 *
 *   Intentionally not const
 */
double extractModValueFromString(QString modString);

/*!
 * @brief filters out alpha numeric characters that are not amino acids from input
 *
 *  If letter is lowercase amino acid, it will return uppercase
 *  in the return string.
 */
QString cleanProteinSequence(const QString &proteinSequence);

/*!
 * @brief adds absorbance of all amino bonds and sidechains @ 214nm
 *
 *  See comments in byophysicaConsts namespace  above for details
 *   of value origins.
 */
double calculateUVAbsorbance214nm(const QString &peptideSequence);

/*!
 * @brief adds hydrophobicities of all amino acids
 *
 *  See comments in byophysicaConsts namespace  above for details
 *   of value origins.
 */
double calculateHydrophobicity(const QString &peptideSequence);

/*!
 * @brief adds monoisotopic masses of all amino acids + H2O
 *
 *  See comments in byophysicaConsts namespace  above for details
 *   of value origins.
 */
double calculateMassOfPeptide(const QString &peptideSequence,
                              QHash<int, QString> modificationPositionName = QHash<int, QString>());

/*!
 * @brief returns list of peptide digests based on specified enzyme cleave points
 *
 *  See comments in byophysicaConsts namespace  above for details
 *   of value origins.
 */
QStringList digestProtein(const QString &proteinSequence, const QString &ctermCleavePoints);

/*!
 * @brief returns list of fragment mz's
 *
 *  See comments in byophysicaConsts namespace  above for details
 *   of value origins.
 */
pmi::point2dList
fragmentPeptide(const QString &peptide, const QString &fragmentationType, double intensity,
                QHash<int, QString> modificationPositionName = QHash<int, QString>());

/*!
 * @brief charged mz based on mass of protein and given charge.
 *
 */
double calculateChargedMzFromMW(double mw, int charge);

/*!
 * @brief uncharged mass from charge and mz.
 *
 */
double calculateUnchargedMassFromMzCharge(double mz, int charge);
}

#endif // BYOPHYSICAL_PARAMETERS_H