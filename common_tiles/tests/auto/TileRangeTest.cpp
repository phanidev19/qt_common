#include <QtTest>
#include "pmi_core_defs.h"
#include "TileRange.h"
#include <QSqlDatabase>
#include <QDebug>

#include <PmiQtStablesConstants.h>

_PMI_BEGIN

class TileRangeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    
    void testInitRange();

    void testRoundTrip();
    void testLevelWidth();

    void testHeight_data();
    void testHeight();

    void testTileCount();


private:
    QSqlDatabase m_db;

};

void TileRangeTest::initTestCase()
{
    m_db = QSqlDatabase::addDatabase(kQSQLITE, "TileRangeTest");
    QString dbFileName = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/tileStore.db3");
    m_db.setDatabaseName(dbFileName);
    
    if (!m_db.open()) {
        qWarning() << "Could not open db file:" << dbFileName;
    }

    QCOMPARE(TileRange::createTable(&m_db), kNoErr);
}

void TileRangeTest::cleanupTestCase()
{
    TileRange::dropTable(&m_db);
    m_db.close();
}


void TileRangeTest::testInitRange()
{
    TileRange range;

    range.initXRange(0.0, 2.0, 0.5);
    QCOMPARE(range.size().width(), 5);

    // same inputs thus expecting same step
    range.initXRangeBySize(0.0, 2.0, 5);
    QCOMPARE(range.stepX(), 0.5);
    
    // -- //

    range.initYRange(0, 2, 1);
    QCOMPARE(range.size().height(), 3);
    
    // same inputs thus expecting same step
    range.initYRangeBySize(0, 2, 3);
    QCOMPARE(range.stepY(), 1);

}

void TileRangeTest::testRoundTrip()
{
    TileRange rn;
    int sizeX = 20000;
    qreal minX = 380.001;
    qreal maxX = minX + 400.0;

    rn.initXRangeBySize(minX, maxX, sizeX);
    
    int minY = 0;
    int stepY = 1;
    int maxY = 2751;
    rn.initYRange(minY, maxY, stepY);
    
    //qDebug() << "Expected" << rn;

    TileRange::saveRange(rn, &m_db);
    TileRange actual;
    TileRange::loadRange(&m_db, &actual);

    //qDebug() << actual;

    QCOMPARE(rn, actual);

}

void TileRangeTest::testLevelWidth()
{
    TileRange tr;
    tr.setSize(QSize(10000, 2752));

    QVector<int> expectedWidth{ 10000, 5000, 2500, 1250, 625, 313 };

    for (int i = 1; i < 7; ++i)
    {
        int w = tr.levelWidth(QPoint(i,i));
        QCOMPARE(w, expectedWidth.at(i-1));
    }
    

}

void TileRangeTest::testHeight_data()
{
    QTest::addColumn<int>("minY");
    QTest::addColumn<int>("maxY");
    QTest::addColumn<int>("stepY");
    QTest::addColumn<int>("expected");

    QTest::newRow("Step-One-One-To-Ten") << 0 << 9 << 1 << 10;
    QTest::newRow("Step-One") << 0 << 2752 << 1 << 2753;
}

void TileRangeTest::testHeight()
{
    QFETCH(int, minY);
    QFETCH(int, maxY);
    QFETCH(int, stepY);
    QFETCH(int, expected);

    TileRange tr;
    tr.initYRange(minY, maxY, stepY);
    QCOMPARE(tr.size().height(), expected);
}

void TileRangeTest::testTileCount()
{
    TileRange tr;
    tr.setSize(QSize(7, 0));
    QCOMPARE(tr.tileCountX(), 1);

    tr.setSize(QSize(64, 0));
    QCOMPARE(tr.tileCountX(), 1);

    tr.setSize(QSize(65, 0));
    QCOMPARE(tr.tileCountX(), 2);

    tr.setSize(QSize(128, 0));
    QCOMPARE(tr.tileCountX(), 2);


}

_PMI_END

QTEST_MAIN(pmi::TileRangeTest)

#include "TileRangeTest.moc"
