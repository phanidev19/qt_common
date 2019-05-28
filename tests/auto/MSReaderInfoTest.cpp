#include <QtTest>
#include "pmi_core_defs.h"
#include "PMiTestUtils.h"
#include "MSReader.h"
#include "MSReaderInfo.h"

_PMI_BEGIN

static const QString DM_Av_BYSPEC2 = QStringLiteral("020215_DM_Av.byspec2");
static const QString CONA_RAW = QStringLiteral("cona_tmt0saxpdetd.raw");

//#define MS_AVAS_SET
#ifndef MS_AVAS_SET
const QStringList TEST_FILES(
{
    DM_Av_BYSPEC2
    , CONA_RAW
});

QHash<QString, std::pair<double, double> > EXPECTED_MIN_MAX_MZ = {
    { DM_Av_BYSPEC2, std::pair<double, double>(197.07627868652344, 1499.9744873046875) },
    { CONA_RAW, std::pair<double, double>(380.00078187824499, 1706.6299982315684) }
};


#else
// You need to start test with argument c:\PMI-Test-Data\Data\MS-Avas

const QStringList TEST_FILES(
{ "011315_DM_AvastinEu_IA_LysN.raw",
"011315_DM_AvastinUS_IA_LysN.raw",
"011315_DM_BioS1_IA_LysN.raw",
"011315_DM_BioS2_IA_LysN.raw",
"011315_DM_BioS3_IA_LysN.raw",
"020215_DM_AvastinEU_trp_Glyco.raw",
"020215_DM_AvastinEU_trp_HCDETD.raw",
"020215_DM_AvastinUS_trp_Glyco.raw",
"020215_DM_AvastinUS_trp_HCDETD.raw",
"020215_DM_BioS2_trp_HCDETD.raw",
"020215_DM_BioS3_trp_Glyco.raw",
"020215_DM_BioS3_trp_HCDETD.raw",
//"021915_E_DM_AvastinEU_intact.raw", invalid files
//"021915_E_DM_AvastinUS_intact_150219185503.raw", invalid files
});

QHash<QString, std::pair<double, double> > EXPECTED_MIN_MAX_MZ = {
    { TEST_FILES[0], std::pair<double, double>(195.11941579919954392607905902, 1515.13291017816914063587319106) },
    { TEST_FILES[1], std::pair<double, double>(195.11975620894335747834702488, 1515.1391511106235157058108598) },
    { TEST_FILES[2], std::pair<double, double>(195.119772547679076524218544364, 1515.14056915087348897941410542) },
    { TEST_FILES[3], std::pair<double, double>(195.119584349872582151874667034, 1515.13715300895296422822866589) },
    { TEST_FILES[4], std::pair<double, double>(195.114692077629712230191216804, 1515.12901305618834157940000296) },
    { TEST_FILES[5], std::pair<double, double>(195.115740754947012192133115605, 1515.13555452529817557660862803) },
    { TEST_FILES[6], std::pair<double, double>(195.116071710772217784324311651, 1515.13129406721145642222836614) },
    { TEST_FILES[7], std::pair<double, double>(195.117771771153741156012983993, 1515.13162195298946244292892516) },
    { TEST_FILES[8], std::pair<double, double>(195.116214841239013821905246004, 1515.13012184436456664116121829) },
    { TEST_FILES[9], std::pair<double, double>(195.118968982557447588987997733, 1515.13581534838044717616867274) },
    { TEST_FILES[10], std::pair<double, double>(195.118020052482080473055248149, 1515.13515163681950070895254612) },
    { TEST_FILES[11], std::pair<double, double>(195.118332617809329576630261727, 1515.13287911404927399416919798) },
};

#endif

class MSReaderInfoTest : public QObject
{
    Q_OBJECT

public:
    explicit MSReaderInfoTest(const QStringList& args);

private Q_SLOTS:
    void testMzMinMax();
    void testPointCount();
    void testPointCount_data();

private:
    QDir m_testDataBasePath;
    QStringList m_files;
};

MSReaderInfoTest::MSReaderInfoTest(const QStringList& args) :m_testDataBasePath(args[0]), m_files()
{

}

void MSReaderInfoTest::testMzMinMax()
{
    QVector<qint64> times;
    for (const QString &fileName : TEST_FILES) {
        QString filePath = m_testDataBasePath.filePath(fileName);
        QVERIFY(QFileInfo(filePath).exists());

        MSReader * reader = MSReader::Instance();
        QCOMPARE(reader->openFile(filePath), kNoErr);

        MSReaderInfo mi(reader);
        
        QElapsedTimer et;
        et.start();

        bool ok;
        std::pair<double, double> minMax = mi.mzMinMax(false, &ok);
        if (!ok) {
            qDebug() << "Failed for" << fileName;
        }
        QVERIFY(ok);

        times.push_back(et.elapsed());
        qDebug() << "Elapsed:" << times.last();

        double min = minMax.first;
        double max = minMax.second;

        auto expectedValue = EXPECTED_MIN_MAX_MZ[fileName];
        QCOMPARE(minMax, expectedValue);

        qDebug() << "minMz:" << QString("%1").arg(min, 0, 'g', 30);
        qDebug() << "maxMz:" << QString("%1").arg(max, 0, 'g', 30);

        QCOMPARE(reader->closeFile(), kNoErr);
        reader->releaseInstance();
    }

    

}

void MSReaderInfoTest::testPointCount_data()
{
    QTest::addColumn<QString>("vendorFilePath");
    QTest::addColumn<int>("ms1ProfilePointCount");
    QTest::addColumn<int>("ms1CentroidPointCount");
    
    QTest::newRow("Thermo file") << m_testDataBasePath.filePath(CONA_RAW) << 72825129 << 5515907;
    QTest::newRow("Byspec file") << m_testDataBasePath.filePath(DM_Av_BYSPEC2) << 2119783 << 2119783;
}

void MSReaderInfoTest::testPointCount()
{
    QFETCH(QString, vendorFilePath);
    MSReader *reader = MSReader::Instance();
    QCOMPARE(reader->openFile(vendorFilePath), kNoErr);
    
    const int msLevel = 1;
    
    MSReaderInfo mi(reader);
    
    bool doCentroiding = true;
    int centroidedPointCount = static_cast<int>(mi.pointCount(1, doCentroiding));
    
    QFETCH(int, ms1CentroidPointCount);
    QCOMPARE(centroidedPointCount, ms1CentroidPointCount);

    doCentroiding = false;
    int profilePointCount = static_cast<int>(mi.pointCount(1, doCentroiding));
    QFETCH(int, ms1ProfilePointCount);
    QCOMPARE(profilePointCount, ms1ProfilePointCount);
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSReaderInfoTest, QStringList() << "Remote Data Folder")

#include "MSReaderInfoTest.moc"