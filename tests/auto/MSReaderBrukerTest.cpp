/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include <MSReaderBruker.h>
#include <MSReader.h>
#include <PMiTestUtils.h>
#include <qt_string_utils.h>
#include <VendorPathChecker.h>

using namespace pmi;
using namespace pmi::msreader;

class MSReaderBrukerTest : public QObject
{
    Q_OBJECT
public:
    //! @a path points to the .d data dir
    explicit MSReaderBrukerTest(const QStringList &args)
        : QObject(), m_path(args[0]), m_isBrukerAnalysisBaf(args[1] == "1")
    {
    }

    ~MSReaderBrukerTest() {}

private Q_SLOTS:
    void initTestCase();
    void testIsBrukerPath();
    void testAdjustBrukerFilenPathToFolderPath();
    void testOpenDirect();
    void testChromatograms();
    void testOpenViaMSReader();
    void cleanupTestCase();
    void testMSReaderInstance();

private:
    QString m_path;
    bool m_isBrukerAnalysisBaf;
};

void MSReaderBrukerTest::initTestCase()
{
}

void MSReaderBrukerTest::testMSReaderInstance()
{
    MSReader * ms = MSReader::Instance();
    QVERIFY(ms);
}

void MSReaderBrukerTest::cleanupTestCase()
{
}

void MSReaderBrukerTest::testIsBrukerPath()
{
    QVERIFY(VendorPathChecker::isBrukerPath("foo.d"));
    QVERIFY(!VendorPathChecker::isBrukerPath("foo.somethingelse"));
    QVERIFY(VendorPathChecker::isAgilentPath("foo.d"));
    QVERIFY(!VendorPathChecker::isAgilentPath("foo.bar"));
}

void MSReaderBrukerTest::testAdjustBrukerFilenPathToFolderPath()
{
    bool changed;
    QCOMPARE(adjustBrukerFilenPathToFolderPath("D:\\foo\\Pfizer_NIST_Pepmap.d\\analysis.baf", &changed),
             "D:/foo/Pfizer_NIST_Pepmap.d");
    QVERIFY(changed);
    QCOMPARE(adjustBrukerFilenPathToFolderPath("D:\\foo\\Pfizer_NIST_Pepmap.d/analysis.baf", &changed),
             "D:/foo/Pfizer_NIST_Pepmap.d");
    QVERIFY(changed);
    QCOMPARE(adjustBrukerFilenPathToFolderPath("D:\\foo\\Pfizer_NIST_Pepmap.d\\analysis.not-baf", &changed),
         "D:\\foo\\Pfizer_NIST_Pepmap.d\\analysis.not-baf");
    QVERIFY(!changed);
    QCOMPARE(adjustBrukerFilenPathToFolderPath("D:\\foo\\Pfizer_NIST_Pepmap.not-d\\analysis.baf", &changed),
         "D:\\foo\\Pfizer_NIST_Pepmap.not-d\\analysis.baf");
    QVERIFY(!changed);
}

void MSReaderBrukerTest::testOpenDirect()
{
    QFileInfo fi(m_path);
    QVERIFY(fi.isDir() && fi.exists());

    MSReaderBruker bruker;
    QCOMPARE(bruker.classTypeName(), MSReaderBase::MSReaderClassTypeBruker);
    QVERIFY(bruker.canOpen(m_path));
    QCOMPARE(bruker.openFile(m_path), kNoErr);
    QVERIFY(!bruker.getFilename().isEmpty());
    QCOMPARE(VendorPathChecker::isBrukerAnalysisBaf(m_path), m_isBrukerAnalysisBaf);
    QCOMPARE(bruker.closeFile(), kNoErr);
}

void MSReaderBrukerTest::testOpenViaMSReader()
{
    QFileInfo fi(m_path);
    QVERIFY(fi.isDir() && fi.exists());

    MSReader * ms = MSReader::Instance();
    QVERIFY(ms);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
    QVERIFY(ms->getFilename().isEmpty());
    QCOMPARE(VendorPathChecker::isBrukerAnalysisBaf(m_path), m_isBrukerAnalysisBaf);
    qDebug() << "Opening path:" << m_path;

    QCOMPARE(ms->openFile(m_path), kNoErr);
    QVERIFY(ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeBruker);
    QVERIFY(!ms->getFilename().isEmpty());

    point2dList points, points2;
    QCOMPARE(ms->getTICData(&points), kNoErr);
    qDebug() << "points size=" << points.size();
    long totalNumber = 0, startScan=0, endScan = 0;
    QCOMPARE(ms->getNumberOfSpectra(&totalNumber, &startScan, &endScan), kNoErr);
    qDebug() << "total,start,end=" << totalNumber << "," << startScan << "," << endScan;

    for (long scan = startScan; scan <= endScan; scan++) {
        ScanInfo obj;
        QString fragType;
        PrecursorInfo pinfo;
        QCOMPARE(ms->getScanInfo(scan, &obj), kNoErr);
        // qDebug() << "scan=" << scan << "obj=" << obj;
        QCOMPARE(ms->getScanData(scan, &points, false), kNoErr);
        QCOMPARE(ms->getScanData(scan, &points2, true), kNoErr);
        qDebug() << scan << ",points,point_cent size=" << points.size() << "," << points2.size();
        QCOMPARE(ms->getFragmentType(scan, -1, &fragType), kNoErr);
//        qDebug() << "fragType=" << fragType;
        if (obj.scanLevel > 1) {
            Err e = ms->getScanPrecursorInfo(scan, &pinfo);
            if (e == kNoErr) {
                qDebug() << "pinfo=" << pinfo.nativeId << "," << pinfo.nScanNumber << "," << pinfo.dMonoIsoMass;
            }
        }
    }
    QCOMPARE(ms->closeFile(), kNoErr);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
}

void MSReaderBrukerTest::testChromatograms()
{
    const QFileInfo fi(m_path);
    QVERIFY(fi.isDir() && fi.exists());

    MSReaderBruker bruker;
    QCOMPARE(bruker.openFile(m_path), kNoErr);

    QList<ChromatogramInfo> chroList;

    // id=0,instId=,inst=,instDescr=,t=3,unit=7,offset=0
    // id=1,instId=,inst=,instDescr=,t=1,unit=7,offset=0
    // id=2,instId=,inst=,instDescr=LC Pump 1 Pressure,t=4,unit=3,offset=0
    // id=3,instId=,inst=,instDescr=Flow,t=6,unit=2,offset=0
    // id=4,instId=,inst=,instDescr=Solvent A,t=5,unit=4,offset=0
    // id=5,instId=,inst=,instDescr=Solvent B,t=5,unit=4,offset=0
    // id=6,instId=,inst=,instDescr=Solvent C,t=5,unit=4,offset=0
    // id=7,instId=,inst=,instDescr=Solvent D,t=5,unit=4,offset=0

    bruker.getChromatograms(&chroList);
    if (chroList.isEmpty()) {
        return;
    }

    QList<ChromatogramInfo> uvChroList;
    bruker.getChromatograms(&uvChroList, "id=0");
    QCOMPARE(uvChroList.size(), 1);
}

PMI_TEST_GUILESS_MAIN_WITH_ARGS(MSReaderBrukerTest, QStringList() << "path" << "isBrukerAnalysisBaf[0|1]")

#include "MSReaderBrukerTest.moc"
