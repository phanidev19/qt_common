/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "ByophysicalAAParameters.h"

QHash<QString, double> byophysicaConsts::buildHydrophobicityTable()
{
    QHash<QString, double> hydrophobicity;
    hydrophobicity.insert("A", hydrophobicityA);
    hydrophobicity.insert("C", hydrophobicityC);
    hydrophobicity.insert("D", hydrophobicityD);
    hydrophobicity.insert("E", hydrophobicityE);
    hydrophobicity.insert("F", hydrophobicityF);
    hydrophobicity.insert("G", hydrophobicityG);
    hydrophobicity.insert("H", hydrophobicityH);
    hydrophobicity.insert("I", hydrophobicityI);
    hydrophobicity.insert("K", hydrophobicityK);
    hydrophobicity.insert("L", hydrophobicityL);
    hydrophobicity.insert("M", hydrophobicityM);
    hydrophobicity.insert("N", hydrophobicityN);
    hydrophobicity.insert("P", hydrophobicityP);
    hydrophobicity.insert("Q", hydrophobicityQ);
    hydrophobicity.insert("R", hydrophobicityR);
    hydrophobicity.insert("S", hydrophobicityS);
    hydrophobicity.insert("T", hydrophobicityT);
    hydrophobicity.insert("V", hydrophobicityV);
    hydrophobicity.insert("W", hydrophobicityW);
    hydrophobicity.insert("Y", hydrophobicityY);

    return hydrophobicity;
}

QHash<QString, double> byophysicaConsts::buildAAExtinctionAt214nm()
{
    QHash<QString, double> extinction214nm;
    extinction214nm.insert("A", extinction214nmA);
    extinction214nm.insert("C", extinction214nmC);
    extinction214nm.insert("D", extinction214nmD);
    extinction214nm.insert("E", extinction214nmE);
    extinction214nm.insert("F", extinction214nmF);
    extinction214nm.insert("G", extinction214nmG);
    extinction214nm.insert("H", extinction214nmH);
    extinction214nm.insert("I", extinction214nmI);
    extinction214nm.insert("K", extinction214nmK);
    extinction214nm.insert("L", extinction214nmL);
    extinction214nm.insert("M", extinction214nmM);
    extinction214nm.insert("N", extinction214nmN);
    extinction214nm.insert("P", extinction214nmP);
    extinction214nm.insert("Q", extinction214nmQ);
    extinction214nm.insert("R", extinction214nmR);
    extinction214nm.insert("S", extinction214nmS);
    extinction214nm.insert("T", extinction214nmT);
    extinction214nm.insert("V", extinction214nmV);
    extinction214nm.insert("W", extinction214nmW);
    extinction214nm.insert("Y", extinction214nmY);
    extinction214nm.insert("BOND", extinction214nmPeptideBond);

    return extinction214nm;
}

QHash<QString, double> byophysicaConsts::buildMasses()
{

    QHash<QString, double> monoMasses;
    monoMasses.insert("A", monoA);
    monoMasses.insert("C", monoC);
    monoMasses.insert("D", monoD);
    monoMasses.insert("E", monoE);
    monoMasses.insert("F", monoF);
    monoMasses.insert("G", monoG);
    monoMasses.insert("H", monoH);
    monoMasses.insert("I", monoI);
    monoMasses.insert("K", monoK);
    monoMasses.insert("L", monoL);
    monoMasses.insert("M", monoM);
    monoMasses.insert("N", monoN);
    monoMasses.insert("P", monoP);
    monoMasses.insert("Q", monoQ);
    monoMasses.insert("R", monoR);
    monoMasses.insert("S", monoS);
    monoMasses.insert("T", monoT);
    monoMasses.insert("V", monoV);
    monoMasses.insert("W", monoW);
    monoMasses.insert("Y", monoY);

    return monoMasses;
}

double byophysica::extractModValueFromString(QString modString)
{
    double modValue = 0;
    bool ok = true;

    ////TODO method to extract double from string is not elegent or robust.  Consider regex.
    modString = modString.split('/').back().replace(")", "");

    if (modString.isEmpty()) {
        return modValue;
    }

    modValue = modString.toDouble(&ok);

    Q_ASSERT(ok);
    return modValue;
}

QString byophysica::cleanProteinSequence(const QString &proteinSequence)
{

    const QByteArray tmp = proteinSequence.toUpper().toLocal8Bit();
    const QByteArray allowedChars("ACDEFGHIKLMNPQRSTVWYZ");

    QString returnString;
    returnString.reserve(tmp.length());
    for (int i = 0; i < tmp.length(); ++i) {
        if (allowedChars.contains(tmp[i])) {
            returnString += tmp[i];
        }
    }

    return returnString;
}

double byophysica::calculateUVAbsorbance214nm(const QString &peptideSequence)
{

    const QString cleanedPeptideSequence = cleanProteinSequence(peptideSequence);

    double absorbance = (cleanedPeptideSequence.size() - 1)
        * byophysicaConsts::EXTINCTION_214nmCOEFFS.value("BOND");

    for (const QString &aminoAcid : cleanedPeptideSequence) {
        if (!byophysicaConsts::EXTINCTION_214nmCOEFFS.keys().contains(aminoAcid)) {
            continue;
        }
        absorbance += byophysicaConsts::EXTINCTION_214nmCOEFFS.value(aminoAcid);
    }

    return absorbance;
}

double byophysica::calculateHydrophobicity(const QString &peptideSequence)
{

    const QString cleanedPeptideSequence = cleanProteinSequence(peptideSequence);

    double hydrophobicity = 0;
    for (const QString &aminoAcid : cleanedPeptideSequence) {
        if (!byophysicaConsts::HYDROPHOBICITIES.keys().contains(aminoAcid)) {
            continue;
        }
        hydrophobicity += byophysicaConsts::HYDROPHOBICITIES.value(aminoAcid);
    }

    return hydrophobicity;
}

double byophysica::calculateMassOfPeptide(const QString &peptideSequence,
                                          QHash<int, QString> modificationPositionName)
{
    const QString cleanedPeptideSequence = cleanProteinSequence(peptideSequence);
    double mass = WATER;
    for (const QString &aa : cleanedPeptideSequence) {
        mass += byophysicaConsts::MONOISOTOPIC_MASSES_AMINO_ACIDS.value(aa);
    }

    for (auto it = modificationPositionName.begin(); it != modificationPositionName.end(); ++it) {
        mass += extractModValueFromString(it.value());
    }

    return mass;
}

QStringList byophysica::digestProtein(const QString &proteinSequence,
                                      const QString &ctermCleavePoints)
{

    QStringList digestedPeptides;

    const QString cleanedPeptideSequence = cleanProteinSequence(proteinSequence);

    QString peptideBuilder;
    for (int i = 0; i < cleanedPeptideSequence.size(); ++i) {
        QString residue = cleanedPeptideSequence[i];
        if (!ctermCleavePoints.contains(residue)) {
            peptideBuilder += residue;
        } else {
            peptideBuilder += residue;
            digestedPeptides.push_back(peptideBuilder);
            peptideBuilder.clear();
        }
    }

    if (!peptideBuilder.isEmpty()) {
        digestedPeptides.push_back(peptideBuilder);
    }

    return digestedPeptides;
}

pmi::point2dList byophysica::fragmentPeptide(const QString &peptide,
                                             const QString &fragmentationType, double intensity,
                                             QHash<int, QString> modificationPositionName)
{
    // TODO (Drew Nichols 2019-04-11) Impliment different fragmentations later.
    Q_UNUSED(fragmentationType);

    // TODO (Drew Nichols 2019-04-12) Replace these w/ recurrent neural net for b, y intensity
    // prediction
    const double generalBIonFragmentIntensity = 0.25;
    const double generalYIonFragmentIntensity = 0.75;

    pmi::point2dList fragmentPoints;
    double yFragPoint = WATER + PROTON;
    double bFragPoint = PROTON;
    for (int i = 0; i < peptide.size(); ++i) {
        if (i != peptide.size() - 1) {
            bFragPoint += byophysicaConsts::MONOISOTOPIC_MASSES_AMINO_ACIDS.value(peptide[i]);
            bFragPoint += extractModValueFromString(modificationPositionName.value(i + 1));
            fragmentPoints.push_back(point2d(bFragPoint, generalBIonFragmentIntensity * intensity));
        }
        yFragPoint += byophysicaConsts::MONOISOTOPIC_MASSES_AMINO_ACIDS.value(
            peptide[peptide.size() - 1 - i]);
        yFragPoint += extractModValueFromString(modificationPositionName.value(peptide.size() - i));
        fragmentPoints.push_back(point2d(yFragPoint, generalYIonFragmentIntensity * intensity));
    }

    std::sort(fragmentPoints.begin(), fragmentPoints.end(), point2d_less_x);
    return fragmentPoints;
}

double byophysica::calculateChargedMzFromMW(double mw, int charge)
{
    return (mw + (charge * PROTON)) / static_cast<double>(charge);
}

double byophysica::calculateUnchargedMassFromMzCharge(double mz, int charge)
{
    return (mz * charge) - (charge * PROTON);
}
