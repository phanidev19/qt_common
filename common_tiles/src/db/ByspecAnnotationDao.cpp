#include "ByspecAnnotationDao.h"
#include <QSqlQuery>
#include "QtSqlUtils.h"

_PMI_BEGIN

Err ByspecAnnotationDao::load(QSqlDatabase * db)
{
    m_annotations.clear();

    QSqlQuery q = makeQuery(db, true);

    Err e = QEXEC_CMD(q, 
        "SELECT p.ObservedMz, s.RetentionTime/60.0 as TimeInMin \
         FROM Spectra s \
         JOIN SpectraProcessed p ON p.SpectraId = s.Id \
         WHERE p.MatchScore < 0.005 ORDER BY TImeInMin;"
        );

    if (e != kNoErr){
        qWarning() << "ByspecAnnotationDao SELECT cmd failed";
        return e;
    }

    while (q.next()) {
        
        bool ok;
        double observedMz = q.value(0).toDouble(&ok);
        Q_ASSERT(ok);
        double retentionTime = q.value(1).toDouble(&ok);
        Q_ASSERT(ok);

        m_annotations.push_back(QPointF(observedMz, retentionTime));
    }

    return e;
}

_PMI_END

