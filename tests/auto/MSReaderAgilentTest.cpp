/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#include <MSReaderAgilent.h>
#include <MSReader.h>
#include <PMiTestUtils.h>
#include <qt_string_utils.h>
#include "safearraywrapper.h"
#include <VendorPathChecker.h>

/*
pwiz                    <-> Agilent
MassHunterData              IMsdrDataReader
    Transition                  subset of chromatogram data
Frame                       from MIDAC namespace
DriftScan                   from MIDAC namespace
ScanRecord                  IMSScanRecord
Spectrum                    IBDASpecData
Chromatogram                IBDAChromData

PeakFilter                  IMsdrPeakFilter
*/

#define TEST_ONLY_KNOWN_DATA

using namespace pmi;
using namespace pmi::msreader;

//! Compares two point lists using qFuzzyXXX functions
static bool fuzzyCompare(const point2dList& points1, const point2dList& points2)
{
    const point2dList::size_type size1 = points1.size();
    const point2dList::size_type size2 = points2.size();

    if (size1 != size2) {
        return false;
    }

    for (point2dList::size_type i = 0; i < size1; ++i) {
        const qreal x1 = points1.at(i).x();
        const qreal x2 = points2.at(i).x();

        const qreal y1 = points1.at(i).y();
        const qreal y2 = points2.at(i).y();

        const bool isX1Zero = qFuzzyIsNull(x1);
        const bool isX2Zero = qFuzzyIsNull(x2);

        if (isX1Zero != isX2Zero) {
            return false;
        }

        const bool isY1Zero = qFuzzyIsNull(y1);
        const bool isY2Zero = qFuzzyIsNull(y2);

        if (isY1Zero != isY2Zero) {
            return false;
        }

        if (!isX1Zero) {
            if (!qFuzzyCompare(x1, x2)) {
                return false;
            }
        }

        if (!isY1Zero) {
            if (!qFuzzyCompare(y1, y2)) {
                return false;
            }
        }
    }

    return true;
}

/*!
 * A helper structure for defining test cases for Internal channel names
 *
 * Each ICN from the list would produce a list of expected ICNs with a size = expectedChromCount
 * (mostly 1).
 */
struct InternalChannelNameInfo
{
    InternalChannelNameInfo(const QStringList& names, const QString& firstExpectedName, int expectedChromCount = 1)
        : names(names)
        , firstExpectedName(firstExpectedName)
        , expectedChromCount(expectedChromCount)
    {
        // The expected name must also work
        this->names.append(firstExpectedName);
    }

    QStringList names;
    QString firstExpectedName;
    int expectedChromCount = 1;
};

struct AgilentTestInfo
{
    AgilentTestInfo()
    {}

    AgilentTestInfo(const QString& filename
        , const QSet<QString>& allExpectedChannelNames
        , long firstChromatogramPointListSize
        , const QList<InternalChannelNameInfo>& channelNamesForFilterTest
        , long xicPointListSize
        , long xicMsLevel
        , bool hasUv
        , long uvPointListSize
        , long ticPointListSize
        , long scanDataScanNumber
        , bool scanDataDoCentroiding
        , long scanDataPointListSize
        , long spectraTotalNumber
        , long spectraStartScan
        , long spectraEndScan
        , const PrecursorInfo& precursorInfo
        , long scanInfoScanNumber
        , const ScanInfo& scanInfo
        , const QString& fragmentType
        , const XICWindow& xicWindow
        , int xicLevel
        , int xicIndex
        , double xicValue
        )
        : filename(filename)
        , allExpectedChannelNames(allExpectedChannelNames)
        , firstChromatogramPointListSize(firstChromatogramPointListSize)
        , channelNamesForFilterTest(channelNamesForFilterTest)
        , xicPointListSize(xicPointListSize)
        , xicMsLevel(xicMsLevel)
        , hasUv(hasUv)
        , uvPointListSize(uvPointListSize)
        , ticPointListSize(ticPointListSize)
        , scanDataScanNumber(scanDataScanNumber)
        , scanDataDoCentroiding(scanDataDoCentroiding)
        , scanDataPointListSize(scanDataPointListSize)
        , spectraTotalNumber(spectraTotalNumber)
        , spectraStartScan(spectraStartScan)
        , spectraEndScan(spectraEndScan)
        , precursorInfo(precursorInfo)
        , scanInfoScanNumber(scanInfoScanNumber)
        , scanInfo(scanInfo)
        , fragmentType(fragmentType)
        , xicWindow(xicWindow)
        , xicLevel(xicLevel)
        , xicIndex(xicIndex)
        , xicValue(xicValue)
    { }
    virtual ~AgilentTestInfo()
    {}

    QString filename;
    QSet<QString> allExpectedChannelNames;
    long firstChromatogramPointListSize = -1;
    QList<InternalChannelNameInfo> channelNamesForFilterTest;
    long xicPointListSize = -1;
    long xicMsLevel = -1;
    bool hasUv = false;
    long uvPointListSize = -1;
    long ticPointListSize = -1;
    long scanDataScanNumber = -1;
    bool scanDataDoCentroiding = false;
    long scanDataPointListSize = -1;
    long spectraTotalNumber = -1;
    long spectraStartScan = -1;
    long spectraEndScan = -1;
    PrecursorInfo precursorInfo;
    long scanInfoScanNumber = -1;
    ScanInfo scanInfo;
    QString fragmentType = "CID";
    XICWindow xicWindow;
    int xicLevel = 1;
    int xicIndex;
    double xicValue;
};

// Expected data for test files
static QVector<AgilentTestInfo> agilentTestInfoList =
{
    { AgilentTestInfo("BMS-ADC2-1000ng.d" // filename
        ,
            {
                QLatin1String("ct=5,dt=6,dn=QTOF,dot=1,sn="),
                QLatin1String("ct=6,dt=6,dn=QTOF,dot=1,sn=")
            }
        , 478 // firstChromatogramPointListSize
        ,
            {
                // a list (size=1 in this case) of internal channel names that would produce the expected internal channel name
                InternalChannelNameInfo({ "ct=7,dt=6,dn=QTOF,dot=1" }, "ct=7,dt=6,dn=QTOF,dot=1,sn=")
            }
        , 241 // xicPointListSize
        , 1 // xicMsLevel
        , false // hasUv
        , 0 // uvPointListSize
        , 478 // ticPointListSize
        , 1 // scanDataScanNumber
        , false // scanDataDoCentroiding
        , 188650 // scanDataPointListSize
        , 478 // spectraTotalNumber
        , 1 // spectraStartScan
        , 478 // spectraEndScan
        , PrecursorInfo() // PrecursorInfo doesn't exist because there is no scan with MsLevel=2
        , 1 // scanInfoScanNumber
        , ScanInfo(0.03876667, 1, "scanId=2333", PeakPickingProfile, ScanMethodFullScan) // retTimeMinutes, scanLevel, nativeId, peakMode, scanMethod
        , QString() // fragmentType
        , XICWindow(0.0, 605.0, 1.0, 3.0) // XicWindow
        , 1 // xicLevel
        , 0 // xicIndex
        , 1055.0267333984375 // xicValue
        )
    },
    { AgilentTestInfo("Contains_UV_Neupogen.d" // filename
        ,
            {
                QLatin1String("ct=5,dt=6,dn=QTOF,dot=1,sn="),
                QLatin1String("ct=6,dt=6,dn=QTOF,dot=1,sn="),
                QLatin1String("ct=1,dt=14,dn=DAD,dot=1,sn=A")
            }
        , 3205 // firstChromatogramPointListSize
        ,
            {
                InternalChannelNameInfo({ "ct=5,dt=6,dn=QTOF,dot=1" }, "ct=5,dt=6,dn=QTOF,dot=1,sn=")
            }
        , 2857 // xicPointListSize
        , 1 // xicMsLevel
        , false // hasUv
        , 5550 // uvPointListSize
        , 2857 // ticPointListSize
        , 1 // scanDataScanNumber
        , false // scanDataDoCentroiding
        , 0 // scanDataPointListSize
        , 3205 // spectraTotalNumber
        , 1 // spectraStartScan
        , 3205 // spectraEndScan
        , PrecursorInfo(735, 472.27172851562500, 472.27172851562500, 2, "scanId=491728") // scanNumber, isolationMass, monoIsoMass, chargeState, nativeId
        , 1 // scanInfoScanNumber
        , ScanInfo(0.03435, 1, "scanId=2069", PeakPickingCentroid, ScanMethodFullScan) // retTimeMinutes, scanLevel, nativeId, peakMode, scanMethod
        , QString() // fragmentType
        , XICWindow(200.0, 300.0, 0.0, 0.0) // XicWindow
        , 1 // xicLevel
        , 2000 // xicIndex
        , 29540.501953125 // xicValue
        )
    },
    { AgilentTestInfo(QString::fromStdWString(L"°Contains_UV_Neupogen.d") // unicoded filename
        , {}
        , 0
        , QList<InternalChannelNameInfo>()
        , 0
        , 0
        , false
        , 0
        , 0
        , 0
        , false
        , 0
        , 0
        , 0
        , 0
        , PrecursorInfo()
        , 0
        , ScanInfo()
        , QString()
        , XICWindow() // XicWindow
        , 1 // xicLevel
        , 0 // xicIndex
        , 0.0 // xicValue
        )
    },
    { AgilentTestInfo("ecoli-0500-r001.d" // filename
        ,
            {
                QLatin1String("ct=6,dt=6,dn=QTOF,dot=1,sn="),
                QLatin1String("ct=5,dt=6,dn=QTOF,dot=1,sn=")
            }
        , 18150 // firstChromatogramPointListSize
        ,
            {
                InternalChannelNameInfo({ "ct=5,dt=6,dn=QTOF,dot=1" }, "ct=5,dt=6,dn=QTOF,dot=1,sn=")
            }
        , 1143 // xicPointListSize
        , 1 // xicMsLevel
        , false // hasUv
        , 0 // uvPointListSize
        , 4502 // ticPointListSize
        , 1 // scanDataScanNumber
        , false // scanDataDoCentroiding
        , 299 // scanDataPointListSize
        , 18150 // spectraTotalNumber
        , 1 // spectraStartScan
        , 18150 // spectraEndScan
        , PrecursorInfo(437, 1198.4201049804687, 1198.4201049804687, 2, "scanId=110596") // scanNumber, isolationMass, monoIsoMass, chargeState, nativeId
        , 1 // scanInfoScanNumber
        , ScanInfo(0.035817, 1, "scanId=2156", PeakPickingCentroid, ScanMethodFullScan) // retTimeMinutes, scanLevel, nativeId, peakMode, scanMethod
        , QString() // fragmentType
        , XICWindow(200.0, 300.0, 5.0, 30.0) // XicWindow
        , 1 // xicLevel
        , 200 // xicIndex
        , 20964.29296875 // xicValue
        )
    },
    { AgilentTestInfo("PL-41897_Red_MT031015_06.d" // filename
        ,
            {
                QLatin1String("ct=6,dt=4,dn=TOF,dot=1,sn="),
                QLatin1String("ct=5,dt=4,dn=TOF,dot=1,sn="),
                QLatin1String("ct=1,dt=14,dn=DAD,dot=1,sn=A"),
                QLatin1String("ct=1,dt=14,dn=DAD,dot=1,sn=B"),
                QLatin1String("ct=1,dt=14,dn=DAD,dot=1,sn=E")
            }
        , 1988 // firstChromatogramPointListSize
        ,
            {
                InternalChannelNameInfo({ "ct=5,dt=4,dn=TOF,dot=1" }, "ct=5,dt=4,dn=TOF,dot=1,sn=")
            }
        , 302 // xicPointListSize
        , 1 // xicMsLevel
        , false // hasUv
        , 4951 // uvPointListSize
        , 1988 // ticPointListSize
        , 900 // scanDataScanNumber
        , false // scanDataDoCentroiding
        , 82637 // scanDataPointListSize
        , 1988 // spectraTotalNumber
        , 1 // spectraStartScan
        , 1988 // spectraEndScan
        , PrecursorInfo() // scanNumber, isolationMass, monoIsoMass, chargeState, nativeId
        , 1 // scanInfoScanNumber
        , ScanInfo(0.038333, 1, "scanId=2307", PeakPickingCentroid, ScanMethodFullScan) // retTimeMinutes, scanLevel, nativeId, peakMode, scanMethod
        , QString() // fragmentType
        , XICWindow(700.0, 800.0, 20.0, 25.0) // XicWindow
        , 1 // xicLevel
        , 111 // xicIndex
        , 21911.87890625 // xicValue
        )
    },
    //NOTE(Ivan Skiba 2018-05-10): This data file isn't included to the auto tests.
    { AgilentTestInfo("LW_08Feb16_Fetuin.d" // filename
        , {}
        , 36592 // firstChromatogramPointListSize
        ,
            {
                InternalChannelNameInfo({ "ct=0,dt=18,dn=FluorescenceDetector,dot=1" }, "ct=0,dt=18,dn=FluorescenceDetector,dot=1,sn=")
            }
        , 17475 // xicPointListSize
        , 1 // xicMsLevel
        , false // hasUv
        , 19502 // uvPointListSize
        , 36550 // ticPointListSize
        , 900 // scanDataScanNumber
        , false // scanDataDoCentroiding
        , 7466 // scanDataPointListSize
        , 36592 // spectraTotalNumber
        , 1 // spectraStartScan
        , 36592 // spectraEndScan
        , PrecursorInfo() // scanNumber, isolationMass, monoIsoMass, chargeState, nativeId
        , 1 // scanInfoScanNumber
        , ScanInfo(0.0468833, 1, "scanId=2307", PeakPickingCentroid, ScanMethodFullScan) // retTimeMinutes, scanLevel, nativeId, peakMode, scanMethod
        , QString() // fragmentType
        , XICWindow(500.0, 1000.0, 20.0, 80.0) // XicWindow
        , 1 // xicLevel
        , 2000 // xicIndex
        , 549.2249755859375 // xicValue
        )
    },
    { AgilentTestInfo("N44504-30-1.d" // filename
        ,
            {
                QLatin1String("ct=5,dt=4,dn=TOF,dot=1,sn="),
                QLatin1String("ct=6,dt=4,dn=TOF,dot=1,sn="),
                QLatin1String("ct=1,dt=14,dn=DAD,dot=1,sn=C"),
                QLatin1String("ct=1,dt=14,dn=DAD,dot=1,sn=F")
            }
        , 1248 // firstChromatogramPointListSize
        ,
            {
                InternalChannelNameInfo({ "ct=1,dt=14,dn=DAD,dot=1,sn=C,sd=Sig=210,16" }, "ct=1,dt=14,dn=DAD,dot=1,sn=C"),
                // a list of internal channel names that would produce 2 chromotograms each, the first chromatogram with the expected internal channel name
                InternalChannelNameInfo({ "ct=1,dt=14,dn=DAD,dot=1", "ct=1,dt=14,dn=DAD,dot=1,sn=" }, "ct=1,dt=14,dn=DAD,dot=1,sn=C", 2),
                InternalChannelNameInfo({ "ct=1,dt=14,dn=DAD,dot=1,sn=F,sd=Sig=210,16" }, "ct=1,dt=14,dn=DAD,dot=1,sn=F"),
            }
        , 313 // xicPointListSize
        , 1 // xicMsLevel
        , true // hasUv
        , 3000 // uvPointListSize
        , 1248 // ticPointListSize
        , 100 // scanDataScanNumber
        , false // scanDataDoCentroiding
        , 62194 // scanDataPointListSize
        , 1248 // spectraTotalNumber
        , 1 // spectraStartScan
        , 1248 // spectraEndScan
        , PrecursorInfo() // scanNumber, isolationMass, monoIsoMass, chargeState, nativeId
        , 6 // scanInfoScanNumber
        , ScanInfo(0.1194833, 1, "scanId=7177", PeakPickingCentroid, ScanMethodFullScan) // retTimeMinutes, scanLevel, nativeId, peakMode, scanMethod
        , QString() // fragmentType
        , XICWindow(1500.0, 2000.0, 5.0, 10.0) // XicWindow
        , 1 // xicLevel
        , 222 // xicIndex
        , 966352.0 // xicValue
        )
    }
};

class MSReaderAgilentTest : public QObject
{
    Q_OBJECT
public:
    //! @a path points to the .d data dir
    explicit MSReaderAgilentTest(const QStringList& args)
        : QObject()
        , m_args(args)
    {
    }

    ~MSReaderAgilentTest() {}

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();

    void testIsAgilentPath();
    void testOpenDirect();
    void testOpenViaMSReader();
    void testMSReaderInstance();
    void testRemoveZeroIntensities();
    void testDumpToFile();
    void testGetChromatograms();
    void testGetXICData();
    void testGetTICData();
    void testGetScanData();
    void testGetNumberOfSpectra();
    void testGetScanPrecursorInfo();
    void testGetScanInfo();
    void testFragmentorType();

private:
    QString path() const;
    void populateTestData();
    void prepareTestPaths();

private:
    const QStringList m_args;
    bool m_validArguments = false;
    QString m_path;
    AgilentTestInfo m_agilentTestInfo;
    QSharedPointer<MSReaderAgilent> m_agilentReader;
};

void MSReaderAgilentTest::initTestCase()
{
    // This should complete with no errors
    populateTestData();
    prepareTestPaths();

    QVERIFY(m_validArguments);

    m_agilentReader.reset(new MSReaderAgilent());

    QCOMPARE(m_agilentReader->openFile(path()), kNoErr);
}

void MSReaderAgilentTest::testMSReaderInstance()
{
    MSReader* ms = MSReader::Instance();
    QVERIFY(ms);
}

void MSReaderAgilentTest::testDumpToFile()
{
    m_agilentReader->dbgTest();
    return;

    MSReader* ms = MSReader::Instance();
    m_path = "C:\\Users\\dev\\Documents\\projects\\common_ms\\tests\\auto\\MSReaderAgilentTest\\BMS-ADC2-1000ng.mzML";
    ms->openFile(path());

    //m_agilentReader->dumpToFile();
    //m_agilentReader->dumpAllScans();
}

void MSReaderAgilentTest::cleanupTestCase()
{
    if (m_agilentReader.data() != nullptr) {

        m_agilentReader->closeFile();
    }
}

void MSReaderAgilentTest::testIsAgilentPath()
{
    QVERIFY(VendorPathChecker::isAgilentPath("foo.d"));
    QVERIFY(!VendorPathChecker::isAgilentPath("foo.somethingelse"));
    QVERIFY(VendorPathChecker::isAgilentPath("foo.d"));
    QVERIFY(!VendorPathChecker::isAgilentPath("foo.bar"));
}

void MSReaderAgilentTest::testOpenDirect()
{
    QFileInfo fi(path());
    QVERIFY(fi.isDir());
    QVERIFY(fi.exists());

    MSReaderAgilent reader;
    QCOMPARE(reader.classTypeName(), MSReaderBase::MSReaderClassTypeAgilent);
    QVERIFY(reader.canOpen(path()));
    QCOMPARE(reader.openFile(path()), kNoErr);
    QVERIFY(!reader.getFilename().isEmpty());
    QCOMPARE(reader.closeFile(), kNoErr);
}

void MSReaderAgilentTest::testOpenViaMSReader()
{
    QFileInfo fi(path());
    QVERIFY(fi.isDir());
    QVERIFY(fi.exists());

    MSReader* ms = MSReader::Instance();
    QVERIFY(ms);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
    QVERIFY(ms->getFilename().isEmpty());

    QCOMPARE(ms->openFile(path()), kNoErr);
    QVERIFY(ms->isOpen());
    QVERIFY(!ms->getFilename().isEmpty());

    QCOMPARE(ms->closeFile(), kNoErr);
    QVERIFY(!ms->isOpen());
    QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
}

QString MSReaderAgilentTest::path() const
{
    return m_path;
}

void MSReaderAgilentTest::prepareTestPaths()
{
    const QString arg = m_args[0].trimmed();
    const QFileInfo fileInfo(arg);
    // args[0] can be either a plain file name or a full file path
    const bool isPath = fileInfo.exists();
    const QString filename = isPath ? fileInfo.fileName() : arg;

    m_path = isPath ? arg : QString("%1/MSReaderAgilentTest/%2").arg(PMI_TEST_FILES_OUTPUT_DIR).arg(filename);

    auto iter = std::find_if(std::cbegin(agilentTestInfoList), std::cend(agilentTestInfoList), [&filename](const AgilentTestInfo& ati)
    {
        return filename == ati.filename;
    });

    if (iter != std::cend(agilentTestInfoList)) {
        m_validArguments = true;
        m_agilentTestInfo = *iter;

#ifndef TEST_ONLY_KNOWN_DATA
    } else {
        m_validArguments = true;
        m_agilentTestInfo = agilentTestInfoList.front();
#endif // #ifndef TEST_ONLY_KNOWN_DATA
    }
}

void MSReaderAgilentTest::populateTestData()
{
    // Update "°Contains_UV_Neupogen.d" with the expected data from "Contains_UV_Neupogen.d"
    const QString sourceTestInfo = "Contains_UV_Neupogen.d";

    auto source = std::find_if(std::begin(agilentTestInfoList), std::end(agilentTestInfoList), [&sourceTestInfo](const AgilentTestInfo& ati)
    {
        return sourceTestInfo == ati.filename;
    });

    QVERIFY(source != std::end(agilentTestInfoList));

    auto target = std::find_if(std::begin(agilentTestInfoList), std::end(agilentTestInfoList), [&sourceTestInfo](const AgilentTestInfo& ati)
    {
        return sourceTestInfo != ati.filename && ati.filename.contains(sourceTestInfo);
    });

    QVERIFY(target != std::end(agilentTestInfoList));

    const QString filename = target->filename;
    *target = *source;
    target->filename = filename;
}

void MSReaderAgilentTest::testGetChromatograms()
{
    QList<ChromatogramInfo> chromatogramList;
    QString channelInternalName = "";
    m_agilentReader->getChromatograms(&chromatogramList, channelInternalName);

    // 1. Check the expected number of chromotograms
    {
        QSet<QString> allActualChannelNames;

        for (const auto chromatogramInfo : chromatogramList) {
            allActualChannelNames.insert(chromatogramInfo.internalChannelName);
        }

        if (allActualChannelNames != m_agilentTestInfo.allExpectedChannelNames) {
            const QSet<QString> newUnexpectedChannels = QSet<QString>(allActualChannelNames)
                .subtract(m_agilentTestInfo.allExpectedChannelNames);

            if (!newUnexpectedChannels.empty()) {
                qDebug() << "";
                qDebug() << "New unexpected chromatograms were added:";

                for (const auto channelName : newUnexpectedChannels) {
                    qDebug() << "\t" << channelName;
                }

                qDebug() << "";
            }

            const QSet<QString> missingExpectedChannels =
                QSet<QString>(m_agilentTestInfo.allExpectedChannelNames)
                .subtract(allActualChannelNames);

            if (!missingExpectedChannels.empty()) {
                qDebug() << "";
                qDebug() << "Some expected chromatograms are missing:";

                for (const auto channelName : missingExpectedChannels) {
                    qDebug() << "\t" << channelName;
                }

                qDebug() << "";
            }

            QFAIL("Actual and expected chromatogram sets are not the same.");
        }
    }

    const long sz = chromatogramList.size() > 0 ? (long)chromatogramList.at(0).points.size() : -1;
    QCOMPARE(sz, m_agilentTestInfo.firstChromatogramPointListSize);

    // 2. Ensure that each channel has a unique name AND it produces a single chromotogram
    for (QList<ChromatogramInfo>::size_type i = 0; i < chromatogramList.size(); ++i) {

        const ChromatogramInfo& info1 = chromatogramList.at(i);

        QList<ChromatogramInfo> chromList;
        m_agilentReader->getChromatograms(&chromList, info1.internalChannelName);
        QCOMPARE(chromList.size(), 1);

        for (QList<ChromatogramInfo>::size_type j = i + 1; j < chromatogramList.size(); ++j) {

            const ChromatogramInfo& info2 = chromatogramList.at(j);

            QVERIFY(info1.internalChannelName != info2.internalChannelName);
            QVERIFY(info1.channelName != info2.channelName);
        }
    }

    // 3. Check the expected chtromatogram by channel name
    for (const InternalChannelNameInfo& info : m_agilentTestInfo.channelNamesForFilterTest) {
        for (const QString& name : info.names) {
            QList<ChromatogramInfo> chromatogramListByName;

            m_agilentReader->getChromatograms(&chromatogramListByName, name);

            const int expectedChromCount = name != info.firstExpectedName ? info.expectedChromCount : 1;

            QCOMPARE(chromatogramListByName.size(), expectedChromCount);
            QCOMPARE(chromatogramListByName.front().internalChannelName, info.firstExpectedName);
            QVERIFY(chromatogramListByName.front().points.size() > 0);
        }
    }
}

void MSReaderAgilentTest::testGetXICData()
{
    point2dList points;
    m_agilentReader->getXICData(m_agilentTestInfo.xicWindow, &points, m_agilentTestInfo.xicLevel);

    QCOMPARE((long)points.size(), m_agilentTestInfo.xicPointListSize);

    const double actual = points.at(m_agilentTestInfo.xicIndex).ry();
    QCOMPARE(actual, m_agilentTestInfo.xicValue);
}

void MSReaderAgilentTest::testGetTICData()
{
    point2dList points;
    m_agilentReader->getTICData(&points);
    QCOMPARE((long)points.size(), m_agilentTestInfo.ticPointListSize);
}

void MSReaderAgilentTest::testGetScanData()
{
    point2dList points;
    msreader::PointListAsByteArrays* pointListAsByteArrays = nullptr;
    m_agilentReader->getScanData(m_agilentTestInfo.scanDataScanNumber, &points, m_agilentTestInfo.scanDataDoCentroiding, pointListAsByteArrays);
    QCOMPARE((long)points.size(), m_agilentTestInfo.scanDataPointListSize);
}

void MSReaderAgilentTest::testGetNumberOfSpectra()
{
    long totalNumber = -1;
    long startScan = -1;
    long endScan = -1;
    m_agilentReader->getNumberOfSpectra(&totalNumber, &startScan, &endScan);

    QCOMPARE(totalNumber, m_agilentTestInfo.spectraTotalNumber);
    QCOMPARE(startScan, m_agilentTestInfo.spectraStartScan);
    QCOMPARE(endScan, m_agilentTestInfo.spectraEndScan);
}

void MSReaderAgilentTest::testGetScanPrecursorInfo()
{
    if (!m_agilentTestInfo.precursorInfo.isValid()) {
        // no test required
        return;
    }

    const long scanNumber = m_agilentTestInfo.precursorInfo.nScanNumber;
    PrecursorInfo precursorInfo;
    m_agilentReader->getScanPrecursorInfo(scanNumber, &precursorInfo);

    QCOMPARE(precursorInfo.nChargeState, m_agilentTestInfo.precursorInfo.nChargeState);
    QCOMPARE(precursorInfo.dIsolationMass, m_agilentTestInfo.precursorInfo.dIsolationMass);
    QCOMPARE(precursorInfo.nativeId, m_agilentTestInfo.precursorInfo.nativeId);
}

void MSReaderAgilentTest::testGetScanInfo()
{
    ScanInfo scanInfo;
    m_agilentReader->getScanInfo(m_agilentTestInfo.scanInfoScanNumber, &scanInfo);

    QCOMPARE(scanInfo.scanLevel, m_agilentTestInfo.scanInfo.scanLevel);
    QCOMPARE(scanInfo.peakMode, m_agilentTestInfo.scanInfo.peakMode);
    QVERIFY(almostEqual(scanInfo.retTimeMinutes, m_agilentTestInfo.scanInfo.retTimeMinutes, 1e-6));
    QCOMPARE(scanInfo.nativeId, m_agilentTestInfo.scanInfo.nativeId);
}

void MSReaderAgilentTest::testFragmentorType()
{
    QString fragmentType;
    m_agilentReader->getFragmentType(0, 0, &fragmentType);
    QCOMPARE(fragmentType, m_agilentTestInfo.fragmentType);
}

struct ZeroIntensitiesTestInfo final
{
    // x values are dummy, can be identical to y for this test
    // note due to the different type of Y values we must perform conversion
    std::vector<double> src;
    std::vector<double> expected;

    ZeroIntensitiesTestInfo(const std::vector<double>& src, const std::vector<double>& expected)
        : src(src)
        , expected(expected)
    {
    }
};

void MSReaderAgilentTest::testRemoveZeroIntensities()
{
    const double maxError = 0.0001;

    const std::vector<ZeroIntensitiesTestInfo> testDataList =
    {
        ZeroIntensitiesTestInfo(
        { 0.034350, 0.045450, 0.056550, 0.067667, 0.078767, 0.089883 },
        { 0.034350, 0.045450, 0.056550, 0.067667, 0.078767, 0.089883 }
        ),

        ZeroIntensitiesTestInfo(
        { },
        { }
        ),

        ZeroIntensitiesTestInfo(
        { 0.034350 },
        { 0.034350 }
        ),

        ZeroIntensitiesTestInfo(
        { 0.0 },
        { 0.0 }
        ),

        ZeroIntensitiesTestInfo(
        { 0.0, 0.045450 },
        { 0.0, 0.045450 }
        ),

        ZeroIntensitiesTestInfo(
        { 0.0, 0.0 },
        { 0.0, 0.0 }
        ),

        // Probably a bug in pwiz, they would clean up any zero-filled vector
        // which size > 2 but leave zero-filled vector of size <= 2
        ZeroIntensitiesTestInfo(
        { 0.0, 0.0, 0.0 },
        { }
        ),

        ZeroIntensitiesTestInfo(
        { 0.0, 0.045450, 0.0 },
        { 0.0, 0.045450, 0.0 }
        ),

        ZeroIntensitiesTestInfo(
        { 0.0, 0.056550, 0.0, 0.067667, 0.078767, 0.0, 0.089883, 0.0 },
        { 0.0, 0.056550, 0.0, 0.067667, 0.078767, 0.0, 0.089883, 0.0 }
        ),

        // Possibly another bug in pwiz, they do not remove such zeroes, although they are not meaningful
        ZeroIntensitiesTestInfo(
        { 0.0, 0.0, 0.056550, 0.0, 0.0, 0.067667, 0.0, 0.078767, 0.0, 0.0, 0.089883, 0.0, 0.0 },
        { 0.0, 0.056550, 0.0, 0.0, 0.067667, 0.0, 0.078767, 0.0, 0.0, 0.089883, 0.0 }
        ),

        ZeroIntensitiesTestInfo(
        { 0.0, 0.0, 0.0, 0.056550, 0.0, 0.0, 0.0, 0.067667, 0.0, 0.0, 0.078767, 0.0, 0.0, 0.0, 0.089883, 0.0, 0.0 },
        { 0.0, 0.056550, 0.0, 0.0, 0.067667, 0.0, 0.0, 0.078767, 0.0, 0.0, 0.089883, 0.0, }
        ),
    };

    for (const auto& testData: testDataList) {

        std::vector<float> ySrc;
        ySrc.reserve(testData.src.size());
        for (double x : testData.src) {
            ySrc.push_back(static_cast<float>(x));
        }

        SAFEARRAY* xArr = nullptr;
        QVERIFY(safeArrayFromVector(testData.src, &xArr));
        SafeArrayWrapper<double> xvals(xArr);

        SAFEARRAY* yArr = nullptr;
        QVERIFY(safeArrayFromVector(ySrc, &yArr));
        SafeArrayWrapper<float> yvals(yArr);

        point2dList points;
        MSReaderAgilent::removeZeroIntensities(xvals, yvals, &points);

        QCOMPARE(points.size(), testData.expected.size());

        for (size_t i = 0; i < testData.expected.size(); ++i) {
            const double x1 = testData.expected[i];
            const double x2 = static_cast<double>(points[i].y());
            QVERIFY(almostEqual(x1, x2, maxError));
        }
    }
}

PMI_TEST_GUILESS_MAIN_WITH_ARGS(MSReaderAgilentTest, QStringList() << "path")

#include "MSReaderAgilentTest.moc"
