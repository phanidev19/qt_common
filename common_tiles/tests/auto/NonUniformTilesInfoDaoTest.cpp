#include <QtTest>
#include "pmi_core_defs.h"

#include "NonUniformTileRange.h"
#include "NonUniformTilesInfoDao.h"
#include <QSqlDatabase>

#include <PmiQtStablesConstants.h>

_PMI_BEGIN

class NonUniformTilesInfoDaoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testRoundTrip();

private:
    QSqlDatabase m_db;
};

void NonUniformTilesInfoDaoTest::testRoundTrip()
{
    // this test tests Dao object https://en.wikipedia.org/wiki/Data_access_object
    // round-trip data are saved and than we check if we load what we saved
    QString filePath = QDir(PMI_TEST_FILES_OUTPUT_DIR).filePath("nutidt_testRoundTrip.db3");
    m_db = QSqlDatabase::addDatabase(kQSQLITE, QString("NonUniformTilesInfoDaoTest"));
    m_db.setDatabaseName(filePath);
    if (!m_db.open()) {
        qWarning() << "Could not open db file:" << filePath;
    }

    NonUniformTilesInfoDao dao(&m_db);
    QCOMPARE(dao.createTable(), kNoErr);
    QCOMPARE(dao.dropTable(), kNoErr);
    QCOMPARE(dao.createTable(), kNoErr);

    NonUniformTileRange expected;
    expected.setMz(197.07627868652344, 1499.9744873046875);
    expected.setMzTileLength(30.0);

    expected.setScanIndex(0, 1000);
    expected.setScanIndexLength(64);

    QCOMPARE(dao.save(expected), kNoErr);

    NonUniformTileRange actual;
    QCOMPARE(dao.load(&actual), kNoErr);

    QCOMPARE(actual, expected);

    m_db.close();
    QFile::remove(filePath);
}

_PMI_END

QTEST_MAIN(pmi::NonUniformTilesInfoDaoTest)

#include "NonUniformTilesInfoDaoTest.moc"
