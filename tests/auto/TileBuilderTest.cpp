#include <QtTest>
#include <QFileInfo>
#include <QFile>
#include <QPainter>
#include <QStringList>

#include <ComInitializer.h>

#include "pmi_core_defs.h"
#include "TileBuilder.h"
#include "MSReader.h"
#include "MSReaderInfo.h"
#include "TileRange.h"
#include "TileStoreSqlite.h"
#include "CheckerDataProvider.h"
#include "db\ScanInfoDao.h"
#include "PMiTestUtils.h"


_PMI_BEGIN

static const QString CONA_RAW("cona_tmt0saxpdetd.raw");
static const QString CONA_TILES_LEVEL_1("cona_tmt0saxpdetd_level_1.db3");
static const QString CONA_TILES_LEVEL_1_8("cona_tmt0saxpdetd_level_1_8.db3");
static const QString CONA_TILES_LEVEL_1_8_RIP("cona_tmt0saxpdetd_level_1_8_RIP.db3");
static const QString FLOWER_IMAGE("pictures-of-flowers17.jpg");
static const QString FLOWER_TILES("pictures-of-flowers17.db3");
static const QString IMAGE_PATTERN_TILES("image-pattern.db3");
static const QString CHECKER_PATTERN_TILES("checker-pattern.db3");

static const QString PIXEL_TILE_RANGE_PRESET("pixelTileRangePreset.json");

class TileBuilderTest : public QObject
{
    Q_OBJECT
public:
    explicit TileBuilderTest(const QStringList &args);

private:
    MSReader * initReader();
    void cleanUpReader(MSReader * ms);

    void compareTileDocuments(const QString &actualFileName, const QString &expectedFileName, int startLevel, int endLevel, bool * ok);

private Q_SLOTS:
    void testBuildLevel1();
    void testBuildTilePyramid();
    void testBuildRipTilePyramid();

    void testBuildLevel1fromImage();
    void testBuildLevel1fromImagePattern();

    void testCreateCheckerDocument();

private:
    QDir m_testDataBasePath;
    MSReader * m_ms;
};


TileBuilderTest::TileBuilderTest(const QStringList &args) 
    : m_testDataBasePath(args[0])
{
}

MSReader * TileBuilderTest::initReader()
{
    return MSReader::Instance();
}



void TileBuilderTest::cleanUpReader(MSReader * ms)
{
    ms->releaseInstance();
    ms = nullptr;
}


void TileBuilderTest::compareTileDocuments(const QString &actualFileName, const QString &expectedFileName, int startLevel, int endLevel, bool * ok)
{
    if (!ok) {
        return;
    }

    *ok = false;
    
    QVERIFY(QFileInfo(expectedFileName).isReadable());

    TileStoreSqlite actual(actualFileName);
    TileRange actualRange;
    QCOMPARE(TileRange::loadRange(&actual.db(), &actualRange), kNoErr);

    TileStoreSqlite expected(expectedFileName);
    TileRange expectedRange;
    QCOMPARE(TileRange::loadRange(&expected.db(), &expectedRange), kNoErr);

    QCOMPARE(actualRange.tileCountX(), expectedRange.tileCountX());
    QCOMPARE(actualRange.tileCountY(), expectedRange.tileCountY());
    
    int differentTiles = 0;

    for (int level = startLevel; level <= endLevel; ++level){
        // compare at level
        
        for (int col = 0; col < actualRange.tileCountY(); col++) {
            int posY = col * Tile::HEIGHT;
            for (int row = 0; row < actualRange.tileCountX(); row++){
                int posX = row * Tile::WIDTH;
                const QPoint level2d(level, level);
                const QPoint tilePos(posX, posY);
                const Tile actualTile = actual.loadTile(level2d, tilePos);
                const Tile expectedTile = expected.loadTile(level2d, tilePos);
                
                if (actualTile != expectedTile) {
                    qWarning() << "Different tile at level" << level2d << "tile position" << tilePos;
                    differentTiles++;
                    // @note you can use saveDataToFile in Tile.h to dump to CSV
                }
            }
        }
    }

    if (differentTiles == 0) {
        *ok = true;
    } else {
        QFAIL(qPrintable(
            QString("Tile documents are different: %1 different tiles! See output at %2").arg(differentTiles).arg(actualFileName)));
    }
}

// takes around 35 seconds
void TileBuilderTest::testBuildLevel1()
{
    QElapsedTimer et;
    et.start();
    
    QString rawFilePath = m_testDataBasePath.filePath(CONA_RAW);
    const QString OUTPUT_FILEPATH = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(CONA_TILES_LEVEL_1);
    QVERIFY(QFileInfo(rawFilePath).isReadable());
    
    {
        if (QFileInfo(OUTPUT_FILEPATH).exists()) {
            QVERIFY(QFile::remove(OUTPUT_FILEPATH));
        }

        TileStoreSqlite tileStore(OUTPUT_FILEPATH);
        QCOMPARE(tileStore.createTable(), kNoErr);

        TileRange range;
        QString rangeSettingsFilePath = QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/TileBuilderTest/"  "testBuildLevel1.json");
        QVERIFY(range.loadFromFile(rangeSettingsFilePath));
        QCOMPARE(TileRange::createTable(&tileStore.db()), kNoErr);
        QCOMPARE(TileRange::saveRange(range, &tileStore.db()), kNoErr);

        ComInitializer comInitializer;
        MSReader * ms = initReader();
        QVERIFY(ms != nullptr);
        QCOMPARE(ms->openFile(rawFilePath), kNoErr);

        TileBuilder ir(range);
        ir.buildLevel1Tiles(&tileStore, ms);
        cleanUpReader(ms);
    }

    // compare with expected result
    const QString EXPECTED_FILEPATH = m_testDataBasePath.filePath(CONA_TILES_LEVEL_1);
    QVERIFY(QFileInfo(EXPECTED_FILEPATH).isReadable());
    bool ok = false;
    compareTileDocuments(OUTPUT_FILEPATH, EXPECTED_FILEPATH, 1, 1, &ok); // FIRST LEVEL only!
    if (ok) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }

    qDebug() << "Done at " << et.elapsed(); // around 32-35 seconds on Intel Core i7 @ 4GHz
}

void TileBuilderTest::testBuildTilePyramid()
{
    QElapsedTimer et;
    et.start();

    const QString inputFilePath = m_testDataBasePath.filePath(CONA_TILES_LEVEL_1);
    const QString EXPECTED_FILEPATH = m_testDataBasePath.filePath(CONA_TILES_LEVEL_1_8);
    const QString OUTPUT_FILEPATH = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(CONA_TILES_LEVEL_1_8);

    // This test takes document with level 1 data and computes the level 2..level 8 data
    // Level 1 document is copied first so that input data are not modified!
    QVERIFY(QFileInfo(inputFilePath).isReadable());
    QVERIFY(QFileInfo(EXPECTED_FILEPATH).isReadable());

    const int LAST_LEVEL = 8;

    // build pyramid
    {
        // fetch level 1 document
        if (QFileInfo(OUTPUT_FILEPATH).exists()) {
            QVERIFY(QFile::remove(OUTPUT_FILEPATH));
        }
        QVERIFY(QFile::copy(inputFilePath, OUTPUT_FILEPATH));

        TileStoreSqlite tileStore(OUTPUT_FILEPATH);
        TileRange range;
        QCOMPARE(TileRange::loadRange(&tileStore.db(), &range), kNoErr);

        TileBuilder ir(range);
        ir.buildTilePyramid(LAST_LEVEL, &tileStore);
    }

    const int START_COMPARE_LEVEL = 2;
    bool ok = false;
    compareTileDocuments(OUTPUT_FILEPATH, EXPECTED_FILEPATH, START_COMPARE_LEVEL, LAST_LEVEL, &ok);
    // clean-up, executed only when compareTileDocuments does not fail, otherwise file available for examination
    if (ok) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }

    qDebug() << "Done at " << et.elapsed(); // around 35 seconds on Intel Core i7 @ 4GHz
}


void TileBuilderTest::testBuildRipTilePyramid()
{
    const QString inputFilePath = m_testDataBasePath.filePath(CONA_TILES_LEVEL_1);
    const QString OUTPUT_FILEPATH = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(CONA_TILES_LEVEL_1_8_RIP);
    
    if (QFileInfo(OUTPUT_FILEPATH).exists()) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }
    QVERIFY(QFile::copy(inputFilePath, OUTPUT_FILEPATH));

    TileStoreSqlite tileStore(OUTPUT_FILEPATH);
    TileRange range;
    QCOMPARE(TileRange::loadRange(&tileStore.db(), &range), kNoErr);

    TileBuilder ir(range);
    ir.buildRipTilePyramid(QPoint(8, 8), &tileStore);
    //TODO: compare rendered document with reference!
}

void TileBuilderTest::testBuildLevel1fromImage()
{
    // db file
    const QString imageFilePath = m_testDataBasePath.filePath(FLOWER_IMAGE);
    const QString OUTPUT_FILEPATH = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(FLOWER_TILES);

    QVERIFY(QFileInfo(imageFilePath).isReadable());
    if (QFileInfo(OUTPUT_FILEPATH).exists()) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }

    {
        // TileStore
        TileStoreSqlite tileStore(OUTPUT_FILEPATH);
        QCOMPARE(tileStore.createTable(), kNoErr);

        // Image
        QImage img(imageFilePath); //1920x1200
        QVERIFY(!img.isNull());

        // TileRange
        TileRange range;
        const  int NULL_VALUES_IN_TILES = 25;
        const int SAMPLING_STEP_SIZE = 1;
        range.initXRange(0, img.width() - 1 + NULL_VALUES_IN_TILES, SAMPLING_STEP_SIZE);  
        range.initYRange(0, img.height() - 1 + NULL_VALUES_IN_TILES, SAMPLING_STEP_SIZE);
        range.setMinIntensity(0.0);
        range.setMaxIntensity(1.0);
        TileRange::createTable(&tileStore.db());
        TileRange::saveRange(range, &tileStore.db());

        TileBuilder ir(range);
        ir.buildLevel1Tiles(&tileStore, img); // saves the tiles*/
    }
    // compare with expected result
    const QString EXPECTED_FILEPATH = m_testDataBasePath.filePath(FLOWER_TILES);
    QVERIFY(QFileInfo(EXPECTED_FILEPATH).isReadable());
    bool ok = false;
    compareTileDocuments(OUTPUT_FILEPATH, EXPECTED_FILEPATH, 1, 1, &ok); // FIRST LEVEL only!
    if (ok) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }
    
}

void TileBuilderTest::testBuildLevel1fromImagePattern()
{
    // db file
    const QString OUTPUT_FILEPATH = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(IMAGE_PATTERN_TILES);
    if (QFileInfo(OUTPUT_FILEPATH).exists()) {
        QFile::remove(OUTPUT_FILEPATH);
    }

    const int LAST_LEVEL = 8;
    {
        // TileStore
        TileStoreSqlite tileStore(OUTPUT_FILEPATH);
        QCOMPARE(tileStore.createTable(), kNoErr);

        // TileRange
        TileRange range;
        QString rangeSettingsFilePath = QDir(QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/TileBuilderTest/")).filePath(PIXEL_TILE_RANGE_PRESET);
        QVERIFY(range.loadFromFile(rangeSettingsFilePath));
        TileRange::createTable(&tileStore.db());
        TileRange::saveRange(range, &tileStore.db());

        // Image patterns
        QRectF tileRc(0, 0, Tile::WIDTH, Tile::HEIGHT);
        QImage even(Tile::WIDTH, Tile::HEIGHT, QImage::Format_ARGB32);
        QPainter painter(&even);
        {   
            QLinearGradient topDown(0.0, 0.0, even.width(), even.height());
            topDown.setColorAt(0, Qt::black);
            topDown.setColorAt(1, Qt::white);
            QBrush evenBrush(topDown);
            painter.fillRect(tileRc, evenBrush);
            painter.end(); 
        }

        QImage odd(Tile::WIDTH, Tile::HEIGHT, QImage::Format_ARGB32);
        {
            QLinearGradient downTop(0.0, 0.0, odd.width(), odd.height());
            downTop.setColorAt(0, Qt::white);
            downTop.setColorAt(1, Qt::black);
            QBrush oddBrush(downTop);
            painter.begin(&odd);
            painter.fillRect(tileRc, oddBrush);
            painter.end(); 
        }

        TileBuilder ir(range);
        ir.buildLevel1Tiles(&tileStore, even, odd); // saves the tiles
        ir.buildTilePyramid(LAST_LEVEL, &tileStore);
    }

    // compare with expected result
    const QString EXPECTED_FILEPATH = m_testDataBasePath.filePath(IMAGE_PATTERN_TILES);
    QVERIFY(QFileInfo(EXPECTED_FILEPATH).isReadable());
    bool ok = false;
    compareTileDocuments(OUTPUT_FILEPATH, EXPECTED_FILEPATH, 1, LAST_LEVEL, &ok); // Complete document
    if (ok) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }
    
}


void TileBuilderTest::testCreateCheckerDocument()
{
    // db file
    const QString OUTPUT_FILEPATH = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath(CHECKER_PATTERN_TILES);
    if (QFileInfo(OUTPUT_FILEPATH).exists()) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }

    const int LAST_LEVEL = 8;
    {
        // TileStore
        TileStoreSqlite tileStore(OUTPUT_FILEPATH);
        QCOMPARE(tileStore.createTable(), kNoErr);

        // TileRange
        TileRange range;
        QString rangeSettingsFilePath = QDir(QFile::decodeName(PMI_TEST_FILES_DATA_DIR "/TileBuilderTest/")).filePath(PIXEL_TILE_RANGE_PRESET);
        QVERIFY(range.loadFromFile(rangeSettingsFilePath));
        TileRange::createTable(&tileStore.db());
        TileRange::saveRange(range, &tileStore.db());

        // Create time information
        ScanInfoDao dao;
        double time = 0;
        for (int y = 0; y < range.size().height(); ++y){

            int tileRow = y / Tile::HEIGHT;
            double step = pow(2, tileRow) / Tile::HEIGHT;

            ScanInfoRow row;
            row.scanIndex = y;
            row.scanNumber = y+1; // UNUSED
            row.retTimeMinutes = time;
            dao.append(row);
        
            time += step;
        }
        ScanInfoDao::createTable(&tileStore.db());
        dao.save(&tileStore.db());

        // checker pattern provider
        qreal offsetX = range.stepX() * 0.5;
        qreal offsetY = range.stepY() * 0.5;
        CheckerDataProvider checker(QSize(Tile::WIDTH, Tile::HEIGHT), offsetX, offsetY);

        TileBuilder ir(range);
        ir.buildLevel1Tiles(&tileStore, &checker);
        ir.buildTilePyramid(LAST_LEVEL, &tileStore);
    }

    // compare with expected result
    const QString EXPECTED_FILEPATH = m_testDataBasePath.filePath(CHECKER_PATTERN_TILES);
    QVERIFY(QFileInfo(EXPECTED_FILEPATH).isReadable());
    bool ok = false;
    compareTileDocuments(OUTPUT_FILEPATH, EXPECTED_FILEPATH, 1, LAST_LEVEL, &ok); // Complete document
    
    if (ok) {
        QVERIFY(QFile::remove(OUTPUT_FILEPATH));
    }
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::TileBuilderTest, QStringList() << "Remote Data Folder")

#include "TileBuilderTest.moc"
