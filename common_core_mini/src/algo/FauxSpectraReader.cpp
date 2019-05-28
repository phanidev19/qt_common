/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "FauxSpectraReader.h"
#include "CsvReader.h"
#include "FeatureFinderParameters.h"

#include <QFileInfo>

#include <random>

_PMI_BEGIN

///// Column Names of Faux Peptide CSV
const QString dMassObject = QStringLiteral("MassObject");
const QString dRetTimeMinutes = QStringLiteral("RetTimeMinutes");
const QString dIntensity = QStringLiteral("Intensity");
const QString dPeakWidthMinutes = QStringLiteral("PeakWidthMinutes");
const QString dMSLevel = QStringLiteral("MSLevel");
const QString dChargesOrder = QStringLiteral("ChargesOrder");
const QString dChargesRatios = QStringLiteral("ChargesRatios");
const QString dExtraMzIons = QStringLiteral("ExtraMzIons");
const QString dFragmentationType = QStringLiteral("FragmentationType");
const QString dModificationPosition = QStringLiteral("ModificationPositions");
const QString dModificationNames = QStringLiteral("ModificationNames");

////FauxScanCreatorParameters
const QString dScansPerSecond = QStringLiteral("ScansPerSecond");
const QString dMedianNoiseLevelMS1 = QStringLiteral("MedianNoiseLevelMS1");
const QString dMinMz = QStringLiteral("MinMz");
const QString dMaxMz = QStringLiteral("MaxMz");
const QString dLowerWindowOffset = QStringLiteral("LowerWindowOffset");
const QString dUpperWindowOffset = QStringLiteral("UpperWindowOffset");

///// Column Names of Faux Peptide Parameters CSV
const QString dFauxScanParameter = QStringLiteral("fauxScanParameter");
const QString dFauxScanParameterValue = QStringLiteral("fauxScanParameterValue");

// TODO (Drew Nichols 2019-04-11)  These are duplicated everywhere.  Consolidate
inline Err toDoubleAndSetParameter(const QString &str, double *out)
{
    bool ok;
    *out = str.toDouble(&ok);
    if (!ok) {
        return kError;
    }
    return kNoErr;
}

// TODO (Drew Nichols 2019-04-11)  These are duplicated everywhere.  Consolidate
inline Err toIntAndSetParameter(const QString &str, int *out)
{
    bool ok;
    *out = str.toInt(&ok);
    if (!ok) {
        rrr(kError);
    }
    return kNoErr;
}

// TODO (Drew Nichols 2019-04-12)  Put this somewhere it can be used by all classes.
inline Err checkSizeEquality(int s1, int s2)
{
    Err e = kNoErr;

    if (s1 != s2) {
        criticalCoreMini() << "Row size is not as expected";
        rrr(kError);
    }

    return e;
}

// Intentinally not const
inline Err readExtraMzIonsToPoint2d(QString ions, point2d *pointOfReturn)
{

    Err e = kNoErr;

    const QStringList ionsSplit = ions.split("/");
    const size_t expectedSizeOfSplit = 2;

    e = checkSizeEquality(ionsSplit.size(), expectedSizeOfSplit); ree;

    bool ok = true;
    double mz = ionsSplit[0].toDouble(&ok);
    double intensity = ionsSplit[1].toDouble(&ok);

    if (!ok) {
        rrr(kError);
    }

    *pointOfReturn = point2d(mz, intensity);

    return e;
}

//// Yes, this is not supposed to be const.
// TODO (Drew Nichols 2019-04-12)  Put this somewhere it can be used by all classes.
inline Err csvReaderInitialization(CsvReader &reader, const QString &filePath = NULL)
{
    Err e = kNoErr;

    if (filePath != NULL) {
        debugCoreMini() << "File:" << filePath;
    }

    reader.setQuotationCharacter('"');
    reader.setFieldSeparatorChar(',');

    if (!reader.open()) {
        warningCoreMini() << "File Not opened";
        rrr(kError);
    }

    return e;
}

template <typename T>
// TODO (Drew Nichols 2019-04-12)  Put this somewhere it can be used by all classes.
inline Err parameterIsSet(T param, const QString &columnName)
{
    Err e = kNoErr;

    if (param == FauxSpectraReader::NOT_SET) {
        criticalCoreMini() << QString("%1 is not set").arg(columnName);
        rrr(kError);
    }

    return e;
}

// TODO (Drew Nichols 2019-04-12)  Put this somewhere it can be used by all classes.
inline Err emptyCheckReturn(bool checked, const QString &nameOfObject)
{
    Err e = kNoErr;

    if (checked) {
        criticalCoreMini() << QString("%1 is Empty!").arg(nameOfObject);
        rrr(kError);
    }

    return e;
}

inline Err checkIfHeaderConformsAsExpected(const QStringList &row,
                                           const QStringList &columnsToCheck)
{

    Err e = kNoErr;

    e = checkSizeEquality(row.size(), columnsToCheck.size()); ree;

    for (int i = 0; i < row.size(); ++i) {
        if (row[i].toLower() != columnsToCheck[i].toLower()) {
            warningCoreMini() << row[i] << columnsToCheck[i];
            warningCoreMini() << "File Header does not conform to msfaux parameters format!";
            rrr(kError);
        }
    }

    return e;
}

// TODO (Drew Nichols 2019-04-23)  Put this somewhere it can be used by all classes.
inline double randomNumberInRange(int starter, int stopper)
{
    double r = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
    return starter + r * (stopper - starter);
}

bool FauxSpectraReader::mzRangeCheck(double mw, int charge)
{
    double mzValue = (mw + charge) / static_cast<double>(charge);
    return mzValue > m_parameters.minMz && mzValue < m_parameters.maxMz ? true : false;
}

FauxSpectraReader::FauxSpectraReader()
{
}

FauxSpectraReader::~FauxSpectraReader()
{
}

Err FauxSpectraReader::readMSFauxCSV(const QString &msFauxCSVFile)
{
    Err e = kNoErr;

    CsvReader reader(msFauxCSVFile);
    e = csvReaderInitialization(reader, msFauxCSVFile); ree;

    enum msFauxCsvColumns {
        massObject,
        retTimeMinutes,
        intensity,
        peakWidthMinutes,
        msLevel,
        chargesOrder,
        chargesRatios,
        extraMzIons,
        fragmentationType,
        modificationPosition,
        modificationNames,
        SIZE_MS_FAUX_CSV_COLUMNS
    };

    //// Check to see if header conforms.
    if (reader.hasMoreRows()) {

        QStringList row(reader.readRow());

        QStringList msFauxCsvColumnsList({ dMassObject, dRetTimeMinutes, dIntensity,
                                           dPeakWidthMinutes, dMSLevel, dChargesOrder,
                                           dChargesRatios, dExtraMzIons, dFragmentationType,
                                           dModificationPosition, dModificationNames });

        e = checkIfHeaderConformsAsExpected(row, msFauxCsvColumnsList);
        ree;
    }

    while (reader.hasMoreRows()) {

        QStringList row(reader.readRow());

        e = checkSizeEquality(row.size(), SIZE_MS_FAUX_CSV_COLUMNS); ree;

        FauxPeptideRow fauxPeptideRow;

        fauxPeptideRow.massObject = row[massObject];
        fauxPeptideRow.fragmentationType = row[fragmentationType];

        e = toDoubleAndSetParameter(row[retTimeMinutes], &fauxPeptideRow.retTimeMinutes); ree;
        e = toDoubleAndSetParameter(row[intensity], &fauxPeptideRow.intensity); ree;
        e = toIntAndSetParameter(row[msLevel], &fauxPeptideRow.msLevel); ree;

        if (!row[peakWidthMinutes].isEmpty()) {
            e = toDoubleAndSetParameter(row[peakWidthMinutes], &fauxPeptideRow.peakWidthMinutes); ree;
        }

        // Build fauxPeptideRow.chargeOrderAndRatios based on MS Level
        if (fauxPeptideRow.msLevel == 1) {

            if (!row[chargesOrder].isEmpty() && !row[chargesRatios].isEmpty()) {
                QStringList chargesOrderSplit = row[chargesOrder].split(";");
                QStringList chargesRatiosSplit = row[chargesRatios].split(";");

                e = checkSizeEquality(chargesOrderSplit.size(), chargesRatiosSplit.size()); ree;

                for (int i = 0; i < static_cast<int>(chargesOrderSplit.size()); ++i) {
                    int chargeState = NOT_SET;
                    e = toIntAndSetParameter(chargesOrderSplit[i], &chargeState); ree;

                    double chargeStateRatio = NOT_SET;
                    e = toDoubleAndSetParameter(chargesRatiosSplit[i], &chargeStateRatio); ree;

                    fauxPeptideRow.chargeOrderAndRatios.insert(chargeState, chargeStateRatio);
                }
            }
        } else {
            int chargeState = NOT_SET;
            e = toIntAndSetParameter(row[chargesOrder], &chargeState); ree;

            fauxPeptideRow.chargeOrderAndRatios.insert(chargeState, NOT_SET);
        }

        // Build fauxPeptideRow.modificationPositionName based on MS Level
        if (!row[modificationPosition].isEmpty() && !row[modificationNames].isEmpty()) {
            QStringList modificationPositions = row[modificationPosition].split(";");
            QStringList modificationNamesSplit = row[modificationNames].split(";");

            e = checkSizeEquality(modificationPositions.size(), modificationNamesSplit.size()); ree;

            for (int i = 0; i < static_cast<int>(modificationNamesSplit.size()); ++i) {
                int modPosition = NOT_SET;
                e = toIntAndSetParameter(modificationPositions[i], &modPosition); ree;

                fauxPeptideRow.modificationPositionName.insert(modPosition,
                                                               modificationNamesSplit[i]);
            }
        }

        // Build fauxPeptideRow.ExtraMzIons
        if (!row[extraMzIons].isEmpty()) {
            QStringList extraMzIonsSplit = row[extraMzIons].split(";");
            for (const QString &extraMz : extraMzIonsSplit) {
                point2d pointOfReturn;
                e = readExtraMzIonsToPoint2d(extraMz, &pointOfReturn); ree;

                fauxPeptideRow.ExtraMzIons.push_back(pointOfReturn);
            }
        }

        //// Both MS1 and MSn should have these populated.
        e = parameterIsSet(fauxPeptideRow.retTimeMinutes, dRetTimeMinutes); ree;
        e = parameterIsSet(fauxPeptideRow.intensity, dIntensity); ree;
        e = parameterIsSet(fauxPeptideRow.msLevel, dMSLevel); ree;

        // Check if all necessary columns for MS1 scans are set.
        if (fauxPeptideRow.msLevel == 1) {
            e = emptyCheckReturn(fauxPeptideRow.massObject.isEmpty(), "Mass Object"); ree;
            e = emptyCheckReturn(fauxPeptideRow.chargeOrderAndRatios.isEmpty(),
                                 "ChargeOrderAndRatios"); ree;
            e = parameterIsSet(fauxPeptideRow.massObject, dMassObject); ree;
            e = parameterIsSet(fauxPeptideRow.peakWidthMinutes, dPeakWidthMinutes); ree;
        }

        m_fauxPeptidesToCreate.push_back(fauxPeptideRow);
    }

    std::sort(m_fauxPeptidesToCreate.begin(), m_fauxPeptidesToCreate.end(),
              [](const FauxPeptideRow &a, const FauxPeptideRow &b) {
                  return a.retTimeMinutes < b.retTimeMinutes;
              });

    return e;
}

Err FauxSpectraReader::readMSFauxParametersCSV(const QString &msFauxCSVParametersFile)
{
    Err e = kNoErr;

    bool parametersFileExists
        = QFileInfo::exists(msFauxCSVParametersFile) && QFileInfo(msFauxCSVParametersFile).isFile();

    if (!parametersFileExists) {
        warningCoreMini() << "No parameters file found.  Using default settings";
        return e;
    }

    CsvReader reader(msFauxCSVParametersFile);
    e = csvReaderInitialization(reader, msFauxCSVParametersFile);
    ree;

    enum FauxScanCreatorParametersCSVColumns {
        fauxScanParameter,
        fauxScanParameterValue,
        SIZE_FAUX_SCAN_CREATOR_PARAMETERS
    };

    //// Check to see if header conforms.
    if (reader.hasMoreRows()) {
        QStringList row(reader.readRow());

        QStringList msFauxCsvColumnsList({ dFauxScanParameter, dFauxScanParameterValue });

        e = checkIfHeaderConformsAsExpected(row, msFauxCsvColumnsList);
        ree;
    }

    while (reader.hasMoreRows()) {

        QStringList row(reader.readRow());

        e = checkSizeEquality(row.size(), SIZE_FAUX_SCAN_CREATOR_PARAMETERS);
        ree;

        // TODO (Drew Nichols 2019-04-12)  Think of a way to abstract these.
        ///// Is it even worth it?
        if (row[fauxScanParameter].toLower() == dScansPerSecond.toLower()) {
            e = toIntAndSetParameter(row[fauxScanParameterValue], &m_parameters.scansPerSecond);
            ree;
        }
        if (row[fauxScanParameter].toLower() == dMedianNoiseLevelMS1.toLower()) {
            e = toDoubleAndSetParameter(row[fauxScanParameterValue],
                                        &m_parameters.medianNoiseLevelMS1);
            ree;
        }
        if (row[fauxScanParameter].toLower() == dMinMz.toLower()) {
            e = toDoubleAndSetParameter(row[fauxScanParameterValue], &m_parameters.minMz);
            ree;
        }
        if (row[fauxScanParameter].toLower() == dMaxMz.toLower()) {
            e = toDoubleAndSetParameter(row[fauxScanParameterValue], &m_parameters.maxMz);
            ree;
        }
        if (row[fauxScanParameter].toLower() == dLowerWindowOffset.toLower()) {
            e = toDoubleAndSetParameter(row[fauxScanParameterValue],
                                        &m_parameters.lowerWindowOffset);
            ree;
        }
        if (row[fauxScanParameter].toLower() == dUpperWindowOffset.toLower()) {
            e = toDoubleAndSetParameter(row[fauxScanParameterValue],
                                        &m_parameters.upperWindowOffset);
            ree;
        }
    }

    return e;
}

QVector<FauxSpectraReader::FauxPeptideRow> FauxSpectraReader::fauxPeptidesToCreate() const
{
    return m_fauxPeptidesToCreate;
}

FauxSpectraReader::FauxScanCreatorParameters FauxSpectraReader::parameters() const
{
    return m_parameters;
}

void FauxSpectraReader::setFauxPeptidesToCreate(const QVector<FauxPeptideRow> &fauxPeptides)
{
    m_fauxPeptidesToCreate = fauxPeptides;

    std::sort(
        m_fauxPeptidesToCreate.begin(), m_fauxPeptidesToCreate.end(),
        [](const FauxSpectraReader::FauxPeptideRow &a, const FauxSpectraReader::FauxPeptideRow &b) {
            return a.retTimeMinutes < b.retTimeMinutes;
        });
}

QVector<FauxSpectraReader::FauxPeptideRow>
FauxSpectraReader::buildRandomPeptideDataMS1Only(int numberOfPeptides, double maxRtTime, int seed)
{

    srand(seed);

    ImmutableFeatureFinderParameters ffParams;
    SettableFeatureFinderParameters ffUserParams;
    QVector<FauxPeptideRow> fauxPeptideRows;

    while (fauxPeptideRows.size() < numberOfPeptides) {

        double mw = randomNumberInRange(ffUserParams.minFeatureMass, ffUserParams.maxFeatureMass);
        double rt = randomNumberInRange(1, maxRtTime);
        double intensity = randomNumberInRange(1e5, 1e9);
        double peakWidth = randomNumberInRange(1, 4) / 10;

        int numberOfCharges = static_cast<int>(qRound(randomNumberInRange(1, 4)));
        int chargeStartPoint
            = static_cast<int>(qRound(randomNumberInRange(-1, ffParams.maxChargeState - 2)));

        Eigen::RowVectorXd chargeIntensitiesInitializer
            = Eigen::VectorXd::Random(numberOfCharges).cwiseAbs();

        double normalizationDenominator = chargeIntensitiesInitializer.maxCoeff() == 0
            ? 1
            : chargeIntensitiesInitializer.maxCoeff();

        chargeIntensitiesInitializer /= normalizationDenominator;
        QVector<double> chargeIntensities(numberOfCharges);
        for (int i = 0; i < chargeIntensitiesInitializer.cols(); ++i) {
            chargeIntensities[i] = chargeIntensitiesInitializer(i);
        }
        std::sort(chargeIntensities.rbegin(), chargeIntensities.rend());

        QVector<int> charges(numberOfCharges);
        std::iota(charges.begin(), charges.end(), chargeStartPoint);

        QVector<int> chargesFinal;
        for (int i = 0; i < charges.size(); ++i) {
            int newCharge = charges[i];
            if (newCharge > 0 && newCharge < ffParams.maxChargeState
                && mzRangeCheck(mw, newCharge)) {
                chargesFinal.push_back(newCharge);
            }
        }

        if (chargesFinal.isEmpty()) {
            continue;
        }

        chargeIntensities.resize(chargesFinal.size());
        QHash<int, double> chargeOrderAndRatios;
        for (int i = 0; i < chargesFinal.size(); ++i) {
            chargeOrderAndRatios.insert(chargesFinal[i], chargeIntensities[i]);
        }

        FauxSpectraReader::FauxPeptideRow fauxPeptideRow;
        fauxPeptideRow.retTimeMinutes = rt;
        fauxPeptideRow.massObject = QString::number(mw);
        fauxPeptideRow.intensity = intensity;
        fauxPeptideRow.peakWidthMinutes = peakWidth;
        fauxPeptideRow.chargeOrderAndRatios = chargeOrderAndRatios;

        fauxPeptideRows.push_back(fauxPeptideRow);
    }

    //// Imparitive that sorted from low to high retention time or msFileReaderSimulated will throw
    ////a fit and only create scans till the first value is less the previous.
    std::sort(fauxPeptideRows.begin(), fauxPeptideRows.end(),
              [](const FauxPeptideRow &a, const FauxPeptideRow &b) {
                  return a.retTimeMinutes < b.retTimeMinutes;
              });
    return fauxPeptideRows;
}

_PMI_END