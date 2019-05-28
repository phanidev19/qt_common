/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include <QtTest>

#include <CsvReader.h>
#include <CsvWriter.h>
#include <MSReader.h>
#include <PMiTestUtils.h>

// In order to use vendor data files from drive P please uncomment this line
//#define PMI_ENABLE_P_DRIVE_TESTS

// In order to test data generation please uncomment this line. Normally you would not need to
// enable it. If you do then please ensure you know what you are doing.
//#define PMI_GENERATE_TEST_DATA

_PMI_BEGIN

class MSReaderChromatogramsTest : public QObject
{
    Q_OBJECT
public:
    explicit MSReaderChromatogramsTest(const QStringList &args);

#ifdef PMI_GENERATE_TEST_DATA
private Q_SLOTS:
#else
private:
#endif
    void generateChromatogramsList_data();
    void generateChromatogramsList();

    void deleteHashStampFiles(const QString &folderPath);

private Q_SLOTS:
    void testChromatogramsList_data();
    void testChromatogramsList(bool generateData = false);

private:
    MSReader *initReader();
    void cleanUpReader(MSReader *ms);

    QDir m_testDataBasePath;
    QString m_testFile;
};

MSReaderChromatogramsTest::MSReaderChromatogramsTest(const QStringList &args)
    : m_testDataBasePath(args[0])
{
}

void MSReaderChromatogramsTest::generateChromatogramsList_data()
{
    testChromatogramsList_data();
}

void MSReaderChromatogramsTest::generateChromatogramsList()
{
    testChromatogramsList(true);
}

void MSReaderChromatogramsTest::deleteHashStampFiles(const QString &folderPath)
{
    QFileInfo folderInfo(folderPath);
    if (!folderInfo.isDir()) {
        return;
    }

    QStringList filesToDelete;

    QDirIterator it(folderPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filename = it.next();
        if (filename.endsWith(QStringLiteral("-hash-stamp"))) {
            filesToDelete << filename;
        }
    }

    for (const QString filename : filesToDelete) {
        QFile::remove(filename);
    }
}

void MSReaderChromatogramsTest::testChromatogramsList_data()
{
    QTest::addColumn<QString>("dataPath");
    QTest::addColumn<bool>("needDeleteHashStamp");

    // dummy case is created especially for case when all subsequent cases are disables.
    QTest::addRow("dummy") << "";

#ifdef PMI_MS_ENABLE_ABI_DISK_TO_MEMORY
    QTest::addRow("Abi.OriginalMAb")
        << m_testDataBasePath.filePath("Abi/OriginalMAb/OriginalMAb.wiff") << false;
#endif

#ifdef PMI_MS_ENABLE_SHIMADZU_API
    QTest::addRow("Shimadzu.BSA_Digest_1pmol-uL_DDA_003")
        << m_testDataBasePath.filePath("Shimadzu/BSA_Digest_1pmol-uL_DDA_003.lcd") << false;
#endif

#ifdef PMI_MS_ENABLE_BRUKER_API
    QTest::addRow("Bruker.Adalimumab_2_248_RA3_01_1857.d")
        << m_testDataBasePath.filePath("Bruker/Adalimumab_2_248_RA3_01_1857.d") << true;

#ifdef PMI_ENABLE_P_DRIVE_TESTS
    QTest::addRow("Bruker.ARS_506")
        << "P:\\PMI-Dev\\Managed\\Data\\VendorData\\Bruker\\WithUV\\ARS_506.d" << false;
    QTest::addRow("Bruker.IgG_alone_RE2_01_1926")
        << "P:\\PMI-Dev\\Managed\\Data\\VendorData\\Bruker\\DDA\\IgG_alone_RE2_01_1926.d" << false;
    QTest::addRow("Bruker.IgG_UPS2_RE3_01_1932")
        << "P:\\PMI-Dev\\Managed\\Data\\VendorData\\Bruker\\DDA\\IgG_UPS2_RE3_01_1932.d" << false;
#endif
#endif
}

void MSReaderChromatogramsTest::testChromatogramsList(bool generateData)
{
    if (QTest::currentDataTag() == QStringLiteral("dummy")) {
        return;
    }
    QFETCH(QString, dataPath);
    QFETCH(bool, needDeleteHashStamp);

    QVERIFY(QFileInfo::exists(dataPath));

    if (needDeleteHashStamp) {
        deleteHashStampFiles(dataPath);
    }

    MSReader *ms = MSReader::Instance();
    Err e = ms->openFile(dataPath);
    QCOMPARE(e, kNoErr);

    QList<msreader::ChromatogramInfo> actualChromatogramsList;
    ms->getChromatograms(&actualChromatogramsList);

    const QString expectedDataFileName
        = QStringLiteral(PMI_TEST_FILES_DATA_DIR "/MSReaderChromatogramsTest/")
        + QTest::currentDataTag() + QStringLiteral(".chromatograms.csv");
    const QString expectedPointsFileNameTemplate = QStringLiteral("expected/")
        + QTest::currentDataTag() + QStringLiteral(".chromatogram.%0.data");
    if (generateData) {
        QFileInfo(expectedDataFileName).dir().mkpath(".");
        CsvWriter writer(expectedDataFileName, "\r\n", '\t');
        writer.open();
        int i = 0;
        for (const auto chromatogram : actualChromatogramsList) {
            ++i;
            const QString pointsFileName = expectedPointsFileNameTemplate.arg(i);
            QFileInfo(m_testDataBasePath.filePath(pointsFileName)).dir().mkpath(".");
            const QStringList row
                = { chromatogram.internalChannelName, QString::number(chromatogram.points.size()),
                    pointsFileName, chromatogram.channelName };
            writer.writeRow(row);

            QFile dumpFile(m_testDataBasePath.filePath(pointsFileName));
            QVERIFY(dumpFile.open(QIODevice::WriteOnly));
            QDataStream fileStream(&dumpFile);
            for (auto point : chromatogram.points) {
                fileStream << point.x() << point.y();
            }
        }
    } else {
        QMap<QString, QString> expectedPointsNo;
        QMap<QString, point2dList> expectedPoints;
        QMap<QString, QString> expectedNames;
        // reader and expectedChromatogramsList variables scope
        {
            CsvReader reader(expectedDataFileName);
            reader.setFieldSeparatorChar('\t');
            reader.open();
            QList<QStringList> expectedChromatogramsList;
            reader.readAllRows(&expectedChromatogramsList);

            for (const auto chromatogram : expectedChromatogramsList) {
                QCOMPARE(chromatogram.size(), 4);
                QVERIFY(!expectedPointsNo.contains(chromatogram[0]));

                expectedPointsNo[chromatogram[0]] = chromatogram[1];
                expectedNames[chromatogram[0]] = chromatogram[3];

                QFile dumpFile(m_testDataBasePath.filePath(chromatogram[2]));
                QVERIFY(dumpFile.open(QIODevice::ReadOnly));
                QDataStream fileStream(&dumpFile);

                point2dList dumpContents;
                for (;;) {
                    double first = 0;
                    double second = 0;
                    fileStream.startTransaction();
                    fileStream >> first;
                    fileStream >> second;
                    if (fileStream.commitTransaction()) {
                        dumpContents.push_back(QPointF(first, second));
                    } else {
                        break;
                    }
                }
                expectedPoints[chromatogram[0]] = dumpContents;
            }
            QCOMPARE(expectedPointsNo.size(), expectedChromatogramsList.size());
            QCOMPARE(expectedNames.size(), expectedChromatogramsList.size());
        }

        QMap<QString, QString> actualPointsNo;
        QMap<QString, point2dList> actualPoints;
        QMap<QString, QString> actualNames;
        for (const auto chromatogram : actualChromatogramsList) {
            // check unique
            QVERIFY(!actualPointsNo.contains(chromatogram.internalChannelName));

            actualPointsNo[chromatogram.internalChannelName]
                = QString::number(chromatogram.points.size());
            actualPoints[chromatogram.internalChannelName] = chromatogram.points;
            actualNames[chromatogram.internalChannelName] = chromatogram.channelName;
        }
        QCOMPARE(actualPointsNo.size(), actualChromatogramsList.size());
        QCOMPARE(actualNames.size(), actualChromatogramsList.size());

        QCOMPARE(actualPointsNo, expectedPointsNo);
        QCOMPARE(actualPoints, expectedPoints);
        if (actualNames != expectedNames) {
            qDebug() << "Notice: actual names and expected names are not equal.";

            QMap<QString, QPair<QString, QString>> compareTable;
            for (auto it = expectedNames.cbegin(); it != expectedNames.cend(); ++it) {
                compareTable[it.key()].first = it.value();
            }
            for (auto it = actualNames.cbegin(); it != actualNames.cend(); ++it) {
                compareTable[it.key()].second = it.value();
            }

            for (auto it = compareTable.cbegin(); it != compareTable.cend(); ++it) {
                qDebug() << "id:       " << it.key();
                qDebug() << "expected: " << it.value().first;
                qDebug() << "actual:   " << it.value().second;
                qDebug();
            }
        }
    }

    ms->closeFile();
}

void MSReaderChromatogramsTest::cleanUpReader(MSReader *ms)
{
    ms->releaseInstance();
    ms = nullptr;
}

MSReader *pmi::MSReaderChromatogramsTest::initReader()
{
    return MSReader::Instance();
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSReaderChromatogramsTest, QStringList() << "Remote data path")

#include "MSReaderChromatogramsTest.moc"
