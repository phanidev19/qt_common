#ifndef BYSPEC_ANNOTATION_DAO_H
#define BYSPEC_ANNOTATION_DAO_H

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"

#include <QVector>
#include <QPointF>
#include "common_errors.h"

class QSqlDatabase;

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT ByspecAnnotationDao {

public:    
    Err load(QSqlDatabase * db);

    QVector<QPointF> data() const { return m_annotations; }

private:
    QVector<QPointF> m_annotations;

};

_PMI_END

#endif // BYSPEC_ANNOTATION_DAO_H