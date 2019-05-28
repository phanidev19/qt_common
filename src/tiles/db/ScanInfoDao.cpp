#include "ScanInfoDao.h"

#include "QtSqlUtils.h"
#include "common_errors.h"
#include <QSqlQuery>

_PMI_BEGIN

Err ScanInfoDao::createTable(QSqlDatabase * db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);

    e = QEXEC_CMD(q, "CREATE TABLE IF NOT EXISTS ScanInfo (\
                             ScanIndex INT NOT NULL, \
                             ScanNumber	INT NOT NULL, \
                             RetTimeMinutes	REAL NOT NULL \
                             )"); ree;
    
    return e;
}

Err ScanInfoDao::dropTable(QSqlDatabase * db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, "DROP TABLE IF EXISTS ScanInfo;"); ree;
    return e;
}



Err ScanInfoDao::clearTable(QSqlDatabase * db)
{
    Err e = kNoErr;
    QSqlQuery q = makeQuery(db, true);
    e = QEXEC_CMD(q, "DELETE FROM ScanInfo;"); ree;
    return e;
}

Err ScanInfoDao::import(const QList<msreader::ScanInfoWrapper> &scanInfoList, QSqlDatabase * db)
{
    ScanInfoDao dao;
    for (int i = 0; i < scanInfoList.size(); ++i) {
        ScanInfoRow row;
        row.scanIndex = i;
        row.scanNumber = scanInfoList.at(i).scanNumber;
        row.retTimeMinutes = scanInfoList.at(i).scanInfo.retTimeMinutes;
        dao.append(row);
    }

    return dao.save(db);
}



Err ScanInfoDao::save(QSqlDatabase * db)
{
    bool transactionStarted = db->transaction();
    Q_ASSERT(transactionStarted);
    
    Err e = kNoErr;
    bool finished = true;
    QSqlQuery q = makeQuery(db, true);

    //Note: prepare is done once and re-used
    e = QPREPARE(q, "INSERT INTO ScanInfo(ScanIndex, ScanNumber, RetTimeMinutes) \
                                   VALUES(:ScanIndex, :ScanNumber, :RetTimeMinutes);"); ree;

    for (const ScanInfoRow &row : m_scanInfoList) {
        q.bindValue(":ScanIndex", row.scanIndex);
        q.bindValue(":ScanNumber", row.scanNumber);
        q.bindValue(":RetTimeMinutes", row.retTimeMinutes);

        e = QEXEC_NOARG(q);

        if (e != kNoErr) {
            finished = false;
            break;
        }
    }
    
    finished ? db->commit() : db->rollback();

    e = QEXEC_CMD(q, "CREATE INDEX IF NOT EXISTS index_ScanInfoScanNumber ON ScanInfo(ScanNumber)"); ree;

    return e;
}

Err ScanInfoDao::load(QSqlDatabase * db)
{
    Err e = kNoErr;
    m_scanInfoList.clear();
    
    QSqlQuery q = makeQuery(db, true);

    e = QEXEC_CMD(q,
             "SELECT ScanIndex, ScanNumber, RetTimeMinutes \
              FROM ScanInfo \
              ORDER BY ScanNumber;"
            );
    
    if (e != kNoErr) {
        qWarning() << "ScanInfo SELECT cmd failed";
        return e;
    }
    //Note: ORDER can get expensive unless we create index.
    //      Also, scannumber and retime both are in same order (should be)
    //ORDER BY ScanIndex, ScanNumber, RetTimeMinutes;");

    while (q.next()) {
        ScanInfoRow row;
        row.scanIndex = q.value(0).toInt();
        row.scanNumber = q.value(1).toInt();
        row.retTimeMinutes = q.value(2).toDouble();

        m_scanInfoList.push_back(row);
    }

    return e;
}

void ScanInfoDao::append(const ScanInfoRow &row)
{
    m_scanInfoList.push_back(row);
}


_PMI_END
