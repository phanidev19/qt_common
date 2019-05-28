/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MODIFICATIONINFOMODEL_H
#define MODIFICATIONINFOMODEL_H

#include "FineControl.h"
#include "UnimodModifications.h"

#include <QAbstractItemModel>
#include <QList>
#include <QSharedPointer>
#include <QSize>
#include <QVector>

class ExtendedAminoAcid
{
public:
    QString m_aminoAcid;
    bool m_nGlyscan = false;

    ExtendedAminoAcid();
    ExtendedAminoAcid(const QString &aminoAcid, bool nGlyscan = false);

    QString toComboBoxDisplayString() const;
    QString toByonicString() const;
};

class Target
{
public:
    UnimodModifications::UmodPosition m_position;
    ExtendedAminoAcid m_extendedAminoAcid;
    bool isActive = false;

    Target();
    Target(UnimodModifications::UmodPosition position, const ExtendedAminoAcid &extendedAminoAcid);

    static QString getComboBoxDisplayString(UnimodModifications::UmodPosition position);
    static QStringList getValidByonicStrings(UnimodModifications::UmodPosition position);
    static QString getPreferredByonicString(UnimodModifications::UmodPosition position);

    static int getOrderPosition(UnimodModifications::UmodPosition position);

    QString toComboBoxDisplayString() const;
    QStringList getValidByonicStrings() const;
    QString toByonicString() const;

    bool operator<(const Target &other) const;
};

class ModificationInfoModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ModificationInfoModel(QSharedPointer<UnimodModifications> modifications,
                          bool proteomeDiscoverer_2_0, bool proteomeDiscoverer_1_4,
                          QObject *parent = nullptr);
    virtual ~ModificationInfoModel();

    bool inverted() const;
    Q_SLOT void setInverted(bool inverted = true);

    QModelIndex modificationParentIndex() const;
    QModelIndex targetParentIndex() const;
    QModelIndex fineParentIndex() const;
    QModelIndex modMaxParentIndex() const;
    QModelIndex attributeParentIndex() const;

    static ModificationInfoModel *
    createFromString(QSharedPointer<UnimodModifications> modifications, bool proteomeDiscoverer_2_0,
                     bool proteomeDiscoverer_1_4, const QString &str);
    QString toByonicString() const;

    bool isCompleted() const;

    virtual int rowCount(const QModelIndex &parent) const override;
    virtual int columnCount(const QModelIndex &parent) const override;
    virtual QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    virtual Qt::ItemFlags flags(const QModelIndex &index) const override;

    virtual QModelIndex index(int row, int column,
                              const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &child) const override;

    virtual bool setData(const QModelIndex &index, const QVariant &value,
                         int role = Qt::EditRole) override;

Q_SIGNALS:
    void modelIsCompleted();

private:
    QString toString(const UnimodModifications::UmodMod &umod, bool byonicFormat) const;
    QString getStatusAdjustedName(const QString &name) const;
    QString getStatusAdjustedNameByonic(const QString &name) const;
    double getStatusAdjustedMonoisotopicDeltaMass(double mass) const;

    void setTargetIsActive(int targetIndex, bool isActive);

    void updatePredefinedModifications();
    void updateTargetsList();

    void checkIsCompleted();

private:
    bool m_proteomeDiscoverer_2_0 = false;
    bool m_proteomeDiscoverer_1_4 = false;

    bool m_inverted = false;

    int m_currentModificationIndex = -1;
    QString m_currentModificationStr;
    QVector<Target> m_currentTargets;
    FineControl::FineType m_currentFine = FineControl::defaultFine;
    int m_currentModMax = 0;
    QString m_attribute;

    QSharedPointer<UnimodModifications> m_modifications;
    QList<QPair<QString, int>> m_predefinedModifications;

    const QVector<FineControl::FineType> m_allFines;
    const QVector<int> m_allModMax;
};

#endif // MODIFICATIONINFOMODEL_H
