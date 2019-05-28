/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "UnimodModifications.h"

#include "pmi_modifications_ui_debug.h"

#include <QMap>

static bool areApproximatelyEquivalent4(const QString &firstTitle, double firstMonoMass,
                                        const QString &secondTitle, double secondMonoMass)
{
    static const double epsilon = 1e-7;
    return std::abs(firstMonoMass - secondMonoMass) < epsilon
        && QString::compare(firstTitle, secondTitle, Qt::CaseInsensitive) == 0;
}

UnimodModifications::UnimodModifications()
{
}

UnimodModifications::~UnimodModifications()
{
}

UnimodModifications::UmodPosition UnimodModifications::toPosition(const QString &str)
{
    static const QMap<QString, UmodPosition> conversionMap
        = { { "Anywhere", UmodPosition::Anywhere },
            { "Any N-term", UmodPosition::AnyNterm },
            { "Any C-term", UmodPosition::AnyCterm },
            { "Protein N-term", UmodPosition::ProteinNterm },
            { "Protein C-term", UmodPosition::ProteinCterm } };
    const auto it = conversionMap.find(str);
    if (it != conversionMap.constEnd()) {
        return it.value();
    }
    warningModUi() << "UnimodModifications::toPosition - unknown position: " << str;
    return UmodPosition::Anywhere;
}

UnimodModifications::UmodXrefSource UnimodModifications::toXrefSource(const QString &str)
{
    static const QMap<QString, UmodXrefSource> conversionMap
        = { { "-", UmodXrefSource::No },
            { "PubMed PMID", UmodXrefSource::PubMedPMID },
            { "CAS Registry", UmodXrefSource::CASRegistry },
            { "CarbBank", UmodXrefSource::CarbBank },
            { "RESID", UmodXrefSource::RESID },
            { "Swiss-Prot", UmodXrefSource::SwissProt },
            { "Prosite", UmodXrefSource::Prosite },
            { "Entrez", UmodXrefSource::Entrez },
            { "Book", UmodXrefSource::Book },
            { "Journal", UmodXrefSource::Journal },
            { "Misc. URL", UmodXrefSource::MiscURL },
            { "FindMod", UmodXrefSource::FindMod },
            { "Other", UmodXrefSource::Other } };
    const auto it = conversionMap.find(str);
    if (it != conversionMap.constEnd()) {
        return it.value();
    }
    warningModUi() << "UnimodModifications::toXrefSource - unknown source: " << str;
    return UmodXrefSource::Other;
}

UnimodModifications::UmodClassification UnimodModifications::toClassification(const QString &str)
{
    static const QMap<QString, UmodClassification> conversionMap
        = { { "-", UmodClassification::No },
            { "Post-translational", UmodClassification::PostTranslational },
            { "Co-translational", UmodClassification::Cotranslational },
            { "Pre-translational", UmodClassification::PreTranslational },
            { "Chemical derivative", UmodClassification::ChemicalDerivative },
            { "Artefact", UmodClassification::Artefact },
            { "N-linked glycosylation", UmodClassification::NLinkedGlycosylation },
            { "O-linked glycosylation", UmodClassification::OLinkedGlycosylation },
            { "Other glycosylation", UmodClassification::OtherGlycosylation },
            { "Synth. pep. protect. gp.", UmodClassification::SynthPepProtectGp },
            { "Isotopic label", UmodClassification::IsotopicLabel },
            { "Non-standard residue", UmodClassification::NonStandardResidue },
            { "Multiple", UmodClassification::Multiple },
            { "Other", UmodClassification::Other },
            { "AA substitution", UmodClassification::AaSubstitution } };
    const auto it = conversionMap.find(str);
    if (it != conversionMap.constEnd()) {
        return it.value();
    }
    warningModUi() << "UnimodModifications::toClassification - unknown classification: " << str;
    return UmodClassification::Other;
}

void UnimodModifications::setModifications(
    const QVector<UnimodModifications::UmodMod> &modifications)
{
    m_modifications = modifications;

    groupAndSortUnimodModificationsByNameAndDeltaMass();
}

int UnimodModifications::findApproximatelyEquivalentModification(const QString &title,
                                                                 double mass) const
{
    for (int i = 0; i < m_modifications.count(); ++i) {
        if (areApproximatelyEquivalent4(m_modifications[i].title, m_modifications[i].delta.monoMass,
                                        title, mass)) {
            return i;
        }
    }
    return -1;
}

int UnimodModifications::groupsCount() const
{
    return m_modificationGroupsMapping.count();
}

int UnimodModifications::groupCount(int index) const
{
    return m_modificationGroupsMapping[index].count();
}

QVector<int> UnimodModifications::groupModifications(int groupIndex) const
{
    return m_modificationGroupsMapping[groupIndex];
}

QVector<int> UnimodModifications::findGroupByModification(int modificationIndex) const
{
    for (auto it = m_modificationGroupsMapping.begin(); it != m_modificationGroupsMapping.end();
         ++it) {
        if (it->contains(modificationIndex)) {
            return *it;
        }
    }
    return QVector<int>();
}

const QVector<UnimodModifications::UmodMod> &UnimodModifications::modifications() const
{
    return m_modifications;
}

bool UnimodModifications::areApproximatelyEquivalent(const UnimodModifications::UmodMod &first,
                                                     const UnimodModifications::UmodMod &second)
{
    return areApproximatelyEquivalent4(first.title, first.delta.monoMass, second.title,
                                       second.delta.monoMass);
}

void UnimodModifications::groupAndSortUnimodModificationsByNameAndDeltaMass()
{
    m_modificationGroupsMapping.clear();

    for (int i = 0; i < m_modifications.count(); ++i) {
        const UnimodModifications::UmodMod &modification = m_modifications[i];

        auto it = m_modificationGroupsMapping.end();
        for (it = m_modificationGroupsMapping.begin(); it != m_modificationGroupsMapping.end();
             ++it) {
            if (areApproximatelyEquivalent(m_modifications[(*it)[0]], modification)) {
                it->push_back(i);
                break;
            }
        }
        if (it == m_modificationGroupsMapping.end()) {
            m_modificationGroupsMapping.push_back(QVector<int>({ i }));
        }
    }

    auto compareModifications
        = [this](const QVector<int> &first, const QVector<int> &second) -> bool {
        const int titleCompareResult = QString::compare(
            m_modifications[first[0]].title, m_modifications[second[0]].title, Qt::CaseInsensitive);
        if (titleCompareResult < 0) {
            return true;
        }
        if (titleCompareResult > 0) {
            return false;
        }
        return m_modifications[first[0]].delta.monoMass < m_modifications[second[0]].delta.monoMass;
    };
    std::sort(m_modificationGroupsMapping.begin(), m_modificationGroupsMapping.end(),
              compareModifications);
}
