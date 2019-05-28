/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "FauxScanCreator.h"
#include "ByophysicalAAParameters.h"
#include "EigenUtilities.h"

const double MASS_SPACING = 100.0;
const double PEAK_WIDTH_DIVIDER = 2.0;

_PMI_BEGIN

FauxScanCreator::FauxScanCreator()
{
    const int upsideDownNines = 666;
    srand(upsideDownNines);
}

FauxScanCreator::~FauxScanCreator()
{
}

Err FauxScanCreator::init(const FauxSpectraReader &fauxSpectraReader,
                          const FauxSpectraReader::FauxScanCreatorParameters &parameters)
{
    Err e = kNoErr;
    m_parameters = parameters;
    m_fauxSpectraReader = fauxSpectraReader;
    e = createScansArray(); ree;
    e = createAveragineTable(); ree;
    return e;
}

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

// TODO (Drew Nichols 2019-04-12)  Put this somewhere it can be used by all classes.
inline Err emptyCheckReturn(bool checkedIsEmpty, const QString &nameOfObject)
{
    Err e = kNoErr;

    if (checkedIsEmpty) {
        criticalCoreMini() << QString("%1 is Empty!").arg(nameOfObject);
        rrr(kError);
    }

    return e;
}

template <typename T>
inline Err checkRangeOfQVector(const QVector<T> &checkThisVector, int index)
{

    Err e = kNoErr;

    if (index < 0 || index >= checkThisVector.size()) {
        criticalCoreMini() << "Index is Out of Range";
        rrr(kError);
    }

    return e;
}

// TODO (Drew Nichols 2019-04-11)  These are duplicated everywhere.  Consolidate
inline double randomNumberInRange(int starter, int stopper)
{
    double r = static_cast<double>(rand()) / static_cast<double>(RAND_MAX);
    return starter + r * (stopper - starter);
}

Err FauxScanCreator::constructMS1Scans()
{
    Err e = kNoErr;

    const QVector<FauxSpectraReader::FauxPeptideRow> &fauxPeptidesToCreate
        = m_fauxSpectraReader.fauxPeptidesToCreate();

    for (const FauxSpectraReader::FauxPeptideRow &row : fauxPeptidesToCreate) {

        if (row.msLevel > 1) {
            continue;
        }

        debugCoreMini() << row.massObject;

        double mw = 0;

        //// checks the input of mass object as to whether it should be double or string
        e = toDoubleAndSetParameter(row.massObject, &mw);
        if (e == kError) {
            e = kNoErr;
            QString peptideSequnce = byophysica::cleanProteinSequence(row.massObject);
            mw = byophysica::calculateMassOfPeptide(peptideSequnce, row.modificationPositionName);
        }

        int mwLookUpIndex = std::floor(mw / MASS_SPACING);
        e = checkRangeOfQVector(m_averagineTable, mwLookUpIndex); ree;
        const Eigen::RowVectorXd &averagineRatios = m_averagineTable[mwLookUpIndex];

        const int peakLength = m_parameters.scansPerSecond * 60 * row.peakWidthMinutes;

        const Eigen::RowVectorXd peak = EigenUtilities::generateGaussianCurve(peakLength);

        const int rtStartIndex = convertRTToScanNumber(row.retTimeMinutes)
            - std::floor(peak.cols() / PEAK_WIDTH_DIVIDER);

        for (auto it = row.chargeOrderAndRatios.begin(); it != row.chargeOrderAndRatios.end();
             ++it) {
            for (int j = 0; j < averagineRatios.cols(); ++j) {
                const double mz = byophysica::calculateChargedMzFromMW(mw + (j * PROTON), it.key());

                Eigen::RowVectorXd peakIsotope
                    = peak * row.intensity * averagineRatios(j) * it.value();

                for (int k = 0; k < peakIsotope.cols(); ++k) {
                    const int scan = k + rtStartIndex + 1;

                    if (scan > -1 && scan < m_numberOfScans) {
                        const point2d scanIon(mz, peakIsotope(k));
                        if (mz > m_parameters.minMz && mz < m_parameters.maxMz) {
                            m_scans[scan].scanIons.push_back(scanIon);
                        }
                    }
                }
            }
        }
    }

    addNoiseToScans();
    return e;
}

Err FauxScanCreator::constructMS2Scans()
{
    Err e = kNoErr;

    const QVector<FauxSpectraReader::FauxPeptideRow> &fauxPeptidesToCreate
        = m_fauxSpectraReader.fauxPeptidesToCreate();

    for (const FauxSpectraReader::FauxPeptideRow &row : fauxPeptidesToCreate) {

        if (row.msLevel < 2) {
            continue;
        }

        const int chargeOfPrecursor = row.chargeOrderAndRatios.keys().front();
        double mw = 0;
        point2dList fragmentIonsPeptide;

        //// checks the input of mass object as to whether it should be double or string
        e = toDoubleAndSetParameter(row.massObject, &mw);
        if (e == kError) {

            e = kNoErr;

            const QString peptideSequence = byophysica::cleanProteinSequence(row.massObject);

            mw = byophysica::calculateMassOfPeptide(peptideSequence, row.modificationPositionName);

            fragmentIonsPeptide
                = byophysica::fragmentPeptide(peptideSequence, row.fragmentationType, row.intensity,
                                              row.modificationPositionName);
        }

        //// Adds extra ions to the primary point2dList fragmentIonsPeptide
        for (const point2d &point : row.ExtraMzIons) {
            point2d pointNew = point;
            pointNew.setY(point.y() * row.intensity);
            fragmentIonsPeptide.push_back(pointNew);
        }

        //// Performs all sorts of machinations as to make MSn scan interesting.
        //// i.e. adds isotopes, double charging if charge >2, and filters out in mz range.
        const double intensityAttenuator = 0.2;
        point2dList prettifiedScan;
        e = prettifyMSnScan(fragmentIonsPeptide, chargeOfPrecursor, intensityAttenuator,
                            &prettifiedScan); ree;

        const int precursorScanIndex = convertRTToScanNumber(row.retTimeMinutes);
        int ms2ScanIndex = precursorScanIndex;
        while (ms2ScanIndex + 1 < m_scans.size()) {

            //// Not const since scan is modified below.
            Scan &scan = m_scans[++ms2ScanIndex];

            //// If scan is already MSn > 1, we don't want to overwrite it.
            if (scan.scanLevel > 1) {
                continue;
            }

            const double chargedMW = byophysica::calculateChargedMzFromMW(mw, chargeOfPrecursor);

            scan.fragmentationType = row.fragmentationType;
            scan.scanLevel = row.msLevel;
            scan.nChargeState = chargeOfPrecursor;
            scan.scanIons.clear();
            scan.scanIons = prettifiedScan;
            // Need to add 1 because scan numbers are not 0 based.
            scan.precursorScanNumber = precursorScanIndex + 1;
            scan.dIsolationMass = chargedMW;
            scan.dMonoIsoMass = chargedMW;
            scan.lowerWindowOffset = m_parameters.lowerWindowOffset;
            scan.upperWindowOffset = m_parameters.upperWindowOffset;

            // NOTE: (Drew Nichols 2019-04-13) an additional minute of scans is added at end of last
            // peak.
            // An MS1 scan will always be found so there will never be an infinte loop.
            break;
        }
    }

    return e;
}

int FauxScanCreator::convertRTToScanNumber(double rt) const
{
    return std::floor(rt * m_parameters.scansPerSecond * 60);
}

Err FauxScanCreator::createScansArray()
{
    Err e = kNoErr;

    const QVector<FauxSpectraReader::FauxPeptideRow> &fauxPeptidesToCreate
        = m_fauxSpectraReader.fauxPeptidesToCreate();

    ////Add 1 to end time to give a bit of a buffer for the last peaks to create.
    int endTime = std::floor(fauxPeptidesToCreate.back().retTimeMinutes) + 1;
    m_numberOfScans = m_parameters.scansPerSecond * 60 * endTime;
    m_scans.reserve(m_numberOfScans);

    if (m_parameters.scansPerSecond < 1) {
        warningCoreMini() << "Scans per second less than 1, Division by zero!";
        rrr(kError);
    }

    for (int i = 0; i < m_numberOfScans; ++i) {
        Scan scan;
        scan.retTimeMinutes = i / (m_parameters.scansPerSecond * 60.0);
        scan.scanNumber = i + 1; // Add 1 here because scanNumber is not zero based.
        m_scans.push_back(scan);
    }

    e = emptyCheckReturn(m_scans.empty(), "Scans Array"); ree;

    return e;
}

Err FauxScanCreator::createAveragineTable()
{
    Err e = kNoErr;
    ////// Load averagine table
    using namespace coreMini;
    AveragineIsotopeTable::Parameters parameters;
    parameters.interIsoTrim = 0.00001;
    parameters.finalIsoTrim = 0.0001;
    parameters.massSpacing = static_cast<int>(MASS_SPACING);

    AveragineIsotopeTable averagineIsotopeTableGenerator(parameters);
    std::vector<std::vector<double>> averagineTable
        = averagineIsotopeTableGenerator.averagineTable();

    for (size_t i = 0; i < averagineTable.size(); ++i) {
        Eigen::RowVectorXd averagineRowEigen = convertVectorToEigenRowVector(averagineTable[i]);

        if (averagineRowEigen.maxCoeff() == 0) {
            e = emptyCheckReturn(true, "Max Value is zero and"); ree;
        } else {
            averagineRowEigen /= averagineRowEigen.maxCoeff();
            m_averagineTable.push_back(averagineRowEigen);
        }
    }

    e = emptyCheckReturn(m_averagineTable.empty(), "Averagine Table"); ree;

    return e;
}

void FauxScanCreator::addNoiseToScans()
{
//////// For debugging uncomment SET_SEED
//#define SET_SEED
#ifdef SET_SEED
    const int upsideDownNines = 666;
    srand(upsideDownNines);
#endif

    int noisePointsToAdd = 1000;
    int noiseRange = 10;
    for (int i = 0; i < static_cast<int>(m_scans.size()); ++i) {

        point2dList &pointList = m_scans[i].scanIons;
        pointList.reserve(pointList.size() + noisePointsToAdd);

        for (int j = 0; j < noisePointsToAdd; ++j) {
            const double randoMZ = randomNumberInRange(m_parameters.minMz, m_parameters.maxMz);
            const double randoIntensity
                = randomNumberInRange(0, m_parameters.medianNoiseLevelMS1 * noiseRange);
            pointList.push_back(point2d(randoMZ, randoIntensity));
        }

        std::sort(pointList.begin(), pointList.end(), point2d_less_x);
        m_scans[i].scanIons = pointList;
    }
}

Err FauxScanCreator::addAveragineIsotopesToMSnScan(const point2dList &inPoints,
                                                   point2dList *outPoints)
{
    Err e = kNoErr;

    const double averagineIntensityThreshold = 0.1;
    point2dList newPoints;
    
    for (const point2d &point : inPoints) {

        int mwLookUpIndex = std::floor(point.x() / MASS_SPACING);
        e = checkRangeOfQVector(m_averagineTable, mwLookUpIndex); ree;

        Eigen::RowVectorXd averagineRatios = m_averagineTable[mwLookUpIndex];

        averagineRatios
            = (averagineRatios.array() < averagineIntensityThreshold).select(0, averagineRatios);

        for (int i = 0; i < averagineRatios.cols(); ++i) {
            point2d transitionPoint;
            transitionPoint.setX(point.x() + (i * PROTON));
            transitionPoint.setY(point.y() * averagineRatios(i));
            newPoints.push_back(transitionPoint);
        }
    }

    *outPoints = newPoints;
    return e;
}

point2dList FauxScanCreator::filterOutMzRange(const point2dList &inPoints)
{
    point2dList newPoints;

    for (const point2d &point : inPoints) {
        if (point.x() >= m_parameters.minMz && point.x() <= m_parameters.maxMz) {
            newPoints.push_back(point);
        }
    }

    return newPoints;
}

point2dList FauxScanCreator::addChargeStatePlusTwoToScan(const point2dList &inPoints,
                                                         double intensityAttenuation)
{
    point2dList newPoints;

    for (const point2d &point : inPoints) {
        point2d transitionPoint;
        transitionPoint.setX((point.x() + PROTON) / 2);
        transitionPoint.setY(point.y() * intensityAttenuation);
        newPoints.push_back(transitionPoint);
        newPoints.push_back(point);
    }

    return newPoints;
}

point2dList FauxScanCreator::filterOutByIntensity(const point2dList &inPoints,
                                                  double intensityThreshold)
{
    point2dList newPoints;

    for (const point2d &point : inPoints) {
        if (point.y() > intensityThreshold) {
            newPoints.push_back(point);
        }
    }

    return newPoints;
}

Err FauxScanCreator::prettifyMSnScan(const point2dList &inPoints, int precursorCharge,
                                     double intensityAttenuation, point2dList *outPoint)
{
    Err e = kNoErr;

    point2dList fragmentIonsPeptideAveragineAdded;
    e = addAveragineIsotopesToMSnScan(inPoints, &fragmentIonsPeptideAveragineAdded); ree;

    //// This needs to be here because addAveragineIsotopesToMSnScan adds points w/ y = 0;
    //// Since they are useless, we filter them out here.  Could also not put them in during
    ///creation / but this filtering method can come in handy for other filtering purporses.
    fragmentIonsPeptideAveragineAdded = filterOutByIntensity(fragmentIonsPeptideAveragineAdded, 1);

    if (precursorCharge > 1) {
        fragmentIonsPeptideAveragineAdded
            = addChargeStatePlusTwoToScan(fragmentIonsPeptideAveragineAdded);
    }

    fragmentIonsPeptideAveragineAdded = filterOutMzRange(fragmentIonsPeptideAveragineAdded);

    std::sort(fragmentIonsPeptideAveragineAdded.begin(), fragmentIonsPeptideAveragineAdded.end(),
              point2d_less_x);

    *outPoint = fragmentIonsPeptideAveragineAdded;
    return e;
}

QVector<FauxScanCreator::Scan> FauxScanCreator::scans() const
{
    return m_scans;
}

int FauxScanCreator::numberOfScans() const
{
    return static_cast<int>(m_scans.size());
}

_PMI_END