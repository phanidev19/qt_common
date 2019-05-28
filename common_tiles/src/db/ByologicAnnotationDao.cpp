#include "ByologicAnnotationDao.h"
#include <QSqlQuery>
#include "QtSqlUtils.h"

_PMI_BEGIN

Err ByologicAnnotationDao::load(QSqlDatabase * db)
{
    QSqlQuery q = makeQuery(db, true);
    Err e = QEXEC_CMD(q, R"(SELECT p.Sequence, pp.TimeStart, pp.TimeEnd, ppx.PrecursorMzStart, ppx.PrecursorMzEnd
        FROM PrimaryPeptides pp
        JOIN Peptides p ON p.Id = pp.PeptidesId
        JOIN PrimaryPeptidesXICs ppx ON ppx.PrimaryPeptidesId = pp.Id)");

    if (e != kNoErr){
        qWarning() << "ByologicAnnotationDao SELECT cmd failed";
        return e;
    }

    m_annotations.clear();
    while (q.next()) {
        bool ok;
        QString sequence = q.value(0).value<QString>();
        double timeStart = q.value(1).toDouble(&ok); Q_ASSERT(ok);
        double timeEnd = q.value(2).toDouble(&ok); Q_ASSERT(ok);
        double precursorMzStart = q.value(3).toDouble(&ok); Q_ASSERT(ok);
        double precursorMzEnd = q.value(4).toDouble(&ok); Q_ASSERT(ok);

        QRectF rect(precursorMzStart, timeStart, precursorMzEnd - precursorMzStart, timeEnd - timeStart);
        m_annotations.push_back(rect);
        m_sequences.push_back(sequence);
    }
    return e;
}

_PMI_END

