/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef UNIMODMODIFICATIONS_H
#define UNIMODMODIFICATIONS_H

#include <QList>
#include <QVector>

class UnimodModifications
{
public:
    enum class UmodPosition {
        Anywhere, //! "Anywhere"
        AnyNterm, //! "Any N-term"
        AnyCterm, //! "Any C-term"
        ProteinNterm, //! "Protein N-term"
        ProteinCterm, //! "Protein C-term"
    };

    enum class UmodXrefSource {
        No, //! "-"
        PubMedPMID, //! "PubMedPMID"
        CASRegistry, //! "CAS Registry"
        CarbBank, //! "CarbBank"
        RESID, //! "RESID"
        SwissProt, //! "Swiss-Prot"
        Prosite, //! "Prosite"
        Entrez, //! "Entrez"
        Book, //! "Book"
        Journal, //! "Journal"
        MiscURL, //! "Misc. URL"
        FindMod, //! "FindMod"
        Other //! "Other"
    };

    enum class UmodClassification {
        No, //! "-"
        PostTranslational, //! "Post-translational"
        Cotranslational, //! "Co-translational"
        PreTranslational, //! "Pre-translational"
        ChemicalDerivative, //! "Chemical derivative"
        Artefact, //! "Artefact"
        NLinkedGlycosylation, //! "N-linked glycosylation"
        OLinkedGlycosylation, //! "O-linked glycosylation"
        OtherGlycosylation, //! "Other glycosylation"
        SynthPepProtectGp, //! "Synth. pep. protect. gp."
        IsotopicLabel, //! "Isotopic label"
        NonStandardResidue, //! "Non-standard residue"
        Multiple, //! "Multiple"
        Other, //! "Other"
        AaSubstitution, //! "AA substitution"
    };

    struct UmodElemRef {
        QString symbol;
        int number = 1;
    };

    struct UmodComposition {
        QVector<UmodElemRef> element;
        QString composition;
        double monoMass = 0;
        bool monoMassSpecified = false;
        double avgeMass = 0;
        bool avgeMassSpecified = false;
    };

    struct UmodNeutralLoss : UmodComposition {
        bool flag = false;
    };

    struct UmodPepNeutralLoss : UmodComposition {
        bool required = false;
    };

    struct UmodSpecificity {
        QVector<UmodNeutralLoss> neutralLoss;
        QVector<UmodPepNeutralLoss> pepNeutralLoss;
        QString miscNotes;
        bool hidden = false;
        QString site;
        UmodPosition position = UmodPosition::Anywhere;
        UmodClassification classification = UmodClassification::No;
        int specGroup = 1;
    };

    struct UmodXref {
        QString text;
        UmodXrefSource source = UmodXrefSource::No;
        QString url;
    };

    struct UmodMod {
        QVector<UmodSpecificity> specificity;
        UmodComposition delta;
        QVector<UmodComposition> ignore;
        QStringList altName;
        QVector<UmodXref> xref;
        QString miscNotes;
        QString title;
        QString fullName;
        QString usernameOfPoster;
        QString groupOfPoster;
        QString dateTimePosted;
        QString dateTimeModified;
        bool approved = false;
        bool approvedSpecified = false;
        QString exCodeName;
        long recordId = 0;
        bool recordIdSpecified = false;
    };

    UnimodModifications();
    ~UnimodModifications();

    static UmodPosition toPosition(const QString &str);
    static UmodXrefSource toXrefSource(const QString &str);
    static UmodClassification toClassification(const QString &str);

    const QVector<UmodMod> &modifications() const;
    void setModifications(const QVector<UmodMod> &modifications);

    int findApproximatelyEquivalentModification(const QString &title, double mass) const;

    int groupsCount() const;
    int groupCount(int index) const;
    // ???? error handling ????
    QVector<int> groupModifications(int groupIndex) const;
    // ???? error handling ????
    QVector<int> findGroupByModification(int modificationIndex) const;

private:
    static bool areApproximatelyEquivalent(const UmodMod &first, const UmodMod &second);
    void groupAndSortUnimodModificationsByNameAndDeltaMass();

private:
    QVector<UmodMod> m_modifications;
    QVector<QVector<int>> m_modificationGroupsMapping;
};

#endif // UNIMODMODIFICATIONS_H
