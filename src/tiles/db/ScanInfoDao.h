#ifndef SCAN_INFO_DAO_H
#define SCAN_INFO_DAO_H

#include "pmi_core_defs.h"
#include "pmi_common_ms_export.h"

#include "common_errors.h"

#include "MSReaderBase.h"

class QSqlDatabase;

_PMI_BEGIN

class PMI_COMMON_MS_EXPORT ScanInfoRow {
public:
    int scanIndex;
    int scanNumber;
    double retTimeMinutes;

    bool operator==(const ScanInfoRow& rhs) const {
        return rhs.scanIndex == scanIndex &&
               rhs.scanNumber == scanNumber &&
               rhs.retTimeMinutes == retTimeMinutes;
    }


};

class PMI_COMMON_MS_EXPORT ScanInfoDao {

public:    
    static Err createTable(QSqlDatabase * db);
    static Err dropTable(QSqlDatabase * db);
    static Err clearTable(QSqlDatabase * db);


    static Err import(const QList<msreader::ScanInfoWrapper> &scanInfoList, QSqlDatabase * db);
    
    Err save(QSqlDatabase * db);
    Err load(QSqlDatabase * db);

    void append(const ScanInfoRow &row);

    std::vector<ScanInfoRow> data() const {
        return m_scanInfoList;
    }

private:
    std::vector<ScanInfoRow> m_scanInfoList;

};

_PMI_END

#endif // SCAN_INFO_DAO_H