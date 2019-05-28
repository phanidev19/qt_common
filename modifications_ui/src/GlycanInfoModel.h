/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef GLYCANINFOMODEL_H
#define GLYCANINFOMODEL_H

#include "FineControl.h"
#include "UnimodModifications.h"

#include <QAbstractItemModel>
#include <QList>
#include <QSharedPointer>
#include <QSize>
#include <QVector>

class GlycanInfoModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    GlycanInfoModel(bool proteomeDiscoverer_2_0, bool proteomeDiscoverer_1_4, bool dbMode,
                    QObject *parent = nullptr);
    virtual ~GlycanInfoModel();

    void setGlycanDbFiles(QList<QPair<QString, QString>> &glycanDbFiles);
    bool dbMode() const;

    QModelIndex glycanTypeParentIndex() const;
    QModelIndex glycanDbParentIndex() const;
    QModelIndex glycanParentIndex() const;
    QModelIndex fineParentIndex() const;
    QModelIndex modMaxParentIndex() const;

    static GlycanInfoModel *createFromString(bool proteomeDiscoverer_2_0,
                                             bool proteomeDiscoverer_1_4, const QString &str);
    QString toByonicString() const;

    bool isCompleted() const;

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    virtual QModelIndex index(int row, int column,
                              const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value,
                         int role = Qt::EditRole) override;

Q_SIGNALS:
    void modelIsCompleted();

private:
    void tryFindFileInDb();

    void checkIsCompleted();

private:
    bool m_proteomeDiscoverer_2_0 = false;
    bool m_proteomeDiscoverer_1_4 = false;

    int m_currentGlycanTypeIndex = -1;

    bool m_dbMode = false;
    int m_currentGlycanDbIndex = -1;
    QString m_currentGlycanDbStr;
    QString m_currentGlycan;

    FineControl::FineType m_currentFine = FineControl::defaultFine;
    int m_currentModMax = 0;

    const QVector<FineControl::FineType> m_allFines;
    const QVector<int> m_allModMax;

    QList<QPair<QString, QString>> m_glycanDbFiles;
};

#endif // GLYCANINFOMODEL_H
