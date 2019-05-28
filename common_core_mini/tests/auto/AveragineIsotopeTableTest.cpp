/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include <QtTest>
#include "pmi_core_defs.h"

#include "AveragineIsotopeTable.h"

_PMI_BEGIN

const QDir TEST_OUTPUT_DIR = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/AveragineIsotopeTableTest/");

using namespace coreMini;

class AveragineIsotopeTableTest : public QObject
{
    Q_OBJECT
public:
    AveragineIsotopeTableTest() {
        TEST_OUTPUT_DIR.mkdir(TEST_OUTPUT_DIR.absolutePath());
    }
private Q_SLOTS:
    void testIsotopeIntensityFractions();
};

void AveragineIsotopeTableTest::testIsotopeIntensityFractions()
{
    AveragineIsotopeTable table(AveragineIsotopeTable::Parameters::makeDefaultForByomapFeatureFinder());
    
    QString dstFileName = TEST_OUTPUT_DIR.filePath("averagine.csv");
    qDebug() << "Storing averagine to" << dstFileName;
    table.saveToFile(dstFileName);
    
    const std::vector<double> &isotope2400 = table.isotopeIntensityFractions(2400);
    QCOMPARE(isotope2400.size(), size_t(7));
}

_PMI_END

QTEST_MAIN(pmi::AveragineIsotopeTableTest)

#include "AveragineIsotopeTableTest.moc"
