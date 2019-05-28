/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "ModificationInfoModel.h"

#include "pmi_modifications_ui_debug.h"

static const int MODIFICATION_INDEX = 0;
static const int TARGET_INDEX = 1;
static const int FINE_INDEX = 2;
static const int MOD_MAX_INDEX = 3;
static const int ATTRIBUTE_INDEX = 4;
static const int LAST_PARENT_INDEX = 4;

static const QString INVERSION_PREFIX = QStringLiteral("(De)");

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

static QVector<Target> getTargetsForModification(const UnimodModifications &modifications,
                                                 int modificationIndex)
{
    QVector<Target> result;

    // targets for modification
    auto group = modifications.findGroupByModification(modificationIndex);
    for (int modIndex : group) {
        for (const auto &specificity : modifications.modifications()[modIndex].specificity) {
            result.push_back(
                Target({ specificity.position, ExtendedAminoAcid(specificity.site, false) }));
            if (specificity.site == "N"
                && specificity.classification
                    == UnimodModifications::UmodClassification::NLinkedGlycosylation) {
                result.push_back(
                    Target({ specificity.position, ExtendedAminoAcid(specificity.site, true) }));
            }
        }
    }
    std::sort(result.begin(), result.end());

    return result;
}

static QVector<Target> getAllPossibleTargets()
{
    static QVector<Target> allPossibleTargets;

    if (allPossibleTargets.isEmpty()) {
        static QVector<Target> possibleTargets;

        // all possible targets
        static const QString aminoAcids = QStringLiteral("ACDEFGHIKLMNPQRSTVWY");
        static const QVector<UnimodModifications::UmodPosition> terminalPositionTypes
            = { UnimodModifications::UmodPosition::AnyNterm,
                UnimodModifications::UmodPosition::ProteinNterm,
                UnimodModifications::UmodPosition::AnyCterm,
                UnimodModifications::UmodPosition::ProteinCterm };

        {
            QVector<Target> targets;
            for (auto position : terminalPositionTypes) {
                targets.push_back(Target(position, ExtendedAminoAcid()));
            }
            std::sort(targets.begin(), targets.end());
            possibleTargets.append(targets);
        }
        {
            QVector<Target> targets;
            for (auto ch : aminoAcids) {
                targets.push_back(
                    Target(UnimodModifications::UmodPosition::Anywhere, ExtendedAminoAcid(ch)));
            }
            std::sort(targets.begin(), targets.end());
            possibleTargets.append(targets);
        }
        {
            QVector<Target> targets;
            for (auto position : terminalPositionTypes) {
                for (auto ch : aminoAcids) {
                    targets.push_back(Target(position, ExtendedAminoAcid(ch)));
                }
            }
            std::sort(targets.begin(), targets.end());
            possibleTargets.append(targets);
        }

        allPossibleTargets = possibleTargets;
    }
    return allPossibleTargets;
}

ModificationInfoModel::ModificationInfoModel(QSharedPointer<UnimodModifications> modifications,
                                             bool proteomeDiscoverer_2_0,
                                             bool proteomeDiscoverer_1_4, QObject *parent)
    : QAbstractItemModel(parent)
    , m_proteomeDiscoverer_2_0(proteomeDiscoverer_2_0)
    , m_proteomeDiscoverer_1_4(proteomeDiscoverer_1_4)
    , m_inverted(false)
    , m_currentModificationIndex(-1)
    , m_currentModMax(0)
    , m_modifications(modifications)
    , m_allFines({ FineControl::FineType::Fixed, FineControl::FineType::Rare1,
                   FineControl::FineType::Rare2, FineControl::FineType::Common1,
                   FineControl::FineType::Common2, FineControl::FineType::CommonGreaterThan2 })
    , m_allModMax({ 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 })
{
    m_currentFine = m_allFines[0];

    updatePredefinedModifications();
    updateTargetsList();
}

ModificationInfoModel::~ModificationInfoModel()
{
}

bool ModificationInfoModel::inverted() const
{
    return m_inverted;
}

void ModificationInfoModel::setInverted(bool inverted)
{
    if (m_inverted == inverted) {
        return;
    }
    m_inverted = inverted;
    updatePredefinedModifications();
    const QModelIndex pIndex = modificationParentIndex();
    if (m_currentModificationIndex >= 0 || !m_currentModificationStr.isEmpty()) {
        Q_EMIT dataChanged(index(0, 0, pIndex),
                           index(rowCount(pIndex) - 1, columnCount(pIndex) - 1),
                           { Qt::DisplayRole });
    }
}

void ModificationInfoModel::updatePredefinedModifications()
{
    static const QString frequentlyUsedModificationsHeader
        = QStringLiteral("-- Frequently-used modifications --");
    static const QString allModificationsHeader = QStringLiteral("-- All modifications --");

    static const QStringList commonModificationNames = { QStringLiteral("Acetyl"),
                                                         QStringLiteral("Amidated"),
                                                         QStringLiteral("Ammonia-loss"),
                                                         QStringLiteral("Carbamidomethyl"),
                                                         QStringLiteral("Carbamyl"),
                                                         QStringLiteral("Carboxymethyl"),
                                                         QStringLiteral("Cation:Na"),
                                                         QStringLiteral("Deamidated"),
                                                         QStringLiteral("Dehydrated"),
                                                         QStringLiteral("Delta:C(1)"),
                                                         QStringLiteral("Delta:H(2)C(2)"),
                                                         QStringLiteral("Delta:H(4)C(3)"),
                                                         QStringLiteral("Dethiomethyl"),
                                                         QStringLiteral("Dicarbamidomethyl"),
                                                         QStringLiteral("Dimethyl"),
                                                         QStringLiteral("Dioxidation"),
                                                         QStringLiteral("DTT"),
                                                         QStringLiteral("Formyl"),
                                                         QStringLiteral("Gln->pyro-Glu"),
                                                         QStringLiteral("Glu->pyro-Glu"),
                                                         QStringLiteral("Hex"),
                                                         QStringLiteral("HexNAc"),
                                                         QStringLiteral("iodoTMT6plex"),
                                                         QStringLiteral("iTRAQ4plex"),
                                                         QStringLiteral("iTRAQ8plex"),
                                                         QStringLiteral("Methyl"),
                                                         QStringLiteral("Methylthio"),
                                                         QStringLiteral("Oxidation"),
                                                         QStringLiteral("Phospho"),
                                                         QStringLiteral("Propionamide"),
                                                         QStringLiteral("Sulfo"),
                                                         QStringLiteral("TMT"),
                                                         QStringLiteral("TMT2plex"),
                                                         QStringLiteral("TMT6plex"),
                                                         QStringLiteral("Trimethyl"),
                                                         QStringLiteral("Trioxidation") };

    static QVector<int> commonModificationIndexes;
    if (commonModificationIndexes.isEmpty()) {
        QVector<int> indexes;
        for (const QString &name : commonModificationNames) {
            const int modificationIndex = findModificationIndexByTitle(*m_modifications, name);
            if (modificationIndex < 0) {
                warningModUi()
                    << "ModificationInfoModel::initPredefinedModifications - unknown name:" << name;
                continue;
            }
            indexes << modificationIndex;
        }
        commonModificationIndexes = indexes;
    }

    m_predefinedModifications.clear();
    if (!m_proteomeDiscoverer_2_0) {
        m_predefinedModifications << qMakePair(frequentlyUsedModificationsHeader, -1);
        for (int modificationIndex : commonModificationIndexes) {
            m_predefinedModifications
                << qMakePair(toString(m_modifications->modifications()[modificationIndex], false),
                             modificationIndex);
        }
        m_predefinedModifications << qMakePair(allModificationsHeader, -1);
    }
}

void ModificationInfoModel::updateTargetsList()
{
    if (m_currentTargets.count() > 0) {
        Q_EMIT beginRemoveRows(targetParentIndex(), 0, m_currentTargets.count() - 1);
        m_currentTargets.clear();
        Q_EMIT endRemoveRows();
    }

    QVector<Target> newTargets;
    if (m_currentModificationIndex >= 0) {
        newTargets = getTargetsForModification(*m_modifications, m_currentModificationIndex);
    } else if (!m_currentModificationStr.isEmpty()) {
        newTargets = getAllPossibleTargets();
    }

    if (newTargets.count() > 0) {
        Q_EMIT beginInsertRows(targetParentIndex(), 0, newTargets.count() - 1);
        m_currentTargets = newTargets;
        Q_EMIT endInsertRows();
    }

    Q_EMIT dataChanged(targetParentIndex(), targetParentIndex(), QVector<int>({ Qt::DisplayRole }));
}

bool ModificationInfoModel::isCompleted() const
{
    if (m_currentModificationIndex < 0 && m_currentModificationStr.isEmpty()) {
        return false;
    }

    for (const Target &target : m_currentTargets) {
        if (target.isActive) {
            return true;
        }
    }

    return false;
}

void ModificationInfoModel::checkIsCompleted()
{
    if (isCompleted()) {
        Q_EMIT modelIsCompleted();
    }
}

QString ModificationInfoModel::getStatusAdjustedName(const QString &name) const
{
    static const QString inversionPrefix = INVERSION_PREFIX + QStringLiteral(" ");
    if (m_inverted) {
        return inversionPrefix + name;
    }
    return name;
}

QString ModificationInfoModel::getStatusAdjustedNameByonic(const QString &name) const
{
    static const QString inversionPrefix = INVERSION_PREFIX;
    if (m_inverted) {
        return inversionPrefix + name;
    }
    return name;
}

double ModificationInfoModel::getStatusAdjustedMonoisotopicDeltaMass(double mass) const
{
    if (m_inverted) {
        return mass * -1.0;
    }
    return mass;
}

QString ModificationInfoModel::toString(const UnimodModifications::UmodMod &umod,
                                        bool byonicFormat) const
{
    static const QString separator = QStringLiteral(" / ");

    const QString statusAdjustedName = byonicFormat ? getStatusAdjustedNameByonic(umod.title)
                                                    : getStatusAdjustedName(umod.title);

    if (umod.delta.monoMass == 0) {
        return statusAdjustedName;
    }
    const double statusAdjustedMonoisotopicDeltaMass
        = getStatusAdjustedMonoisotopicDeltaMass(umod.delta.monoMass);
    return (statusAdjustedName.isEmpty() ? QString() : (statusAdjustedName + separator))
        + (statusAdjustedMonoisotopicDeltaMass > 0.0 ? "+" : "")
        + QString::number(statusAdjustedMonoisotopicDeltaMass, 'f', 6);
}

QModelIndex ModificationInfoModel::modificationParentIndex() const
{
    return createIndex(0, MODIFICATION_INDEX, (quintptr)-1);
}

QModelIndex ModificationInfoModel::targetParentIndex() const
{
    return createIndex(0, TARGET_INDEX, (quintptr)-1);
}

QModelIndex ModificationInfoModel::fineParentIndex() const
{
    return createIndex(0, FINE_INDEX, (quintptr)-1);
}

QModelIndex ModificationInfoModel::modMaxParentIndex() const
{
    return createIndex(0, MOD_MAX_INDEX, (quintptr)-1);
}

QModelIndex ModificationInfoModel::attributeParentIndex() const
{
    return createIndex(0, ATTRIBUTE_INDEX, (quintptr)-1);
}

QString ModificationInfoModel::toByonicString() const
{
    static const QString targetsSeparator = QStringLiteral(", ");
    static const QString modAndTargetsSeparator = QStringLiteral(" @ ");
    static const QString targetsAndFineSeparator = QStringLiteral(" | ");
    static const QString fineAndAttributeSeparator = QStringLiteral(" | ");

    if (m_currentModificationIndex < 0 && m_currentModificationStr.isEmpty()) {
        return QString();
    }

    QStringList activeTargets;
    for (const Target &target : m_currentTargets) {
        if (target.isActive) {
            activeTargets << target.toByonicString();
        }
    }
    if (activeTargets.isEmpty()) {
        return QString();
    }

    QString result;

    if (!m_currentModificationStr.isEmpty()) {
        result += m_currentModificationStr;
    } else {
        result += toString(m_modifications->modifications()[m_currentModificationIndex], true);
    }
    result += modAndTargetsSeparator;
    result += activeTargets.join(targetsSeparator);
    result += targetsAndFineSeparator;
    result += FineControl::toByonicString(m_currentFine, m_currentModMax);
    if (!m_attribute.isEmpty()) {
        result += fineAndAttributeSeparator;
        result += m_attribute;
    }

    return result;
}

QModelIndex ModificationInfoModel::index(int row, int column, const QModelIndex &parent) const
{
    if (parent == modificationParentIndex()) {
        return createIndex(row, column, (quintptr)MODIFICATION_INDEX);
    }
    if (parent == targetParentIndex()) {
        return createIndex(row, column, (quintptr)TARGET_INDEX);
    }
    if (parent == fineParentIndex()) {
        return createIndex(row, column, (quintptr)FINE_INDEX);
    }
    if (parent == modMaxParentIndex()) {
        return createIndex(row, column, (quintptr)MOD_MAX_INDEX);
    }
    if (parent == attributeParentIndex()) {
        return createIndex(row, column, (quintptr)ATTRIBUTE_INDEX);
    }
    return createIndex(row, column, (quintptr)-1);
}

QModelIndex ModificationInfoModel::parent(const QModelIndex &child) const
{
    if (!child.isValid() || child.model() != this) {
        return QModelIndex();
    }
    switch (child.internalId()) {
    case (quintptr)MODIFICATION_INDEX:
        return modificationParentIndex();
    case (quintptr)TARGET_INDEX:
        return targetParentIndex();
    case (quintptr)FINE_INDEX:
        return fineParentIndex();
    case (quintptr)MOD_MAX_INDEX:
        return modMaxParentIndex();
    case (quintptr)ATTRIBUTE_INDEX:
        return attributeParentIndex();
    default:
        return QModelIndex();
    }
    return QModelIndex();
}

int ModificationInfoModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return 1;
    }
    if (parent == modificationParentIndex()) {
        return m_predefinedModifications.count() + m_modifications->groupsCount();
    }
    if (parent == targetParentIndex()) {
        return m_currentTargets.count();
    }
    if (parent == fineParentIndex()) {
        return m_allFines.count();
    }
    if (parent == modMaxParentIndex()) {
        return m_allModMax.count();
    }
    if (parent == attributeParentIndex()) {
        return 0;
    }
    return 0;
}

int ModificationInfoModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return LAST_PARENT_INDEX;
    }
    return 1;
}

QVariant ModificationInfoModel::data(const QModelIndex &index, int role) const
{
    // modification combo state
    if (index == modificationParentIndex()) {
        if (role == Qt::UserRole) {
            if (m_currentModificationIndex >= 0) {
                return m_currentModificationIndex;
            } else if (!m_currentModificationStr.isEmpty()) {
                return m_currentModificationStr;
            }
            return QVariant();
        }
    }

    // modification combobox contents
    if (index.parent() == modificationParentIndex()) {
        if (index.row() < m_predefinedModifications.count()) {
            if (role == Qt::DisplayRole || role == Qt::EditRole) {
                return m_predefinedModifications[index.row()].first;
            }
            if (role == Qt::UserRole) {
                return m_predefinedModifications[index.row()].second;
            }
        }
        const int groupIndex = index.row() - m_predefinedModifications.count();
        if (groupIndex < 0 || groupIndex >= m_modifications->groupsCount()) {
            return QVariant();
        }

        const int modificationIndex = m_modifications->groupModifications(groupIndex)[0];
        if (role == Qt::DisplayRole || role == Qt::EditRole) {
            const UnimodModifications::UmodMod &mod
                = m_modifications->modifications()[modificationIndex];
            return toString(mod, false);
        }
        if (role == Qt::UserRole) {
            return modificationIndex;
        }
        return QVariant();
    }

    // target combo display text
    if (index == targetParentIndex()) {
        if (role == Qt::DisplayRole) {
            static const QString noItemsText = QStringLiteral("- no items -");
            static const QString separator = QStringLiteral(", ");
            QStringList items;
            for (const Target &target : m_currentTargets) {
                if (target.isActive) {
                    items << target.toComboBoxDisplayString();
                }
            }
            if (items.isEmpty()) {
                return noItemsText;
            }
            return items.join(separator);
        }
        return QVariant();
    }

    // targets combobox contents
    if (index.parent() == targetParentIndex()) {
        if (index.row() < 0 || index.row() >= m_currentTargets.count()) {
            return QVariant();
        }
        if (role == Qt::DisplayRole) {
            return m_currentTargets[index.row()].toComboBoxDisplayString();
        }
        if (role == Qt::CheckStateRole) {
            return m_currentTargets[index.row()].isActive ? Qt::Checked : Qt::Unchecked;
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

    // attribute data
    if (index == attributeParentIndex()) {
        if (role == Qt::UserRole) {
            return m_attribute;
        }
        return QVariant();
    }

    return QVariant();
}

void ModificationInfoModel::setTargetIsActive(int targetIndex, bool isActive)
{
    QModelIndex updateIndex = index(targetIndex, 0, targetParentIndex());
    m_currentTargets[targetIndex].isActive = isActive;
    Q_EMIT dataChanged(updateIndex, updateIndex, QVector<int>({ Qt::CheckStateRole }));
    Q_EMIT dataChanged(targetParentIndex(), targetParentIndex(), QVector<int>({ Qt::DisplayRole }));
}

bool ModificationInfoModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index == modificationParentIndex()) {
        if (value.type() == QVariant::String) {
            const QString newModification = value.toString().trimmed();
            if (m_currentModificationStr == newModification) {
                return true;
            }
            m_currentModificationStr = newModification;
            m_currentModificationIndex = -1;
            m_inverted = false;
            updateTargetsList();
            Q_EMIT dataChanged(modificationParentIndex(), modificationParentIndex());
            checkIsCompleted();
        }
        if (value.type() == QVariant::Int) {
            const int newModification = value.toInt();
            if (newModification < 0 || newModification >= m_modifications->groupsCount()) {
                return false;
            }
            if (m_currentModificationIndex == newModification) {
                return true;
            }
            m_currentModificationIndex = newModification;
            m_currentModificationStr.clear();
            updateTargetsList();
            Q_EMIT dataChanged(modificationParentIndex(), modificationParentIndex());
            checkIsCompleted();
        }
    }

    // change selected targets. We change items, not parent
    if (index.parent() == targetParentIndex()) {
        if (index.row() < 0 || index.row() >= m_currentTargets.count()) {
            return false;
        }
        if (role == Qt::CheckStateRole) {
            setTargetIsActive(index.row(), value.toInt() == Qt::Checked);
            checkIsCompleted();
            return true;
        }
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

    if (index == attributeParentIndex()) {
        if (value.type() != QVariant::String) {
            return false;
        }
        const QString newAttributeValue = value.toString().trimmed();
        if (m_attribute == newAttributeValue) {
            return true;
        }
        m_attribute = newAttributeValue;
        Q_EMIT dataChanged(attributeParentIndex(), attributeParentIndex(),
                           QVector<int>({ Qt::UserRole }));
        checkIsCompleted();
        return true;
    }

    return false;
}

Qt::ItemFlags ModificationInfoModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags result = QAbstractItemModel::flags(index);

    if (index.parent() == modificationParentIndex()) {
        if (index.row() < m_predefinedModifications.count()) {
            if (m_predefinedModifications[index.row()].second < 0) {
                result.setFlag(Qt::ItemIsSelectable, false);
            }
        }
    }

    if (index.parent() == targetParentIndex()) {
        if (index.row() >= 0 && index.row() < rowCount(targetParentIndex())
            && index.column() == 0) {
            result.setFlag(Qt::ItemIsEnabled, true);
            result.setFlag(Qt::ItemIsUserCheckable, true);
        }
    }

    return result;
}

ModificationInfoModel *
ModificationInfoModel::createFromString(QSharedPointer<UnimodModifications> modifications,
                                        bool proteomeDiscoverer_2_0, bool proteomeDiscoverer_1_4,
                                        const QString &str)
{
    static const QString firstSeparator = QStringLiteral("@");
    static const QString secondSeparator = QStringLiteral("|");
    static const QString modPartsSeparator = QStringLiteral("/");
    static const QString targetsSeparator = QStringLiteral(",");

    QStringList tokens = str.split(firstSeparator);
    if (tokens.count() < 2) {
        return nullptr;
    }
    const QString modStr = tokens.takeFirst().trimmed();

    tokens = tokens.join(firstSeparator).split(secondSeparator);
    if (tokens.count() < 2) {
        return nullptr;
    }
    const QString targetsStr = tokens.takeFirst().trimmed();
    const QString fineStr = tokens.takeFirst().trimmed();
    QString attribute;
    if (!tokens.isEmpty()) {
        attribute = tokens.join(secondSeparator).trimmed();
    }

    ModificationInfoModel *result
        = new ModificationInfoModel(modifications, proteomeDiscoverer_2_0, proteomeDiscoverer_1_4);

    // set modification
    tokens = modStr.split(modPartsSeparator);
    QString modTitle = tokens.value(0).trimmed();
    if (modTitle.startsWith(INVERSION_PREFIX)) {
        result->setInverted(true);
        modTitle = modTitle.mid(INVERSION_PREFIX.length()).trimmed();
    }
    const double modMonoMass = tokens.value(1).toDouble() * (result->inverted() ? -1.0 : 1.0);
    const int modIndex
        = modifications->findApproximatelyEquivalentModification(modTitle, modMonoMass);
    if (modIndex >= 0) {
        result->m_currentModificationIndex = modIndex;
    } else {
        result->m_currentModificationStr = modStr;
    }

    // set targets
    result->updateTargetsList();
    const QStringList targetsStrings = targetsStr.split(targetsSeparator);
    for (QString target : targetsStrings) {
        target = target.trimmed();
        for (int i = 0; i < result->m_currentTargets.count(); ++i) {
            if (result->m_currentTargets[i].getValidByonicStrings().contains(target)) {
                result->setTargetIsActive(i, true);
                break;
            }
        }
    }

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

    result->m_attribute = attribute;

    return result;
}

ExtendedAminoAcid::ExtendedAminoAcid()
{
}

ExtendedAminoAcid::ExtendedAminoAcid(const QString &aminoAcid, bool nGlyscan)
    : m_aminoAcid(aminoAcid)
    , m_nGlyscan(nGlyscan)
{
}

inline QString ExtendedAminoAcid::toComboBoxDisplayString() const
{
    if (m_nGlyscan) {
        return QStringLiteral("N-Glycan");
    } else if (m_aminoAcid.length() == 1 && m_aminoAcid[0].isLetter()) {
        return m_aminoAcid;
    }
    return QString();
}

QString ExtendedAminoAcid::toByonicString() const
{
    if (m_nGlyscan) {
        return QStringLiteral("NGlycan");
    } else if (m_aminoAcid.length() == 1 && m_aminoAcid[0].isLetter()) {
        return m_aminoAcid;
    }
    return QString();
}

Target::Target()
    : m_position(UnimodModifications::UmodPosition::Anywhere)
{
}

Target::Target(UnimodModifications::UmodPosition position,
               const ExtendedAminoAcid &extendedAminoAcid)
    : m_position(position)
    , m_extendedAminoAcid(extendedAminoAcid)
{
}

QString Target::getComboBoxDisplayString(UnimodModifications::UmodPosition position)
{
    switch (position) {
    case UnimodModifications::UmodPosition::AnyNterm:
        return QStringLiteral("Peptide N - term");
    case UnimodModifications::UmodPosition::AnyCterm:
        return QStringLiteral("Peptide C - term");
    case UnimodModifications::UmodPosition::ProteinNterm:
        return QStringLiteral("Protein N - term");
    case UnimodModifications::UmodPosition::ProteinCterm:
        return QStringLiteral("Protein C - term");
    default:
        return QString();
    }
    return QString();
}

QStringList Target::getValidByonicStrings(UnimodModifications::UmodPosition position)
{
    switch (position) {
    case UnimodModifications::UmodPosition::AnyNterm:
        return QStringList({ QStringLiteral("NTerm"), QStringLiteral("NTerminal"),
                             QStringLiteral("NTerminus"), QStringLiteral("N-Term"),
                             QStringLiteral("N - Terminal"), QStringLiteral("N - Terminus") });
    case UnimodModifications::UmodPosition::AnyCterm:
        return QStringList({ QStringLiteral("CTerm"), QStringLiteral("CTerminal"),
                             QStringLiteral("CTerminus"), QStringLiteral("C-Term"),
                             QStringLiteral("C - Terminal"), QStringLiteral("C - Terminus") });
    case UnimodModifications::UmodPosition::ProteinNterm:
        return QStringList(
            { QStringLiteral("Protein NTerm"), QStringLiteral("Protein NTerminal"),
              QStringLiteral("Protein NTerminus"), QStringLiteral("Protein N - Term"),
              QStringLiteral("Protein N - Terminal"), QStringLiteral("Protein N - Terminus") });
    case UnimodModifications::UmodPosition::ProteinCterm:
        return QStringList(
            { QStringLiteral("Protein CTerm"), QStringLiteral("Protein CTerminal"),
              QStringLiteral("Protein CTerminus"), QStringLiteral("Protein C - Term"),
              QStringLiteral("Protein C - Terminal"), QStringLiteral("Protein C - Terminus") });
    default:
        return QStringList({ QString() });
    }
}

QString Target::getPreferredByonicString(UnimodModifications::UmodPosition position)
{
    return getValidByonicStrings(position).first();
}

QString Target::toComboBoxDisplayString() const
{
    static const QString separator = QStringLiteral(" ");
    QStringList result;

    const QString positionStr = getComboBoxDisplayString(m_position);
    if (!positionStr.isEmpty()) {
        result.push_back(positionStr);
    }
    const QString extendedAminoAcidStr = m_extendedAminoAcid.toComboBoxDisplayString();
    if (!extendedAminoAcidStr.isEmpty()) {
        result.push_back(extendedAminoAcidStr);
    }

    return result.join(separator);
}

QStringList Target::getValidByonicStrings() const
{
    static const QString separator = QStringLiteral(" ");

    const QStringList positionStrings = getValidByonicStrings(m_position);
    QStringList result;
    for (auto positionStr : positionStrings) {
        QStringList byonicString;

        if (!positionStr.isEmpty()) {
            byonicString.push_back(positionStr);
        }
        const QString extendedAminoAcidStr = m_extendedAminoAcid.toByonicString();
        if (!extendedAminoAcidStr.isEmpty()) {
            byonicString.push_back(extendedAminoAcidStr);
        }

        result << byonicString.join(separator);
    }
    return result;
}

QString Target::toByonicString() const
{
    static const QString separator = QStringLiteral(" ");
    QStringList byonicString;

    const QString positionStr = getPreferredByonicString(m_position);
    if (!positionStr.isEmpty()) {
        byonicString.push_back(positionStr);
    }
    const QString extendedAminoAcidStr = m_extendedAminoAcid.toByonicString();
    if (!extendedAminoAcidStr.isEmpty()) {
        byonicString.push_back(extendedAminoAcidStr);
    }

    return byonicString.join(separator);
}

int Target::getOrderPosition(UnimodModifications::UmodPosition position)
{
    switch (position) {
    case UnimodModifications::UmodPosition::AnyNterm:
        return 0;
    case UnimodModifications::UmodPosition::ProteinNterm:
        return 1;
    case UnimodModifications::UmodPosition::AnyCterm:
        return 2;
    case UnimodModifications::UmodPosition::ProteinCterm:
        return 3;
    default:
        return 4;
    }
}

bool Target::operator<(const Target &other) const
{
    if (getOrderPosition(m_position) < getOrderPosition(other.m_position)) {
        return true;
    }
    if (getOrderPosition(m_position) > getOrderPosition(other.m_position)) {
        return false;
    }
    return m_extendedAminoAcid.toComboBoxDisplayString()
        < other.m_extendedAminoAcid.toComboBoxDisplayString();
}
