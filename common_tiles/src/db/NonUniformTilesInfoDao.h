/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef NON_UNIFORM_TILES_INFO_DAO_H
#define NON_UNIFORM_TILES_INFO_DAO_H

#include "pmi_core_defs.h"
#include "common_errors.h"
#include "pmi_common_tiles_export.h"

class QSqlDatabase;

_PMI_BEGIN

class NonUniformTileRange;

class PMI_COMMON_TILES_EXPORT NonUniformTilesInfoDao {

public:    
    explicit NonUniformTilesInfoDao(QSqlDatabase * db);
    
    Err createTable();
    Err dropTable();

    //TODO: rename range to info
    Err save(const NonUniformTileRange &range);
    Err load(NonUniformTileRange * range);

private:
    QSqlDatabase * m_db;

};

_PMI_END

#endif // NON_UNIFORM_TILES_INFO_DAO_H