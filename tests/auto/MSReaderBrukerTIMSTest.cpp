/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
*/

#include <MSReaderBrukerTims.h>
#include <MSReader.h>
#include <PMiTestUtils.h>
#include <qt_string_utils.h>
#include <VendorPathChecker.h>

using namespace pmi;
using namespace pmi::msreader;

class MSReaderBrukerTIMSTest : public QObject
{
    Q_OBJECT
public:
    //! @a path points to the .d data dir
    explicit MSReaderBrukerTIMSTest(const QStringList &args)
        : QObject(), m_path(args[0]), m_isBrukerAnalysisBaf(args[1] == "1")
    {
    }

    ~MSReaderBrukerTIMSTest() {}

private Q_SLOTS:
    void initTestCase();
    void testIsBrukerPath();
    void testAdjustBrukerFilenPathToFolderPath();
    void testOpenDirect();
    void testChromatograms();
    void testOpenViaMSReader();
    void testMSReaderInstance();
    void cleanupTestCase();

private:
    QString m_path;
    bool m_isBrukerAnalysisBaf;
};

void MSReaderBrukerTIMSTest::initTestCase()
{
}

void MSReaderBrukerTIMSTest::testMSReaderInstance()
{
    MSReader* ms = MSReader::Instance();
    QVERIFY(ms);
}

void MSReaderBrukerTIMSTest::cleanupTestCase()
{
}

void MSReaderBrukerTIMSTest::testIsBrukerPath()
{
    QVERIFY(VendorPathChecker::isBrukerPath("foo.d"));
    QVERIFY(!VendorPathChecker::isBrukerPath("foo.somethingelse"));
    QVERIFY(VendorPathChecker::isAgilentPath("foo.d"));
    QVERIFY(!VendorPathChecker::isAgilentPath("foo.bar"));
}

void MSReaderBrukerTIMSTest::testAdjustBrukerFilenPathToFolderPath()
{
    bool changed = true;

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

void MSReaderBrukerTIMSTest::testOpenDirect()
{
    QFileInfo fi(m_path);
    QVERIFY(fi.isDir() && fi.exists());

    MSReaderBrukerTims msReader;
    QCOMPARE(msReader.classTypeName(), MSReaderBase::MSReaderClassTypeBrukerTims);

    // uncomment after Tims data is added
    //QCOMPARE(msReader.openFile(m_path), kFunctionNotImplemented);

    /*QVERIFY(msReader.canOpen(m_path));
    QCOMPARE(msReader.openFile(m_path), kNoErr);
    QVERIFY(!msReader.getFilename().isEmpty());
    QCOMPARE(pmi::isBrukerAnalysisBaf(m_path), m_isBrukerAnalysisBaf);
    QVERIFY(!msReader.hasUV());
    QCOMPARE(msReader.closeFile(), kNoErr);*/
}

void MSReaderBrukerTIMSTest::testOpenViaMSReader()
{
    QFileInfo fi(m_path);
    QVERIFY(fi.isDir() && fi.exists());

    /*MSReader *ms = MSReader::Instance();
    QVERIFY(ms);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
    QVERIFY(ms->getFilename().isEmpty());
    QCOMPARE(pmi::isBrukerAnalysisBaf(m_path), m_isBrukerAnalysisBaf);
    qDebug() << "Opening path:" << m_path;

    QCOMPARE(ms->openFile(m_path), kNoErr);
    QVERIFY(ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeBrukerTIMS);
    QVERIFY(!ms->getFilename().isEmpty());*/
}

void MSReaderBrukerTIMSTest::testChromatograms()
{

}

PMI_TEST_GUILESS_MAIN_WITH_ARGS(MSReaderBrukerTIMSTest, QStringList() << "path" << "isBrukerAnalysisBaf[0|1]")

#include "MSReaderBrukerTIMSTest.moc"
