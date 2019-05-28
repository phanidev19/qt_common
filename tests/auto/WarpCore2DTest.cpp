/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Michael Georgoulopoulos mgeorgoulopoulos@gmail.com
 */

#include <PlotBase.h>
#include <TimeWarp.h>
#include <TimeWarp2D.h>
#include <WarpCore2D.h>
#include <pmi_core_defs.h>
#include <warp_core.h>

#include <QtTest>

#include <random>

_PMI_BEGIN

class WarpCore2DTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    /*! \brief Simple test, with just one apex. */
    void testSingleApex();

    /*! 
        \brief Test with random generated plots. /see testRandomData below.
        This test proves that the new warp has identical behaviour with the legacy, when put in legacy mode.
    */
    void testRandomizedLegacy();

    /*!
        \brief Test with random generated plots. /see testRandomData below.
        This test proves that the new warp creates equal or better warps when in 2D mode.
    */
    void testRandomizedNew();
};

/*! \brief Takes a list of intensities per minute and expands to arbitrary sample rate, linearly
 * interpolating between input samples. */
static Err uniformExpand(const std::vector<double> &inputSamples, double sampleRate,
                         std::vector<double> *outputSamples)
{
    Err e = kNoErr;

    outputSamples->clear();

    sampleRate = std::max(1.0, sampleRate);
    double duration = static_cast<double>(inputSamples.size());
    double samplingPeriod = 1.0 / sampleRate;

    for (double t = 0.0; t <= duration; t += samplingPeriod) {
        int leftIndex = qBound(0, qFloor(t), static_cast<int>(inputSamples.size() - 1));
        int rightIndex = qBound(0, qCeil(t), static_cast<int>(inputSamples.size() - 1));
        double lerpFactor = t - static_cast<double>(leftIndex);
        double leftSample = inputSamples[leftIndex];
        double rightSample = inputSamples[rightIndex];
        double sample = leftSample * (1.0 - lerpFactor) + lerpFactor * rightSample;
        outputSamples->push_back(sample);
    }

    return e;
}

/*! \brief Checks if number is between edge1 and edge2 (inclusive). Order independent. */
static bool isBoundBy(double number, double edge1, double edge2)
{
    if (edge2 < edge1) {
        std::swap(edge1, edge2);
    }

    return number >= edge1 && number <= edge2;
}

void WarpCore2DTest::testSingleApex()
{
    // Define 5 minute intervals where all intensities are zero plus one apex
    //      ^
    //     /:\            Signal A (apex x = 2)
    //    / : \                           
    // |0-1-2-3-4-5|
    //        ^
    //       /:\        Signal B (apex x = 3)
    //      / : \                           
    // |0-1-2-3-4-5|
    const int minuteCount = 5;
    const int apexTimeA = 2;
    const int apexTimeB = 3;
    std::vector<double> compactA(minuteCount, 0.0);
    compactA[apexTimeA] = 1.0;
    std::vector<double> compactB(minuteCount, 0.0);
    compactB[apexTimeB] = 1.0;

    // Define sample indices where apexes should match
    const double samplesPerMinute = 50.0;
    int apexIndexA = qRound(apexTimeA * samplesPerMinute);
    int apexIndexB = qRound(apexTimeB * samplesPerMinute);

    // define time knots for the interval (in index space)
    std::vector<int> knotsA;
    for (int m = 0; m <= minuteCount; m++) {
        knotsA.push_back(qRound(static_cast<double>(m) * samplesPerMinute));
    }

    // expand to the given sample rate
    std::vector<double> expandedA;
    QVERIFY(uniformExpand(compactA, samplesPerMinute, &expandedA) == kNoErr);
    std::vector<double> expandedB;
    QVERIFY(uniformExpand(compactB, samplesPerMinute, &expandedB) == kNoErr);

    // convert to WarpElement
    std::vector<WarpElement> warpElementsA;
    for (const double &i : expandedA) {
        warpElementsA.push_back(WarpElement::legacyWarpElement(i));
    }
    std::vector<WarpElement> warpElementsB;
    for (const double &i : expandedB) {
        warpElementsB.push_back(WarpElement::legacyWarpElement(i));
    }

    // Warp
    std::vector<int> knotsB;
    WarpCore2D warpCore2D;
    QVERIFY(warpCore2D.constructWarp(warpElementsA, warpElementsB, knotsA, &knotsB) == kNoErr);

    // we expect to find apex B mapped close to the position of apexA
    const double perfectShift = apexIndexA - apexIndexB;
    const double actualShift = knotsA[apexTimeA] - knotsB[apexTimeA];

    // say we are satisfied with bridging the gap +/- 5%
    const double okDistance = 0.05;
    QVERIFY(isBoundBy(actualShift, perfectShift * (1.0 + okDistance),
                      perfectShift * (1.0 - okDistance)));
}

// Variables for random test
const double sampleRate = 10.0; // samples per minute
const int peptideCount = 50;
const double totalDuration = 60.0;
const double dynamicRange = 1000000.0; // largest intensity divided by smallest intensity.
const int minMz = 100;
const int maxMz = 500;
const double minFeatureDuration = 5.0;
const double maxFeatureDuration = 10.0;
const double minPeptideAppearance = maxFeatureDuration / 2.0;
const double maxPeptideAppearance = totalDuration - maxFeatureDuration / 2.0;
const double mzWindowToExtractMaxima = 20.0;

class RandomMSModel
{
public:
    RandomMSModel(int seed)
        : m_randomEngine(seed)
        , m_intensityDistribution(1.0 / dynamicRange, 1.0)
        , m_mzDistribution(minMz, maxMz)
        , m_apexTimeDistribution(minPeptideAppearance, maxPeptideAppearance)
        , m_featureDurationDistribution(minFeatureDuration, maxFeatureDuration)
    {
        for (int i = 0; i < peptideCount; i++) {
            m_peptides.push_back(randomPeptide());
        }
    }

    /*! Get the dominant mz/intensity pairs at time t. */
    WarpElement sample(double t) const
    {
        // mimick a centroided scan
        point2dList scan;

        for (const Peptide &p : m_peptides) {
            double apexDist = std::abs(t - p.apexTime);
            if (apexDist >= p.featureDuration * 0.5) {
                continue;
            }

            point2d mzInt;
            mzInt.rx() = p.mz;
            mzInt.ry() = p.apexIntensity * cos(M_PI * (2.0 * apexDist / p.featureDuration));
            scan.push_back(mzInt);
        }

        WarpElement ret;
        WarpElement::fromScan(scan, 4, mzWindowToExtractMaxima, &ret);

        return ret;
    }

private:
    double randomMz() { return static_cast<double>(m_mzDistribution(m_randomEngine)); }

    double randomApexTime() { return m_apexTimeDistribution(m_randomEngine); }

    double randomIntensity() { return m_intensityDistribution(m_randomEngine); }

    double randomFeatureDuration() { return m_featureDurationDistribution(m_randomEngine); }

private:
    struct Peptide {
        double apexTime;
        double apexIntensity;
        double mz;
        double featureDuration;
    };

    Peptide randomPeptide()
    {
        Peptide ret;
        ret.apexTime = randomApexTime();
        ret.apexIntensity = randomIntensity();
        ret.mz = randomMz();
        ret.featureDuration = randomFeatureDuration();
        return ret;
    }

private:
    std::default_random_engine m_randomEngine;
    std::uniform_real_distribution<double> m_intensityDistribution;
    std::uniform_int_distribution<int> m_mzDistribution;
    std::uniform_real_distribution<double> m_apexTimeDistribution;
    std::uniform_real_distribution<double> m_featureDurationDistribution;

    QVector<Peptide> m_peptides;
};

struct RandomMSSampler {
    double signalMultiplier;
    double noiseMultiplier;
    std::default_random_engine noiseEngine;
    std::uniform_real_distribution<double> noiseDistribution;
    TimeWarp timeDistortion;
    int positionalNoiseSeed;

    RandomMSSampler(int seed, double signal, double noise)
        : signalMultiplier(signal)
        , noiseMultiplier(noise)
        , noiseEngine(seed)
        , noiseDistribution(0.0, 1.0)
        , positionalNoiseSeed(seed)
    {
        
    }

    WarpElement sample(const RandomMSModel &model, double t)
    {
        // apply time distortion
        t = timeDistortion.warp(t);

        WarpElement ret = model.sample(t);
        ret.scale(signalMultiplier);

        // add noise
        for (int i = 0; i < WarpElement::maxMzIntensityPairCount; i++) {
            // positional noise, so that we get the same noise when requesting the same t / i
            int currentSeed
                = qRound(t * sampleRate) * WarpElement::maxMzIntensityPairCount + i
                + positionalNoiseSeed;
            noiseEngine.seed(currentSeed);

            MzIntensityPair &mzInt = ret.mzIntensityPairs[i];
            double noise = noiseDistribution(noiseEngine);
            mzInt.intensity += noiseMultiplier * noise;
        }

        return ret;
    }
};

/*! \brief Randomly modifies knots A and returns the result. */
static std::vector<double> randomizeWarp(const std::vector<double> &knotsA, int seed)
{
    const double maximumDeviation = 5.0;
    std::default_random_engine randomEngine(seed);
    std::uniform_real_distribution<double> deviationDistribution(-maximumDeviation,
                                                                 maximumDeviation);

    std::vector<double> knotsB = knotsA;
    double deviation = 0.0;
    for (double &t : knotsB) {
        t += deviation;
        deviation += deviationDistribution(randomEngine);
    }

    return knotsB;
}

/*!
   \brief Runs a randomized test using \a seed.
   \param [out] sqDistLegacy Summed square distance of the knots produced by legacy to the
        optimal knots.
   \param [out] sqDist2D Summed square distance of the knots produced by legacy to
        the optimal knots.
*/
static Err testRandomData(int seed, bool legacyMode, double *sqDistLegacy, double *sqDist2D)
{
    Err e = kNoErr;

    *sqDistLegacy = 0.0;
    *sqDist2D = 0.0;

    std::default_random_engine randomEngine(seed);

    // create a random model
    RandomMSModel model(randomEngine());

    // Create knots 10 minutes apart
    const double knotDuration = 10.0;
    std::vector<double> knotsA;
    for (double t = 0.0; t < totalDuration; t += knotDuration) {
        knotsA.push_back(t);
    }
    knotsA.push_back(totalDuration);

    // convert to index space
    std::vector<int> knotsAIndexSpace;
    for (const double &t : knotsA) {
        knotsAIndexSpace.push_back(qRound(t * sampleRate));
    }

    // Create a distortion time warp by randomizing knotsA
    std::vector<double> knotsB = randomizeWarp(knotsA, randomEngine());

    // Create two samplers of the same model, with different signal/noise ratios.
    // First sampler will get a null time distortion.
    RandomMSSampler samplerA(randomEngine(), 1000.0, 200.0);

    // Second sampler will get a time distortion
    RandomMSSampler samplerB(randomEngine(), 500.0, 100.0);
    samplerB.timeDistortion.setAnchors(knotsA, knotsB);

    // produce plots to align
    double maxDuration = std::max(knotsA.back(), knotsB.back());
    std::vector<double> bpiA;
    std::vector<double> bpiB;
    std::vector<WarpElement> warpElementsA;
    std::vector<WarpElement> warpElementsB;
    for (double t = 0.0; t <= maxDuration; t += 1.0 / sampleRate) {
        WarpElement a = samplerA.sample(model, t);
        WarpElement b = samplerB.sample(model, t);

        if (legacyMode) {
            // set a and b to be legacy
            for (int i = 0; i < WarpElement::maxMzIntensityPairCount; i++) {
                a.mzIntensityPairs[i].mz = 0.0;
                b.mzIntensityPairs[i].mz = 0.0;
            }
        }

        bpiA.push_back(a.mzIntensityPairs[0].intensity);
        bpiB.push_back(b.mzIntensityPairs[0].intensity);
        warpElementsA.push_back(a);
        warpElementsB.push_back(b);
    }

    // Common options for both legacy and 2D warp
    const double penalty = 0.0;
    const int globalSkew = 500;

    // do legacy time warp
    std::vector<int> knotsBLegacyIndexSpace;
    warp_core(penalty, globalSkew, bpiA, bpiB, knotsAIndexSpace, knotsBLegacyIndexSpace);

    // do 2D time warp
    std::vector<int> knotsB2DIndexSpace;
    WarpCore2D warpCore2D;
    warpCore2D.setStretchPenalty(penalty);
    warpCore2D.setGlobalSkew(globalSkew);
    warpCore2D.setMzMatchPpm(100.0);
    warpCore2D.constructWarp(warpElementsA, warpElementsB, knotsAIndexSpace, &knotsB2DIndexSpace);

    // convert back from index space
    std::vector<double> knotsBLegacy;
    for (const int i : knotsBLegacyIndexSpace) {
        knotsBLegacy.push_back(static_cast<double>(i) / sampleRate);
    }
    std::vector<double> knotsB2D;
    for (const int i : knotsB2DIndexSpace) {
        knotsB2D.push_back(static_cast<double>(i) / sampleRate);
    }

    for (int i = 0; i < static_cast<int>(knotsA.size()); i++) {
        double distLegacy = knotsBLegacy[i] - knotsB[i];
        *sqDistLegacy += distLegacy * distLegacy;
        double dist2D = knotsB2D[i] - knotsB[i];
        *sqDist2D += dist2D * dist2D;
    }

/*
    // Code to produce graphical plots of the alignments. Uncomment to use.
    TimeWarp timeWarpLegacy;
    timeWarpLegacy.setAnchors(knotsA, knotsBLegacy);
    TimeWarp timeWarp2D;
    timeWarp2D.setAnchors(knotsA, knotsB2D);
    FILE *fp = fopen(QString("RandomizedWarpTest%1.txt").arg(seed).toStdString().c_str(), "w");
    for (double t = 0.0; t <= maxDuration; t += 1.0 / sampleRate) {
        WarpElement a = samplerA.sample(model, t);
        WarpElement bLegacy = samplerB.sample(model, timeWarpLegacy.unwarp(t));
        WarpElement b2D = samplerB.sample(model, timeWarp2D.unwarp(t));
        
        fprintf(fp, "%f\t%f\t%f\t%f\n", t, a.mzIntensityPairs[0].intensity,
                bLegacy.mzIntensityPairs[0].intensity, b2D.mzIntensityPairs[0].intensity);
    }
    fclose(fp);
*/

    return e;
}

void WarpCore2DTest::testRandomizedLegacy()
{
    const bool legacyMode = true;
    for (int i = 0; i < 10; i++) {
        double sqDistLegacy = 0.0;
        qInfo() << "Trying " << i << "\n";
        double sqDist2D = 0.0;
        QVERIFY(testRandomData(i, legacyMode, &sqDistLegacy, &sqDist2D) == kNoErr);
        QVERIFY(sqDistLegacy >= sqDist2D);
        qInfo() << "SqDist from optimal:\n";
            qInfo() << "Legacy: " << sqDistLegacy << ", 2D: " << sqDist2D << "\n";
    }
}

void WarpCore2DTest::testRandomizedNew()
{
    const bool legacyMode = false;
    for (int i = 0; i < 10; i++) {
        double sqDistLegacy = 0.0;
        qInfo() << "Trying " << i << "\n";
        double sqDist2D = 0.0;
        QVERIFY(testRandomData(i, legacyMode, &sqDistLegacy, &sqDist2D) == kNoErr);
        QVERIFY(sqDistLegacy > sqDist2D);
        qInfo() << "SqDist from optimal:\n";
        qInfo() << "Legacy: " << sqDistLegacy << ", 2D: " << sqDist2D << "\n";
    }
}

_PMI_END

QTEST_MAIN(pmi::WarpCore2DTest)

#include "WarpCore2DTest.moc"
