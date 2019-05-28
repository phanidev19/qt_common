#include "MSReaderTestUtils.h"

#include "SequentialNonUniformTileIterator.h"
#include "NonUniformTileStoreSqlite.h"
#include <MSDataNonUniformAdapter.h>
#include <MSReader.h>
#include <MSReaderInfo.h>
#include <RandomNonUniformTileIterator.h>

#include <MSDataNonUniform.h>

#include <CacheFileManager.h>
#include <PMiTestUtils.h>

// common_stable
#include <pmi_core_defs.h>
#include <QtSqlUtils.h>

#include <QtTest>
#include <QSet>

#include "CsvWriter.h"

#include "NonUniformTileBuilder.h"
#include "NonUniformTileHillFinder.h"
#include "NonUniformTileIntensityIndex.h"
#include "NonUniformTileMaxIntensityFinder.h"
#include "NonUniformTilePoint.h"
#include "MzScanIndexRect.h"

#include "ChargeDeterminator.h"
#include "FeatureClusterSqliteFormatter.h"
#include "HillClusterCsvFormatter.h"
#include "InsilicoPeptidesCsvFormatter.h"
#include "NonUniformFeature.h"
#include "NonUniformFeatureFindingSession.h"
#include "NonUniformHillClusterFinder.h"
#include "PmiMemoryInfo.h"

#include "ChargeDeterminatorNN.h"
#include "MonoisotopeDeterminatorNN.h"

_PMI_BEGIN

//#define DUMP_XIC_STATS // dumps the XICs to CSV for observation
#define TEST_BUILD_CACHES // if not defined, it is assumed the caches are already prepared and you tweak just performance
#define TEST_MEASURE // if not defined, it is assumed you tweak just cache buildings or prepare them for performance tweaking measure later

// XIC windows files
const QStringList XIC_FILES_AvastinEU = {
    "011315_DM_AvastinEu_IA_LysN.XIC_0.bin",
    "011315_DM_AvastinEu_IA_LysN.XIC_1.bin",
    "011315_DM_AvastinEu_IA_LysN.XIC_2.bin"
};

const QStringList XIC_FILES_AvastinUS = {
    "011315_DM_AvastinUS_IA_LysN.XIC_0.bin",
    "011315_DM_AvastinUS_IA_LysN.XIC_1.bin",
    "011315_DM_AvastinUS_IA_LysN.XIC_2.bin"
};

const QString DM_AvastinEu = "011315_DM_AvastinEu_IA_LysN.raw";
const QString DM_AvastinUS = "011315_DM_AvastinUS_IA_LysN.raw";

QHash<QString, QStringList > FILE_XIC_WINDOWS = {
    { DM_AvastinEu, XIC_FILES_AvastinEU },
    { DM_AvastinUS, XIC_FILES_AvastinUS }
};

const QStringList MSDATA_TEST_FILES = {
    DM_AvastinEu,
//#define TEST_ALL_FILES
#ifdef TEST_ALL_FILES
    DM_AvastinUS
#endif
};


class FileWatcher {
public:
    FileWatcher(const QString &src, const QString &dst) :m_src(src), m_dst(dst), m_dir(dst) {
    }
    //! moves src to dst
    bool move(){
        bool renamed = m_dir.rename(m_src, m_dst);
        m_undo = true;

        return renamed;
    }

    bool undo(){
        bool renamed = m_dir.rename(m_dst, m_src);
        m_undo = false;
        return renamed;
    }

    ~FileWatcher() {
        if (m_undo) {
            undo();
        }
    }

    QString m_src;
    QString m_dst;
private:
    bool m_undo = false;
    QDir m_dir;
};

// MzTileLength, ScanIndexTileLength pair
const point2dList TILE_GRID_CONFIGURATIONS = {
//#define ALL_CONFIGS
#ifdef ALL_CONFIGS
    // Yong's tips 
    QPointF(1007.0, 301),
    QPointF(10000.0, 1),
    QPointF(0.0123, 2007),

    // derivates from tips above
    QPointF(0.05, 1003),
    QPointF(500.05, 150.0),

    // original defaults
    QPointF(30, 64),
    QPointF(60, 64),
    QPointF(1, 1000),
#else
    QPointF(1, 1000),
#endif
};


//! logged data for getXICData performance
struct LoggedData {
    double MzTileLength = -1;
    double ScanIndexTileLength = -1;
    double sumTime_NonUniform = -1;
    double sumTime_Manual = -1;
    int cacheIndex = -1;
    int tileCountHitDb = 0;
    int tileCountHitCache = 0;
    int tileCountForXICs = 0;

};

//! logged data for cache creation!

struct CacheCreateInfo 
{
    QString inputData; // we can test multiple input data
    int configIndex = 0; // we can test multiple configurations
    double mzTileLength = -1.0; // 30.0
    double scanIndexTileLength = -1;// 64
    double timeCreated = -1.0; // how long it took to create the cache
    double timeFileOpenScanInfo = -1.0;  // how long it took to open file and get scan data and meta info like min/max mz

    static void exportCSV(const QString &csvFilePath, const QVector<CacheCreateInfo> &data) 
    {
        QStringList columns = { "Input file", "Configuration", "MzTileLength", "ScanIndexTileLength", "Time", "Meta info time"};
        RowNode root;
        root.addData(columns);

        for (const CacheCreateInfo &info : data) {
            VariantItems items = VariantItems() << info.inputData << info.configIndex << info.mzTileLength << info.scanIndexTileLength << info.timeCreated << info.timeFileOpenScanInfo;
            root.addChild(new RowNode(items, &root));
        }
        
        QString msg = writeToFile(csvFilePath, root, AllDescendantNodes, false, nullptr);
        if (!msg.isEmpty()) {
            qDebug() << msg;
        }
    }

};


class MSDataNonUniformAdapterTest : public QObject
{
    Q_OBJECT
public:
    MSDataNonUniformAdapterTest(const QStringList &args);

private Q_SLOTS:
    void testCreateNonUniformTiles();
    void testHillClusterFinder_data();
    void testHillClusterFinder();

private:
    void dumpXICstatsToCsv();
    static QPoint tileCountForXICWindow(MSDataNonUniformAdapter * adapter, const NonUniformTileRange &range, const XICWindow &win,const MSReader * reader);

private:
    QDir m_testDataBasePath;
    const QScopedPointer<CacheFileManager> m_cacheFileManager;
};

MSDataNonUniformAdapterTest::MSDataNonUniformAdapterTest(const QStringList &args)
    : m_testDataBasePath(args[0])
    , m_cacheFileManager(new CacheFileManager())
{
}

QPoint MSDataNonUniformAdapterTest::tileCountForXICWindow(MSDataNonUniformAdapter * adapter, const NonUniformTileRange &range, const XICWindow &win,const MSReader * reader) {

    XICWindow fixedWindow;
    reader->alignTimesInWindow(win, adapter, 1, &fixedWindow);

    int tileXStart = range.tileX(fixedWindow.mz_start);
    int tileXEnd = range.tileX(fixedWindow.mz_end);
    int tileCountX = tileXEnd - tileXStart + 1;
    
    int scanIndexStart = adapter->m_data->m_converter.timeToScanIndex(fixedWindow.time_start);
    int scanIndexEnd = adapter->m_data->m_converter.timeToScanIndex(fixedWindow.time_end);

    int tileYStart = range.tileY(scanIndexStart);
    int tileYEnd = range.tileY(scanIndexEnd);
    int tileCountY = tileYEnd - tileYStart + 1;

    return QPoint(tileCountX, tileCountY);
}


void MSDataNonUniformAdapterTest::testCreateNonUniformTiles()
{
#ifdef TEST_BUILD_CACHES
    {
        qDebug() << "BUILD_CACHES";

        QVector<CacheCreateInfo> loggedData;
        for (const QString &fileName : MSDATA_TEST_FILES) {
            QString filePath = m_testDataBasePath.filePath(fileName);
            QVERIFY(QFileInfo(filePath).exists());

            QString msDataBaseName = QFileInfo(filePath).baseName();
            if (msDataBaseName.isEmpty()) {
                msDataBaseName = fileName;
            }
            int i = 0;

            CacheCreateInfo info;
            info.inputData = fileName;

            QElapsedTimer etMeta;
            etMeta.start();
            
            MSReader * reader = MSReader::Instance();
            QCOMPARE(reader->openFile(filePath), kNoErr);

            QList<msreader::ScanInfoWrapper> scanInfo;
            QCOMPARE(reader->getScanInfoListAtLevel(1, &scanInfo), kNoErr);

            bool ok;
            NonUniformTileRange range = MSDataNonUniformAdapter::createRange(reader, true, scanInfo.size(), &ok);
            QVERIFY(ok);
            
            info.timeFileOpenScanInfo = etMeta.elapsed();

            qDebug() << "Preset settings ready in" << info.timeFileOpenScanInfo << "ms";

            for (const QPointF& config : TILE_GRID_CONFIGURATIONS) {
                i++;
                info.configIndex = i;
                // if it already exists, skip this configuration
                // just the existance is checked, not the config in the final cache
                QString cacheFilePath;
                QCOMPARE(reader->cacheFileManager()->findOrCreateCachePath(
                             MSDataNonUniformAdapter::formatSuffix(), &cacheFilePath),
                         kNoErr);
                
                MSDataNonUniformAdapter adapter(cacheFilePath);
                QString srcCacheFilePath = adapter.cacheFilePath();
                QFileInfo srcFileInfo(srcCacheFilePath);
                QString dstDirPath = m_testDataBasePath.absolutePath() + "/" + QString("%1/%2").arg(msDataBaseName).arg(i);
                QDir dstDir(dstDirPath);
                QString dstCacheFilePath = dstDir.filePath(srcFileInfo.fileName());
                {
                    QFileInfo fi(dstCacheFilePath);
                    if (fi.exists()) {
                        qDebug() << "cache" << i << "already exists! Skipping!";
                        continue;
                    }
                }

                qDebug() << "Creating cache" << i;

                // tweak settings
                double currMzTileLength = config.x();
                double currScanIndexTileLength = config.y();
                range.setMzTileLength(currMzTileLength);
                range.setScanIndexLength(currScanIndexTileLength);

                info.mzTileLength = currMzTileLength;
                info.scanIndexTileLength = currScanIndexTileLength;

                if (adapter.hasValidCacheDbFile()) {
                    qDebug() << "We already have CACHE at" << adapter.cacheFilePath();
                    adapter.closeDatabase();
                    QVERIFY(QFile::remove(adapter.cacheFilePath()));
                }

                // create cache & measure creation time
                QElapsedTimer et;
                et.start();
                QCOMPARE(adapter.createNonUniformTiles(reader, range, scanInfo, nullptr), kNoErr);
                qint64 time = et.elapsed();

                info.timeCreated = time;

                QVERIFY(srcFileInfo.exists());
                QVERIFY(srcFileInfo.size() > 0);
                qDebug() << "Created cache " << i << "file" << fileName << "time" << time << "size" << srcFileInfo.size();

                // move it to dst 
                adapter.closeDatabase();
                qDebug() << "Creating dst path" << dstDirPath;
                QVERIFY(dstDir.mkpath(dstDirPath));
                QVERIFY(dstDir.rename(srcCacheFilePath, dstCacheFilePath));

                loggedData.append(info);
            }
            reader->releaseInstance();

        } // for fileName
        QString cacheCreateReportFilePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSDataNonUniformAdapterTest/createCache.csv");
        CacheCreateInfo::exportCSV(cacheCreateReportFilePath, loggedData);
    }
#endif

#ifdef DUMP_XIC_STATS
    qDebug() << "Dumping XIC stats!";
    dumpXICstatsToCsv();
#endif

#ifdef TEST_MEASURE

    qDebug() << "MEASURE";
    // MEASURE
    // caches are ready, now test the XIC perf data

    QHash<QString, QVector<LoggedData> > loggedInfo;

    QString xicWindowsBaseDir = QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/MSDataNonUniformAdapterTest");
    for (const QString &fileName : MSDATA_TEST_FILES) {
        QString msDataFilePath = m_testDataBasePath.filePath(fileName);
        QVERIFY(QFileInfo(msDataFilePath).exists());

        m_cacheFileManager->setSourcePath(msDataFilePath);

        QString msDataBaseName = QFileInfo(msDataFilePath).baseName();
        QString cacheFileName;

        {
            QString cacheFilePath;
            QCOMPARE(m_cacheFileManager->findOrCreateCachePath(MSDataNonUniformAdapter::formatSuffix(), &cacheFilePath), kNoErr);

            cacheFileName = QFileInfo(cacheFilePath).fileName();
        }

        // caches are indexed from [1..N]
        int cacheLastIndex = static_cast<int>(TILE_GRID_CONFIGURATIONS.size()); // from outside

        QVector<XICWindow> xicWindows;
        QStringList xicBinFiles = FILE_XIC_WINDOWS[fileName];
        for (const QString& xicBinFile : xicBinFiles) {
            QString xicBinFilePath = QDir(xicWindowsBaseDir).filePath(xicBinFile);
            QVector<XICWindow> wins = MSReaderTestUtils::xicFromFile(xicBinFilePath);
            QVERIFY(!wins.isEmpty());
            xicWindows.append(wins);
        }

        for (int cacheIndex = 1; cacheIndex <= cacheLastIndex; ++cacheIndex) {
            qDebug() << "==== Checking" << fileName << "cache" << cacheIndex << "====";
            
            QString srcCacheFilePath = QDir(m_testDataBasePath.filePath(msDataBaseName) + QString("/%1/").arg(cacheIndex)).filePath(cacheFileName);
            if (!QFileInfo(srcCacheFilePath).exists()) {
                qWarning() << "File" << fileName << "does not have cache" << cacheIndex << "at" << srcCacheFilePath << "Skipping!";
                continue;
            }
            
            // move/copy it to new location //TODO: provide API in MSReader to point to different cache
            QString dstCacheFilePath = m_testDataBasePath.filePath(cacheFileName);

            // if it exists, remove
            if (QFileInfo(dstCacheFilePath).exists()) {
                qWarning() << "Removing left-over cache!";
                QFile::remove(dstCacheFilePath);
            }
            
            // move to dst
            FileWatcher fw(srcCacheFilePath, dstCacheFilePath);
            QVERIFY(fw.move());

            LoggedData item;
            item.cacheIndex = cacheIndex;
            // get cache info (tile lenghts)
            NonUniformTileRange range;
            {
                QString cacheFilePath;
                QCOMPARE(m_cacheFileManager->findOrCreateCachePath(
                             MSDataNonUniformAdapter::formatSuffix(), &cacheFilePath),
                         kNoErr);

                MSDataNonUniformAdapter adapter(cacheFilePath);

                bool ok = false;
                range = adapter.loadRange(&ok);
                QVERIFY(ok);
                
                item.MzTileLength = range.mzTileLength();
                item.ScanIndexTileLength = range.scanIndexTileLength();
            }
            

            QStringList columns = { "NonUniform_ms", "Manual_ms", "TilesDb", "TilesCache", "TilesComputedX", "TilesComputedY" , "mz_start", "mz_end" , "time_start", "time_end" };
            RowNode root;
            root.addData(columns);

            XICWindow failWin;
            // measure performance
            {
                MSReader * reader = MSReader::Instance();
                reader->openFile(msDataFilePath);
                
                qreal nutSum = 0.0; 
                qreal manSum = 0.0; 
                bool ok = true;
                for (const XICWindow& window : xicWindows) {
                    // NonUniform cache enabled
                    reader->setXICDataMode(MSReader::XICModeTiledCache);
                    QElapsedTimer et;
                    et.start();
                    point2dList actual;
                    QCOMPARE(reader->getXICData(window, &actual), kNoErr);
                    qreal cacheTime = et.elapsed();
                    nutSum += cacheTime;

                    int cachedTiles = -1;
                    int dbTiles = -1;
                    QPoint tileCounts;
                    XICWindow nonUniformWin;
                    {
                        QSharedPointer<MSDataNonUniformAdapter> nonUniformTilesCache = reader->m_fileName_nonUniformTiles[reader->m_openReader->getFilename()];
                        if (nonUniformTilesCache.isNull()) {
                            qDebug() << "You are trying to read Vendor file! Turn off vendor method for XIC!";
                            QVERIFY(!nonUniformTilesCache.isNull());
                        }

                        MSDataNonUniform * data = nonUniformTilesCache->m_data;
                        data->m_manager->fetchCounts(&dbTiles, &cachedTiles);
                        data->m_manager->resetFetchCounters();

                        item.tileCountHitDb += dbTiles;
                        item.tileCountHitCache += cachedTiles;
                        tileCounts = tileCountForXICWindow(nonUniformTilesCache.data(), range, window, reader);
                        item.tileCountForXICs += tileCounts.x() * tileCounts.y();

                        reader->alignTimesInWindow(window, nonUniformTilesCache.data(), 1, &nonUniformWin);
                    }

                    // Compare with manual 
                    reader->setXICDataMode(MSReader::XICModeManual);
                    et.start();
                    point2dList expected;
                    QCOMPARE(reader->getXICData(window, &expected), kNoErr);
                    qreal rawTime = et.elapsed();
                    manSum += rawTime;

                    // log per XIC window perf data
                    
                    VariantItems logItemPerXIC = VariantItems() << cacheTime << rawTime << dbTiles << cachedTiles << tileCounts.x() << tileCounts.y()
                        << nonUniformWin.mz_start << nonUniformWin.mz_end << nonUniformWin.time_start << nonUniformWin.time_end;
                    root.addChild(new RowNode(logItemPerXIC, &root));

                    //QCOMPARE(actual, expected);
                    if (actual != expected) {
                        failWin = window;
                        ok = false;
                        break;
                    }
                }

                if (!ok) {
                    qWarning() << "Cache" << cacheIndex << "FAILED!!!" << failWin.mz_start << failWin.mz_end;
                    item.sumTime_NonUniform = -1.0;
                    item.sumTime_Manual = -1.0;

                } else {
                    item.sumTime_NonUniform = nutSum;
                    item.sumTime_Manual = manSum;
                }

                loggedInfo[fileName].push_back(item);

                // save info about the cache times
                QString filePathTimes = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSDataNonUniformAdapterTest/ms_%1_cacheIndex_%2.csv").arg(msDataBaseName).arg(cacheIndex);
                writeToFile(filePathTimes, root, AllDescendantNodes, false, nullptr);

                reader->releaseInstance();
            }

            // back again
            QVERIFY(fw.undo());
        } // for-each cache index
    } // for-each msData file

    // generate summary
    {
        QString reportFilePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSDataNonUniformAdapterTest/summary.csv");
        QFile file(reportFilePath);
        if (!file.open(QIODevice::WriteOnly)) {
            qWarning() << "Cannot write summary!";
            return;
        }

        QTextStream out(&file);
        // header
        static const QString sep = ";";
        out << "fileName" << sep
            << "cacheIndex" << sep
            << "MzTileLength" << sep
            << "ScanIndexTileLength" << sep
            << "sumTime_NonUniform" << sep
            << "sumTime_Manual" << sep 
            << "tileCountHitDB" << sep
            << "tileCountHitIteratorCache" << sep
            << "tileCountHitComputed" << sep
            << endl;

        // body
        for (const QString &fileName : MSDATA_TEST_FILES){
            QVector<LoggedData> items = loggedInfo[fileName];
            for (const LoggedData &logItem : items) {
                out << fileName << sep
                    << logItem.cacheIndex << sep
                    << logItem.MzTileLength << sep
                    << logItem.ScanIndexTileLength << sep
                    << logItem.sumTime_NonUniform << sep
                    << logItem.sumTime_Manual << sep 
                    << logItem.tileCountHitDb << sep
                    << logItem.tileCountHitCache << sep
                    << logItem.tileCountForXICs << sep
                    << endl;
            }
        }

        out.flush();
        file.close();
    }

#endif

}

void MSDataNonUniformAdapterTest::testHillClusterFinder_data()
{
    QTest::addColumn<bool>("outputHills");
    
    QTest::newRow("output-hills") << true;
    QTest::newRow("output-insilico-peptides-csv") << false;
}

void MSDataNonUniformAdapterTest::testHillClusterFinder()
{
    static const QString MSFILE = "cona_tmt0saxpdetd.raw";
    static const QString MSFILE_NON_UNIFORM_CACHE = "cona_tmt0saxpdetd.raw.NonUniform.cache";

    QRect tileRect(0, 0, 12, 12);
    double percLimit = 1.0;

    QString conaRaw = m_testDataBasePath.filePath(MSFILE);
    QString conaCache = m_testDataBasePath.filePath(MSFILE_NON_UNIFORM_CACHE);
    bool fullTest = false;

    qDebug() << "Testing file" << conaRaw;
    qDebug() << "Testing cache" << conaCache;

    // settings
    QString msFilePath = conaRaw;
    QString cacheFilePath = conaCache;

    MSReader *reader = MSReader::Instance();
    Err e = reader->openFile(msFilePath);
    QCOMPARE(e, kNoErr);

    qDebug() << "Centroided points" << MSReaderInfo(reader).pointCount(1, true);

    // load scan meta-info
    QList<msreader::ScanInfoWrapper> scanInfo;
    e = reader->getScanInfoListAtLevel(1, &scanInfo);
    QCOMPARE(e, kNoErr);

    // load centroided cache
    MSDataNonUniformAdapter centroidedDoc(cacheFilePath);
    e = centroidedDoc.load(scanInfo);
    QCOMPARE(e, kNoErr);
    NonUniformTileStore::ContentType content = NonUniformTileStore::ContentMS1Centroided;
    QVERIFY(centroidedDoc.hasData(content));

    QFETCH(bool, outputHills);

    // by default we export hills format, if test case flags otherwise, output features in insilico
    // format
    QString fileNameSuffix = outputHills ? QString() : QString("-insilico");
    QString csvFileName = QString("features%1.csv").arg(fileNameSuffix);

    QString csvFilePath = m_testDataBasePath.filePath(csvFileName);
    CsvWriter writer(csvFilePath, "\r\n", ',');
    bool csvFileIsOpened = writer.open();
    if (!csvFileIsOpened) {
        qWarning() << "Cannot open destination file" << csvFilePath;
        QVERIFY(csvFileIsOpened);
    }
    qDebug() << "Saving to csv file:\n" << csvFilePath;

    NonUniformFeatureFindingSession session(&centroidedDoc);
    session.setSearchArea(tileRect);

    NonUniformHillClusterFinder finder(&session);
    finder.setPercentLimit(percLimit);

    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");
    QVERIFY(QFileInfo::exists(neuralNetworkDbFilePath));
    
    ChargeDeterminatorNN *chargeDeterminator = new ChargeDeterminatorNN();
    e = chargeDeterminator->init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);
    finder.setChargeDeterminator(chargeDeterminator);

    MonoisotopeDeterminatorNN *monoDeterminator = new MonoisotopeDeterminatorNN();
    e = monoDeterminator->init(neuralNetworkDbFilePath);
    QCOMPARE(e, kNoErr);
    finder.setMonoisotopeDeterminator(monoDeterminator);


    quint32 pointCount = centroidedDoc.store()->pointCount(content, tileRect);
    qDebug() << pointCount << "points will be processed";

    FeatureClusterSqliteFormatter *sqliteFormatter = nullptr;
    bool sqliteEnabled = true;
    QSqlDatabase db;
    if (sqliteEnabled) {
        // db file
        QString dbFilePath = m_testDataBasePath.filePath("features.db3");
        if (QFileInfo(dbFilePath).exists()) {
            bool ok = QFile::remove(dbFilePath);
            QVERIFY2(ok, "Failed to remove temporary features database");
        }
        e = pmi::addDatabaseAndOpen("Features", dbFilePath, db);
        QVERIFY(e == kNoErr);

        sqliteFormatter = new FeatureClusterSqliteFormatter(&db, &session, &finder);
        e = sqliteFormatter->init();
        QVERIFY(e == kNoErr);
        e = sqliteFormatter->writeSamples(QStringList{ conaRaw });
        QVERIFY(e == kNoErr);
        sqliteFormatter->setCurrentSample(conaRaw);

        connect(&finder, &NonUniformHillClusterFinder::featureFound, sqliteFormatter,
                &FeatureClusterSqliteFormatter::acceptFeature);

        connect(sqliteFormatter, &FeatureClusterSqliteFormatter::rowFormatted, [&]() {
            Err e = sqliteFormatter->saveFeature();
            if (e != kNoErr) {
                qWarning() << "Failed to write feature to database!";
                finder.stop();
            }
        });
    }

    if (outputHills) {
        HillClusterCsvFormatter *formatter = new HillClusterCsvFormatter(&session, &finder);
        writer.writeRow(formatter->header());

        // register formatter for features
        connect(&finder, &NonUniformHillClusterFinder::featureFound, formatter,
            &HillClusterCsvFormatter::acceptFeature);

        // use formatter to output to CsvWriter
        connect(formatter, &HillClusterCsvFormatter::rowFormatted,
            [&writer](const QStringList &newHill) {
            writer.writeRow(newHill);
        });

    } else {
        InsilicoPeptidesCsvFormatter *formatter = new InsilicoPeptidesCsvFormatter(&session, &finder);
        writer.writeRow(formatter->header());

        // register formatter for features
        connect(&finder, &NonUniformHillClusterFinder::featureFound, formatter,
            &InsilicoPeptidesCsvFormatter::acceptFeature);

        // use formatter to output to CsvWriter
        connect(formatter, &InsilicoPeptidesCsvFormatter::rowFormatted,
            [&writer](const QStringList &newHill) {
            writer.writeRow(newHill);
        });
    }
    
    finder.run();

    int selectedPoints = -1;
    int deselectedPoints = -1;
    session.searchAreaSelectionStats(&selectedPoints, &deselectedPoints);
    
    if (percLimit == 1.0) {
        QCOMPARE(static_cast<quint32>(selectedPoints), pointCount);
        QCOMPARE(deselectedPoints, 0);
    } else {
        double selectedPerc = selectedPoints / double(pointCount);
        double absDistance = std::abs(selectedPerc - percLimit);
        QVERIFY(absDistance < 0.0001);
    }

    qDebug() << "Mem peak:" << MemoryInfo::peakProcessMemory();
    
    if (sqliteFormatter) {
        sqliteFormatter->flush();
    }

    db.close();
}

void MSDataNonUniformAdapterTest::dumpXICstatsToCsv()
{
    QString xicWindowsBaseDir = QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/auto/data" "/MSDataNonUniformAdapterTest");
    
    QStringList columns = { "mz_start", "mz_end", "time_start", "time_end", "scanIndexStart", "scanIndexEnd", "file" };
    RowNode root;
    root.addData(columns);
    
    QStringList fileNames = { DM_AvastinEu, DM_AvastinUS };
    for (const QString& rawFileName : fileNames){
        QStringList xicBinFiles = FILE_XIC_WINDOWS[rawFileName];

        QString rawFilePath = m_testDataBasePath.filePath(rawFileName);
        MSReader * reader = MSReader::Instance();
        QCOMPARE(reader->openFile(rawFilePath), kNoErr);

        ScanIndexNumberConverter converter = ScanIndexNumberConverter::fromMSReader(reader);

        for (const QString& xicBinFile : xicBinFiles) {
            QString xicBinFilePath = QDir(xicWindowsBaseDir).filePath(xicBinFile);
            QVector<XICWindow> wins = MSReaderTestUtils::xicFromFile(xicBinFilePath);
            {
                for (const XICWindow& win : wins) {
                    
                    long scanNumber = 0;
                    reader->getBestScanNumberFromScanTime(1, win.time_start, &scanNumber);
                    int scanIndexStart = converter.toScanIndex(scanNumber);
                    
                    reader->getBestScanNumberFromScanTime(1, win.time_end, &scanNumber);
                    int scanIndexEnd = converter.toScanIndex(scanNumber);
                    
                    VariantItems item = VariantItems() << win.mz_start << win.mz_end << win.time_start << win.time_end << scanIndexStart << scanIndexEnd << xicBinFile;
                    root.addChild(new RowNode(item, &root));
                }
            }
        }
    }
    
    QString xicStatsFilePath = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/MSDataNonUniformAdapterTest/xicWindowsStats.csv");
    writeToFile(xicStatsFilePath, root, AllDescendantNodes, false, nullptr);
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::MSDataNonUniformAdapterTest, QStringList() << "Remote Data Folder")

#include "MSDataNonUniformAdapterTest.moc"
