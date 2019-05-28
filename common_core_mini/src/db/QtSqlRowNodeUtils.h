/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */

#ifndef __QT_SQL_ROWNODE_UTILSH__H__
#define __QT_SQL_ROWNODE_UTILSH__H__

#include <common_errors.h>
#include <QtSqlUtils.h>
#include "RowNode.h"

/*!
 * \brief InitRootRowNode
 * \param q
 * \param xnode
 * \param insertToExisting assume to existing
 */
inline void
InitRootRowNode(QSqlQuery & q, RowNode * xnode, bool insertToExisting = false, int maxInsert=-1)
{
    if (!insertToExisting)
        xnode->clear();

    QSqlRecord rec = q.record();

    VariantItems vector;
    vector.reserve(rec.count());
    {
        for (int i = 0; i < rec.count(); i++) {
            vector << rec.fieldName(i);
        }
    }
    if (!insertToExisting)
        xnode->setup(vector, NULL);

    int insertCount = 0;
    while (q.next()) {
        VariantItems vector2;
        vector2.reserve(rec.count());
        for (int i = 0; i < rec.count(); i++) {
            vector2 << q.value(i);
        }
        RowNode * rnode = new RowNode();
        rnode->setup(vector2, xnode);
        xnode->addChild(rnode);
        if (maxInsert >= 0 && ++insertCount >= maxInsert)
            break;
    }
}

inline void extract(QSqlQuery & q, RowNode * xnode, bool insertToExisting = false, int maxInsert=-1) {
    return InitRootRowNode(q,xnode,insertToExisting,maxInsert);
}

/*!
 * \brief CollectQueryToList will collec the first item for each query into the given list
 * \param q A SELECT query exists with at least one column.
 * \param outList the output from the query
 * \param insertToExisting if false will clear the outList first.
 */
inline void
CollectQueryToList(QSqlQuery & q, QList<qlonglong> & outList, bool insertToExisting = false) {
    if (!insertToExisting)
        outList.clear();
    while(q.next()) {
        qlonglong val = q.value(0).toLongLong();
        outList.push_back(val);
    }
}

_PMI_BEGIN

inline Err
insertRowNode(QSqlQuery q, const RowNode & root, const QString & tableName) {
    Err e = kNoErr;
    QStringList header = root.getRootDataAsStringList();
    QStringList questionList;

    if (root.getChildListSize() <= 0 || header.size() <= 0)
        return e;

    for (int i = 0; i < header.size(); i++)
        questionList << "?";

    QString insert_cmd = "INSERT INTO "+tableName+"("+header.join(",")+") VALUES("+questionList.join(",")+")";
    e = QPREPARE(q, insert_cmd); eee;
    for (int i = 0; i < root.getChildListSize(); i++) {
        RowNode * row = root.getChild(i);
        for (int j = 0; j < header.size(); j++) {
            q.bindValue(j, row->getDataAt(j));
        }
        e = QEXEC_NOARG(q); eee;
    }

error:
    return e;
}

_PMI_END

#endif
