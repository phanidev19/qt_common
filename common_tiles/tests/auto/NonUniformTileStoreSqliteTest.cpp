#include <QtTest>
#include "pmi_core_defs.h"
#include "NonUniformTile.h"
#include "QSqlDatabase"
#include "NonUniformTileStoreSqlite.h"

#include <PmiQtStablesConstants.h>

_PMI_BEGIN

class NonUniformTileStoreSqliteTest : public QObject
{
    Q_OBJECT
public:
    NonUniformTileStoreSqliteTest();

private:
    NonUniformTile createSinTile(const QPoint& position);

private Q_SLOTS:
    void testRoundTrip();
    void testSaveEmptyTile();

private:
    QSqlDatabase m_db;
    QDir m_testOutputDir;
};

NonUniformTileStoreSqliteTest::NonUniformTileStoreSqliteTest()
{
    QString testName = "NonUniformTileStoreSqliteTest";
    m_testOutputDir = QString(PMI_TEST_FILES_OUTPUT_DIR) + "/" + testName;
    if (!m_testOutputDir.exists()) {
        m_testOutputDir.mkpath(m_testOutputDir.absolutePath());
    }
}


void NonUniformTileStoreSqliteTest::testRoundTrip()
{
    // this test tests saving and loading of tiles to NonUniformTileStoreSqlite
    // round-trip means save and check if we load what we saved
    QString filePath = m_testOutputDir.filePath("testRoundTrip.db3");
    if (QFileInfo(filePath).exists()){
        QFile::remove(filePath);
    }

    m_db = QSqlDatabase::addDatabase(kQSQLITE, QString("NonUniformTileStoreSqliteTest"));
    m_db.setDatabaseName(filePath);
    if (!m_db.open()) {
        qWarning() << "Could not open db file:" << filePath;
    }

    NonUniformTile t = createSinTile(QPoint(0, 0));
    NonUniformTileStoreSqlite store(&m_db);
    store.init();
    store.init();

    QVERIFY(store.saveTile(t, NonUniformTileStore::ContentMS1Raw));
    NonUniformTile actual = store.loadTile(t.position(), NonUniformTileStore::ContentMS1Raw);

    QCOMPARE(actual, t);

    NonUniformTile t2 = createSinTile(QPoint(1, 0));
    QVERIFY(store.saveTile(t2, NonUniformTileStore::ContentMS1Centroided));
    actual = store.loadTile(t2.position(), NonUniformTileStore::ContentMS1Centroided);
    QCOMPARE(actual, t2);

    m_db.close();
    QFile::remove(filePath);
}

void NonUniformTileStoreSqliteTest::testSaveEmptyTile()
{
    QString filePath = m_testOutputDir.filePath("testSaveEmptyTile.db3");
    if (QFileInfo(filePath).exists()) {
        QFile::remove(filePath);
    }

    m_db = QSqlDatabase::addDatabase(kQSQLITE, QString("NonUniformTileStoreSqliteTest_testSaveEmptyTile"));
    m_db.setDatabaseName(filePath);
    if (!m_db.open()) {
        qWarning() << "Could not open db file:" << filePath;
    }
    
    NonUniformTileStoreSqlite store(&m_db);
    QVERIFY(store.init());

    NonUniformTile tile;
    QPoint tilePos(0, 0);
    NonUniformTileStore::ContentType content = NonUniformTileStore::ContentMS1Centroided;
    
    tile.setPosition(tilePos);
    
    for (int i = 0; i < 4; ++i) {
        tile.append(point2dList());
    }
   
    QVERIFY(store.saveTile(tile, content));

    NonUniformTile loaded = store.loadTile(tilePos, content);
    QCOMPARE(loaded.value(0), point2dList());
    QCOMPARE(loaded.value(1), point2dList());
    QCOMPARE(loaded.value(2), point2dList());
    QCOMPARE(loaded.value(3), point2dList());

    m_db.close();
    QFile::remove(filePath);
}


pmi::NonUniformTile NonUniformTileStoreSqliteTest::createSinTile(const QPoint& position)
{
    NonUniformTile t;

    int scanNumberCount = 5;
    int maxMzCount = 20;
    qsrand(0);
    
    QVector<point2dList> tileData;
    for (int i = 0; i < scanNumberCount; ++i) {
        int mzSize = qrand() % maxMzCount;
        point2dList mzIntensity;
        for (int j = 0; j < mzSize; ++j) {
            mzIntensity.push_back( QPointF(j, sin(j)) );
        }
        tileData.push_back(mzIntensity);
    }

    t.setPosition(position);
    t.setData(tileData);
    return t;
}

_PMI_END

QTEST_MAIN(pmi::NonUniformTileStoreSqliteTest)

#include "NonUniformTileStoreSqliteTest.moc"
