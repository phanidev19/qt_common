/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#ifndef FINDER_SAMPLES_DAO_H
#define FINDER_SAMPLES_DAO_H

#include "pmi_common_ms_export.h"

#include <common_errors.h>
#include <pmi_core_defs.h>

#include <QList>

class QSqlDatabase;

_PMI_BEGIN

class FinderSamplesDao {

public:    
    explicit FinderSamplesDao(QSqlDatabase *db);
    
    Err createTable() const;

    Err insertSamples(const QStringList &sampleNames) const;

    Err samplesId(const QString &sampleFilePath, int *sampleId) const;

    Err uniqueIds(QList<int> *ids) const;

private:
    QSqlDatabase *m_db;
};

_PMI_END

#endif // FINDER_SAMPLES_DAO_H