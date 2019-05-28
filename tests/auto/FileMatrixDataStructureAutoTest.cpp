/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "FauxSpectraReader.h"
#include "FauxScanCreator.h"
#include "FileMatrixDataStructure.h"

#include <pmi_core_defs.h>

#include <QtTest>

_PMI_BEGIN

class FileMatrixDataStructureAutoTest : public QObject
{
    Q_OBJECT
    QVector<FauxSpectraReader::FauxPeptideRow> m_fauxFileDataVector;
    QVector<FauxScanCreator::Scan> m_scansVec;


private Q_SLOTS:
    ////Builds an MSFaux file and stores it directly into memory in m_ScansVec n = 100 peptides
    void generateMSFauxFile();

    /*!
    * \brief Builds FileMatrixDataStructure and compares against expected input.
    *
    * m_scansVec is used to build FileMatrixDataStructure.  Then using m_scansVec
    * XIC extraction occurs and the max intensity extracted is compared against
    * the expected max intensity that was used to generate msfaux data.
    */
    void testXICExtractionTest();
};

inline double fRand(double fMin, double fMax)
{
    double f = static_cast<double>(rand() / static_cast<double>(RAND_MAX));
    return fMin + f * (fMax - fMin);
}

void FileMatrixDataStructureAutoTest::generateMSFauxFile() {
    srand(666);
    int numberOfPeptidesToGenerate = 100;
    for (int i = 0; i < numberOfPeptidesToGenerate; ++i) {
        FauxSpectraReader::FauxPeptideRow peptide;
        //// A limit of 1700 Da is used because max isotope switches over at around 1800.
        //// Going above 1800 would require building averagine table to determine max isotope.
        peptide.massObject = QString::number(fRand(200, 1700));
        peptide.retTimeMinutes = fRand(1, 20);
        peptide.intensity = fRand(1000000, 50000000);
        peptide.peakWidthMinutes = 0.3;
        peptide.chargeOrderAndRatios;
        peptide.chargeOrderAndRatios.insert(2, 1);
        peptide.chargeOrderAndRatios.insert(1, 0.5);
        m_fauxFileDataVector.push_back(peptide);
    }

    FauxSpectraReader fauxSpectraReader;
    FauxSpectraReader::FauxScanCreatorParameters parameters;
    parameters.minMz = 0;
    parameters.maxMz = 1802;
    FauxScanCreator fauxScanCreator;

    fauxSpectraReader.setFauxPeptidesToCreate(m_fauxFileDataVector);
    fauxScanCreator.init(fauxSpectraReader, parameters);
    fauxScanCreator.constructMS1Scans();


    m_scansVec = fauxScanCreator.scans();
}

void FileMatrixDataStructureAutoTest::testXICExtractionTest() {
    
    FileMatrixDataStructure dataFile(m_scansVec);

    QElapsedTimer et;
    et.start();
    for (const FauxSpectraReader::FauxPeptideRow &peptide : m_fauxFileDataVector) {
        double xicStart = peptide.retTimeMinutes - (peptide.peakWidthMinutes / 2.0);
        double xicEnd = peptide.retTimeMinutes + (peptide.peakWidthMinutes / 2.0);

        for (int charge : peptide.chargeOrderAndRatios.keys()) {
            double mz = (peptide.massObject.toDouble() + (charge * PROTON)) / static_cast<double>(charge);
            double ppm = 10;
            double tolerance = (mz * ppm) / 1000000;
            RetrieveSegement seg = dataFile.retrieveSegementFromDataFileMatrixErrorRange(mz, tolerance, xicStart, xicEnd);

            QCOMPARE(seg.xic.maxCoeff() , peptide.intensity * (charge / 2.0));
        }
    }
    qDebug() << "Total Time for XIC retrieval" << et.elapsed() << "MilliSeconds for" << m_fauxFileDataVector.size() * 2 << "XICs";
}

_PMI_END

QTEST_MAIN(pmi::FileMatrixDataStructureAutoTest)
#include "FileMatrixDataStructureAutoTest.moc"
