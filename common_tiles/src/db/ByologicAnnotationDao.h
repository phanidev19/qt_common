#ifndef BYOLOGIC_ANNOTATION_DAO
#define BYOLOGIC_ANNOTATION_DAO

#include "pmi_core_defs.h"
#include "pmi_common_tiles_export.h"

#include <QVector>
#include <QRectF>
#include "common_errors.h"

class QSqlDatabase;

_PMI_BEGIN

class PMI_COMMON_TILES_EXPORT ByologicAnnotationDao 
{
public:
    Err load(QSqlDatabase * db);

    QVector<QRectF> data() const { return m_annotations; }
    QVector<QString> sequences() const { return m_sequences; }
private:
    QVector<QRectF> m_annotations;
    QVector<QString> m_sequences;
};

_PMI_END

#endif // BYOLOGIC_ANNOTATION_DAO