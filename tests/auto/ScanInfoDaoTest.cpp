#include <QtTest>
#include "pmi_core_defs.h"

#include <ScanInfoDao.h>
#include <QSqlDatabase>
#include "MSReader.h"

#include <PmiQtStablesConstants.h>

_PMI_BEGIN

const QString TEST_DB_FILENAME = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR "/scanInfoDaoTest.db3");
// TODO:
const QString TEST_RAW_FILENAME = QFile::decodeName("C:\\work\\pmi\\src\\qt_apps_libs\\common_ms\\tests\\auto\\data\\cona_tmt0saxpdetd.raw");

class ScanInfoDaoTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase();
    void cleanupTestCase();
    
    void testSaveLoad();
    void testImport();

    void cleanup();

private:
    QSqlDatabase m_db;

};

void ScanInfoDaoTest::testSaveLoad()
{
    ScanInfoRow row;
    row.scanIndex = 0;
    row.scanNumber = 1;
    row.retTimeMinutes = 0.02;
    
    ScanInfoDao daoSave;
    daoSave.append(row);
    QCOMPARE(daoSave.save(&m_db), kNoErr);

    ScanInfoDao daoLoad;
    QCOMPARE(daoLoad.load(&m_db), kNoErr);

    std::vector<ScanInfoRow> scanInfoList = daoLoad.data();
    QCOMPARE(scanInfoList.size(), size_t(1));
    QCOMPARE(scanInfoList.at(0), row);

}


void ScanInfoDaoTest::testImport()
{
    msreader::ScanInfoWrapper item;
    item.scanInfo.retTimeMinutes = 0.02;
    item.scanNumber = 1;

    QList<msreader::ScanInfoWrapper> lockmassList;
    lockmassList.append(item);
    
    ScanInfoDao dao;
    dao.import(lockmassList, &m_db);

    ScanInfoDao daoLoad;
    QCOMPARE(daoLoad.load(&m_db), kNoErr);

    ScanInfoRow row;
    row.scanIndex = 0;
    row.scanNumber = item.scanNumber;
    row.retTimeMinutes = item.scanInfo.retTimeMinutes;

    std::vector<ScanInfoRow> scanInfoList = daoLoad.data();
    QCOMPARE(scanInfoList.size(), size_t(1));
    QCOMPARE(scanInfoList.at(0), row);

}

void ScanInfoDaoTest::cleanup()
{
    ScanInfoDao::clearTable(&m_db);
}

void ScanInfoDaoTest::initTestCase()
{
    m_db = QSqlDatabase::addDatabase(kQSQLITE, "ScanInfoDaoTest");
    m_db.setDatabaseName(TEST_DB_FILENAME);

    if (!m_db.open()) {
        qWarning() << "Could not open db file:" << TEST_DB_FILENAME;
    }

    QCOMPARE(ScanInfoDao::createTable(&m_db), kNoErr);
}

void ScanInfoDaoTest::cleanupTestCase()
{
    QCOMPARE(ScanInfoDao::dropTable(&m_db), kNoErr);
    m_db.close();
    QFile::remove(TEST_DB_FILENAME);
}



_PMI_END

QTEST_MAIN(pmi::ScanInfoDaoTest)

#include "ScanInfoDaoTest.moc"
