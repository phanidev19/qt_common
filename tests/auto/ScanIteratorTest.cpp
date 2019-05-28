/*
* Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "CrossSampleFeatureCollatorTurbo.h"

#include "FauxSpectraReader.h"  
#include "FauxScanCreator.h"
#include "FeatureFinderParameters.h"
#include "ScanIterator.h"

#include <PMiTestUtils.h>
#include <pmi_core_defs.h>

#include <QtTest>

_PMI_BEGIN

class ScanIteratorTest : public QObject
{
    Q_OBJECT
public:
    ScanIteratorTest() {}

private Q_SLOTS:
    void buildScansVec();
    void runScanIterator();
    void falseNegativeCheck();
    void falsePositiveCheck();
    void summaryCheck();

private:
    QVector<FauxScanCreator::Scan> m_scansVec;
    QVector<CrossSampleFeatureTurbo> m_crossSampleFeatureTurbos;
    QVector<FauxSpectraReader::FauxPeptideRow> m_fauxPeptideRows;
    double m_mwTolerance = 0.005;
    double m_rtTolerance = 0.1;
    int m_numberOfFeaturesFound = 0;
    int m_numberOfFalsePositives = 0;
    int m_numberOfFalseNegatives = 0;
    int m_numberOfPeptidesToCreate = 100;
};

void ScanIteratorTest::buildScansVec() {

    Err e = kNoErr;
    FauxSpectraReader fauxScanReader;
    
    double maxRtTime = 5;
    int seedInvertedNines = 666;
    m_fauxPeptideRows = fauxScanReader.buildRandomPeptideDataMS1Only(m_numberOfPeptidesToCreate,
                                                                     maxRtTime, seedInvertedNines);

    fauxScanReader.setFauxPeptidesToCreate(m_fauxPeptideRows);

    FauxScanCreator fauxScanCreator;
    e = fauxScanCreator.init(fauxScanReader, fauxScanReader.parameters());
    e = fauxScanCreator.constructMS1Scans();

    m_scansVec = fauxScanCreator.scans();
    QCOMPARE(kNoErr, e);
}

void ScanIteratorTest::runScanIterator() {

    Err e = kNoErr;

    SettableFeatureFinderParameters ffUserParams;
    ffUserParams.minPeakWidthMinutes = 0.0;

    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");

    QVector<CrossSampleFeatureTurbo> crossSampleFeatureTurbos;
    ScanIterator scanIterator(neuralNetworkDbFilePath, ffUserParams);
    scanIterator.init();
    scanIterator.iterateMSFileLinearDBSCANSelectTestPurposes(m_scansVec, &crossSampleFeatureTurbos);
    
    m_crossSampleFeatureTurbos = crossSampleFeatureTurbos;
    QCOMPARE(crossSampleFeatureTurbos.size(), 102);
    QCOMPARE(e, kNoErr);
    m_numberOfFeaturesFound = crossSampleFeatureTurbos.size();
}

void ScanIteratorTest::falseNegativeCheck(){
  
    for (const FauxSpectraReader::FauxPeptideRow & fauxPeptideRow : m_fauxPeptideRows) {

        QString mwToSearch = fauxPeptideRow.massObject;
        double rtApexToSearch = fauxPeptideRow.retTimeMinutes;

        CrossSampleFeatureTurbo foundFeature;
        for (const CrossSampleFeatureTurbo & crossSampleFeatureTurbo: m_crossSampleFeatureTurbos) {
            double mwToFind = crossSampleFeatureTurbo.mwMonoisotopic;
            double rtApexToFind = crossSampleFeatureTurbo.rt;

            if (std::abs(mwToFind - mwToSearch.toDouble()) < m_mwTolerance  
                && std::abs(rtApexToFind - rtApexToSearch) < m_rtTolerance) {
                foundFeature = crossSampleFeatureTurbo;
                break;
            }
        }

        if (foundFeature.mwMonoisotopic == -1) {
            m_numberOfFalseNegatives++;
        }
        else {
            foundFeature.clear();
        }
    }
    qDebug() << "FALSE NEGATIVES" << m_numberOfFalseNegatives;
    QCOMPARE(0, m_numberOfFalseNegatives);
}

void ScanIteratorTest::falsePositiveCheck() {

    for (const CrossSampleFeatureTurbo & crossSampleFeatureTurbo : m_crossSampleFeatureTurbos) {
    
        double mwToSearch = crossSampleFeatureTurbo.mwMonoisotopic;
        double rtApexToSearch = crossSampleFeatureTurbo.rt;
    
        FauxSpectraReader::FauxPeptideRow foundFauxPeptideRow;
        for (const FauxSpectraReader::FauxPeptideRow & fauxPeptideRow : m_fauxPeptideRows) {
            
            QString mwToFind = fauxPeptideRow.massObject;
            double rtApexToFind = fauxPeptideRow.retTimeMinutes;

            if (std::abs(mwToFind.toDouble() - mwToSearch) < m_mwTolerance  
                && std::abs(rtApexToFind - rtApexToSearch) < m_rtTolerance) {
                foundFauxPeptideRow = fauxPeptideRow;
                break;
            }
        }
        
        if (foundFauxPeptideRow.massObject.isEmpty()) {
            m_numberOfFalsePositives++;
        }
        else {
            foundFauxPeptideRow.clear();
        }   
    }
    qDebug() << "FALSE POSITIVES" << m_numberOfFalsePositives;
    QCOMPARE(2, m_numberOfFalsePositives);
}

void ScanIteratorTest::summaryCheck(){
    QCOMPARE(m_numberOfFeaturesFound, ((m_numberOfPeptidesToCreate - m_numberOfFalseNegatives) + m_numberOfFalsePositives));
}

_PMI_END

QTEST_MAIN(pmi::ScanIteratorTest)

#include "ScanIteratorTest.moc"
