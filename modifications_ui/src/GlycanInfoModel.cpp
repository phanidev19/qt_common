/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "GlycanInfoModel.h"

#include "GlycanEditorDialog.h"
#include "pmi_modifications_ui_debug.h"

#include <QDir>

static const quintptr INVALID_INDEX = -1;
static const quintptr GLYCAN_TYPE_INDEX = 0;
static const quintptr GLYCAN_DB_INDEX = 1;
static const quintptr GLYCAN_INDEX = 2;
static const quintptr FINE_INDEX = 3;
static const quintptr MOD_MAX_INDEX = 4;
static const int LAST_PARENT_INDEX = 4;

static int findModificationIndexByTitle(const UnimodModifications &modifications,
                                        const QString &title)
{
    const QVector<UnimodModifications::UmodMod> &modList = modifications.modifications();
    for (int i = 0; i < modList.count(); ++i) {
        if (QString::compare(modList[i].title, title, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

struct GlycanType {
    QString byonicString;
    QString uiString;
};

static const QList<GlycanType> s_allGlycanTypes
    = { { QStringLiteral("NGlycan"), QStringLiteral("N-Glycan") },
        { QStringLiteral("OGlycan"), QStringLiteral("O-Glycan") } };

GlycanInfoModel::GlycanInfoModel(bool proteomeDiscoverer_2_0, bool proteomeDiscoverer_1_4,
                                 bool dbMode, QObject *parent)
    : QAbstractItemModel(parent)
    , m_proteomeDiscoverer_2_0(proteomeDiscoverer_2_0)
    , m_proteomeDiscoverer_1_4(proteomeDiscoverer_1_4)
    , m_currentGlycanTypeIndex(-1)
    , m_dbMode(dbMode)
    , m_currentGlycanDbIndex(-1)
    , m_currentModMax(0)
    , m_allFines({ FineControl::FineType::Rare1, FineControl::FineType::Rare2,
                   FineControl::FineType::Common1, FineControl::FineType::Common2,
                   FineControl::FineType::CommonGreaterThan2 })
    , m_allModMax({ 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 })
{
    m_currentFine = m_allFines[0];
}

GlycanInfoModel::~GlycanInfoModel()
{
}

void GlycanInfoModel::setGlycanDbFiles(QList<QPair<QString, QString>> &glycanDbFiles)
{
    if (m_glycanDbFiles == glycanDbFiles) {
        return;
    }

    if (m_currentGlycanDbIndex >= 0) {
        m_currentGlycanDbStr = m_glycanDbFiles[m_currentGlycanDbIndex].first;
        m_currentGlycanDbIndex = -1;
    }

    if (m_glycanDbFiles.count() > 0) {
        Q_EMIT beginRemoveRows(glycanDbParentIndex(), 0, m_glycanDbFiles.count() - 1);
        m_glycanDbFiles.clear();
        Q_EMIT endRemoveRows();
    }

    if (glycanDbFiles.count() > 0) {
        Q_EMIT beginInsertRows(glycanDbParentIndex(), 0, glycanDbFiles.count() - 1);
        m_glycanDbFiles = glycanDbFiles;
        Q_EMIT endInsertRows();
    }

    tryFindFileInDb();

    Q_EMIT dataChanged(glycanDbParentIndex(), glycanDbParentIndex(),
                       QVector<int>({ Qt::DisplayRole }));
}

bool GlycanInfoModel::dbMode() const
{
    return m_dbMode;
}

bool GlycanInfoModel::isCompleted() const
{
    if (m_currentGlycanTypeIndex < 0) {
        return false;
    }

    if (m_dbMode) {
        return m_currentGlycanDbIndex >= 0 || !m_currentGlycanDbStr.isEmpty();
    }

    // if !dbMode
    return GlycanEditorDialog::isGlycanValid(m_currentGlycan);
}

void GlycanInfoModel::tryFindFileInDb()
{
    if (m_currentGlycanDbIndex >= 0) {
        return;
    }
    const QFileInfo currentFile(m_currentGlycanDbStr);
    for (int i = 0; i < m_glycanDbFiles.count(); ++i) {
        if (QFileInfo(m_glycanDbFiles[i].first) == currentFile) {
            m_currentGlycanDbIndex = i;
            m_currentGlycanDbStr.clear();
            return;
        }
    }
}

void GlycanInfoModel::checkIsCompleted()
{
    if (isCompleted()) {
        Q_EMIT modelIsCompleted();
    }
}

QModelIndex GlycanInfoModel::glycanTypeParentIndex() const
{
    return createIndex(0, GLYCAN_TYPE_INDEX, INVALID_INDEX);
}

QModelIndex GlycanInfoModel::glycanDbParentIndex() const
{
    return createIndex(0, GLYCAN_DB_INDEX, INVALID_INDEX);
}

QModelIndex GlycanInfoModel::glycanParentIndex() const
{
    return createIndex(0, GLYCAN_INDEX, INVALID_INDEX);
}

QModelIndex GlycanInfoModel::fineParentIndex() const
{
    return createIndex(0, FINE_INDEX, INVALID_INDEX);
}

QModelIndex GlycanInfoModel::modMaxParentIndex() const
{
    return createIndex(0, MOD_MAX_INDEX, INVALID_INDEX);
}

QString GlycanInfoModel::toByonicString() const
{
    static const QString modAndTargetsSeparator = QStringLiteral(" @ ");
    static const QString typeAndFineSeparator = QStringLiteral(" | ");

    if (m_currentGlycanTypeIndex < 0) {
        return QString();
    }

    QString result;

    if (m_dbMode) {
        static const QString filePrintMask = QStringLiteral("[file:///%0]");
        if (m_currentGlycanDbIndex < 0) {
            result += filePrintMask.arg(QDir::toNativeSeparators(m_currentGlycanDbStr));
        } else {
            result += filePrintMask.arg(
                QDir::toNativeSeparators(m_glycanDbFiles[m_currentGlycanDbIndex].first));
        }
    } else {
        result += m_currentGlycan;
    }

    result += modAndTargetsSeparator;
    result += s_allGlycanTypes[m_currentGlycanTypeIndex].byonicString;
    result += typeAndFineSeparator;
    result += FineControl::toByonicString(m_currentFine, m_currentModMax);

    return result;
}

QModelIndex GlycanInfoModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent == glycanTypeParentIndex()) {
        return createIndex(row, column, GLYCAN_TYPE_INDEX);
    }
    if (parent == glycanDbParentIndex()) {
        return createIndex(row, column, GLYCAN_DB_INDEX);
    }
    if (parent == glycanParentIndex()) {
        return createIndex(row, column, GLYCAN_INDEX);
    }
    if (parent == fineParentIndex()) {
        return createIndex(row, column, FINE_INDEX);
    }
    if (parent == modMaxParentIndex()) {
        return createIndex(row, column, MOD_MAX_INDEX);
    }
    return createIndex(row, column, INVALID_INDEX);
}

QModelIndex GlycanInfoModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.model() != this) {
        return QModelIndex();
    }
    switch (child.internalId()) {
    case GLYCAN_TYPE_INDEX:
        return glycanTypeParentIndex();
    case GLYCAN_DB_INDEX:
        return glycanDbParentIndex();
    case GLYCAN_INDEX:
        return glycanParentIndex();
    case FINE_INDEX:
        return fineParentIndex();
    case MOD_MAX_INDEX:
        return modMaxParentIndex();
    default:
        return QModelIndex();
    }
    return QModelIndex();
}

int GlycanInfoModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return 1;
    }
    if (parent == glycanTypeParentIndex()) {
        return s_allGlycanTypes.count();
    }
    if (parent == glycanDbParentIndex()) {
        return m_glycanDbFiles.count();
    }
    if (parent == glycanParentIndex()) {
        return 0;
    }
    if (parent == fineParentIndex()) {
        return m_allFines.count();
    }
    if (parent == modMaxParentIndex()) {
        return m_allModMax.count();
    }
    return 0;
}

int GlycanInfoModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return LAST_PARENT_INDEX;
    }
    return 1;
}

QVariant GlycanInfoModel::data(const QModelIndex &index, int role) const
{
    // glycan type combo state
    if (index == glycanTypeParentIndex()) {
        if (role == Qt::UserRole) {
            return m_currentGlycanTypeIndex;
        }
    }

    // glycan type combobox contents
    if (index.parent() == glycanTypeParentIndex()) {
        if (index.row() < 0 || index.row() >= s_allGlycanTypes.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return s_allGlycanTypes[index.row()].uiString;
        }
        if (role == Qt::UserRole) {
            return index.row();
        }
        return QVariant();
    }

    // glycan db combo state
    if (index == glycanDbParentIndex()) {
        if (role == Qt::UserRole) {
            if (m_currentGlycanDbIndex >= 0) {
                return m_currentGlycanDbIndex;
            } else if (!m_currentGlycanDbStr.isEmpty()) {
                return m_currentGlycanDbStr;
            }
            return QVariant();
        }
    }

    // modification combobox contents
    if (index.parent() == glycanDbParentIndex()) {
        if (index.row() < 0 || index.row() >= m_glycanDbFiles.count()) {
            return QVariant();
        }

        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            return m_glycanDbFiles[index.row()].second;
        }
        if (role == Qt::ToolTipRole) {
            return m_glycanDbFiles[index.row()].second + QStringLiteral("\n")
                + m_glycanDbFiles[index.row()].first;
        }
        if (role == Qt::UserRole) {
            return index.row();
        }
        return QVariant();
    }

    // glycan data
    if (index == glycanParentIndex()) {
        if (role == Qt::DisplayRole || role == Qt::EditRole || role == Qt::UserRole) {
            return m_currentGlycan;
        }
        return QVariant();
    }

    // fine combobox contents
    if (index.parent() == fineParentIndex()) {
        if (index.row() < 0 || index.row() >= m_allFines.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return FineControl::toComboBoxDisplayString(m_allFines[index.row()],
                                                        m_proteomeDiscoverer_2_0);
        }
        if (role == Qt::UserRole) {
            return static_cast<int>(m_allFines[index.row()]);
        }
        return QVariant();
    }

    // fine combobox data
    if (index == fineParentIndex()) {
        if (role == Qt::UserRole) {
            return static_cast<int>(m_currentFine);
        }
        return QVariant();
    }

    // modMax combobox contents
    if (index.parent() == modMaxParentIndex()) {
        if (index.row() < 0 || index.row() >= m_allModMax.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return QString::number(m_allModMax[index.row()]);
        }
        if (role == Qt::UserRole) {
            return m_allModMax[index.row()];
        }
        return QVariant();
    }

    // modMax combobox data
    if (index == modMaxParentIndex()) {
        if (role == Qt::UserRole) {
            return m_currentModMax;
        }
        return QVariant();
    }

    return QVariant();
}

bool GlycanInfoModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (role != Qt::UserRole) {
        return false;
    }
    if (index == glycanTypeParentIndex()) {
        if (value.type() != QVariant::Int) {
            return false;
        }
        const int glycanTypeIndex = value.toInt();
        if (m_currentGlycanTypeIndex == glycanTypeIndex) {
            return true;
        }
        if (glycanTypeIndex < 0 || glycanTypeIndex >= rowCount(glycanTypeParentIndex())) {
            return false;
        }
        m_currentGlycanTypeIndex = glycanTypeIndex;
        Q_EMIT dataChanged(glycanTypeParentIndex(), glycanTypeParentIndex(),
                           QVector<int>({ Qt::UserRole }));
        checkIsCompleted();
        return true;
    }

    if (index == glycanDbParentIndex()) {
        if (value.type() == QVariant::String) {
            const QString newGlycanDb = value.toString().trimmed();
            if (m_currentGlycanDbStr == newGlycanDb) {
                return true;
            }
            m_currentGlycanDbStr = newGlycanDb;
            m_currentGlycanDbIndex = -1;
            Q_EMIT dataChanged(glycanDbParentIndex(), glycanDbParentIndex());
            checkIsCompleted();
            return true;
        }
        if (value.type() == QVariant::Int) {
            const int newGlycanDb = value.toInt();
            if (newGlycanDb < 0 || newGlycanDb >= m_glycanDbFiles.count()) {
                return false;
            }
            if (m_currentGlycanDbIndex == newGlycanDb) {
                return true;
            }
            m_currentGlycanDbStr.clear();
            m_currentGlycanDbIndex = newGlycanDb;
            Q_EMIT dataChanged(glycanDbParentIndex(), glycanDbParentIndex());
            checkIsCompleted();
            return true;
        }
    }

    if (index == glycanParentIndex()) {
        if (value.type() != QVariant::String) {
            return false;
        }
        const QString newGlycan = value.toString().trimmed();
        if (m_currentGlycan == newGlycan) {
            return true;
        }
        m_currentGlycan = newGlycan;
        Q_EMIT dataChanged(glycanParentIndex(), glycanParentIndex(),
                           QVector<int>({ Qt::UserRole }));
        checkIsCompleted();
        return true;
    }

    if (index == fineParentIndex()) {
        if (value.type() != QVariant::Int) {
            return false;
        }
        if (!FineControl::isValid(value.toInt())) {
            return false;
        }
        FineControl::FineType fine = FineControl::fromInt(value.toInt());
        if (m_currentFine == fine) {
            return true;
        }
        m_currentFine = fine;
        Q_EMIT dataChanged(fineParentIndex(), fineParentIndex(), QVector<int>({ Qt::UserRole }));
        checkIsCompleted();
        return true;
    }

    if (index == modMaxParentIndex()) {
        if (value.type() != QVariant::Int) {
            return false;
        }
        const int modMaxValue = value.toInt();
        if (m_currentModMax == modMaxValue) {
            return true;
        }
        if (modMaxValue < m_allModMax.first() || modMaxValue > m_allModMax.last()) {
            return false;
        }
        m_currentModMax = modMaxValue;
        Q_EMIT dataChanged(modMaxParentIndex(), modMaxParentIndex(),
                           QVector<int>({ Qt::UserRole }));
        checkIsCompleted();
        return true;
    }
    return false;
}

GlycanInfoModel *GlycanInfoModel::createFromString(bool proteomeDiscoverer_2_0,
                                                   bool proteomeDiscoverer_1_4, const QString &str)
{
    static const QString fileTag = QStringLiteral("file:///");
    const bool dbMode = str.contains(fileTag);

    static const QString firstSeparator = QStringLiteral("@");
    static const QString secondSeparator = QStringLiteral("|");

    QStringList tokens;
    if (dbMode) {
        QRegExp regexp(QStringLiteral("^\\s*\\[file:///(.*)\\]\\s*@(.*)$"));
        regexp.indexIn(str);
        tokens = regexp.capturedTexts();
        if (tokens.isEmpty()) {
            return nullptr;
        }
        // get rid first element with whole string
        tokens.takeFirst();
    } else {
        tokens = str.split(firstSeparator);
    }

    if (tokens.count() < 2) {
        return nullptr;
    }
    const QString glycanStr = tokens.takeFirst().trimmed();

    tokens = tokens.join(firstSeparator).split(secondSeparator);
    if (tokens.count() < 2) {
        return nullptr;
    }
    const QString glycanType = tokens.takeFirst().trimmed();
    const QString fineStr = tokens.takeFirst().trimmed();

    GlycanInfoModel *result
        = new GlycanInfoModel(proteomeDiscoverer_2_0, proteomeDiscoverer_1_4, dbMode);

    // set glycan type
    for (int i = 0; i < s_allGlycanTypes.count(); ++i) {
        auto glycanTypeItem = s_allGlycanTypes[i];
        if (glycanTypeItem.byonicString == glycanType) {
            result->m_currentGlycanTypeIndex = i;
        }
    }
    if (result->m_currentGlycanTypeIndex < 0) {
        return nullptr;
    }

    // set glycan
    if (dbMode) {
        result->m_currentGlycanDbStr = glycanStr;
        result->tryFindFileInDb();
    } else {
        result->m_currentGlycan = glycanStr;
    }

    // set fine
    for (FineControl::FineType fine : result->m_allFines) {
        if (fine == FineControl::FineType::CommonGreaterThan2) {
            bool needBreak = false;
            for (int maxMod : result->m_allModMax) {
                if (fineStr == FineControl::toByonicString(fine, maxMod)) {
                    result->m_currentFine = fine;
                    result->m_currentModMax = maxMod;
                    needBreak = true;
                    break;
                }
            }
            if (needBreak) {
                break;
            }
        } else {
            if (fineStr == FineControl::toByonicString(fine)) {
                result->m_currentFine = fine;
                result->m_currentModMax = result->m_allModMax[0];
                break;
            }
        }
    }

    return result;
}
