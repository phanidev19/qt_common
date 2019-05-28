/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "GlycanEditorDialog.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QValidator>

class MonoisotopicMassCalculator
{
public:
    static double HexNacDeltaMass()
    {
        return 8 * massCarbon + 13 * massHydrogen + 5 * massOxygen + 1 * massNitrogen;
    }
    static double HexDeltaMass() { return 6 * massCarbon + 10 * massHydrogen + 5 * massOxygen; }
    static double FucDeltaMass() { return 6 * massCarbon + 10 * massHydrogen + 4 * massOxygen; }
    static double PentDeltaMass() { return 5 * massCarbon + 8 * massHydrogen + 4 * massOxygen; }
    static double NeuAcDeltaMass()
    {
        return 11 * massCarbon + 17 * massHydrogen + 8 * massOxygen + 1 * massNitrogen;
    }
    static double NeuGcDeltaMass()
    {
        return 11 * massCarbon + 17 * massHydrogen + 9 * massOxygen + 1 * massNitrogen;
    }
    static double SodiumDeltaMass() { return 1 * massSodium - 1 * massHydrogen; }

private:
    static const double massHydrogen;
    static const double massOxygen;
    static const double massCarbon;
    static const double massNitrogen;
    static const double massSodium;
};

const double MonoisotopicMassCalculator::massHydrogen = 1.007825035;
const double MonoisotopicMassCalculator::massOxygen = 15.99491463;
const double MonoisotopicMassCalculator::massCarbon = 12;
const double MonoisotopicMassCalculator::massNitrogen = 14.003074;
const double MonoisotopicMassCalculator::massSodium = 22.9897677;

class MonoisotopicElement
{
public:
    MonoisotopicElement(const QString &name, double mass,
                        const QStringList &equivalentNames = QStringList())
        : m_name(name)
        , m_mass(mass)
        , m_equivalentNames(equivalentNames)
    {
    }
    QString m_name;
    double m_mass = 0.0;
    QStringList m_equivalentNames;
};

static const QList<MonoisotopicElement> elements
    = { MonoisotopicElement(QStringLiteral("HexNAc"), MonoisotopicMassCalculator::HexNacDeltaMass(),
                            QStringList({ QStringLiteral("GalNac"), QStringLiteral("GlcNac") })),
        MonoisotopicElement(
            QStringLiteral("Hex"), MonoisotopicMassCalculator::HexDeltaMass(),
            QStringList({ QStringLiteral("Gal"), QStringLiteral("Glc"), QStringLiteral("Man") })),
        MonoisotopicElement(QStringLiteral("Fuc"), MonoisotopicMassCalculator::FucDeltaMass(),
                            QStringList({ QStringLiteral("dHex") })),
        MonoisotopicElement(QStringLiteral("Pent"), MonoisotopicMassCalculator::PentDeltaMass(),
                            QStringList({ QStringLiteral("Xyl") })),
        MonoisotopicElement(QStringLiteral("NeuAc"), MonoisotopicMassCalculator::NeuAcDeltaMass()),
        MonoisotopicElement(QStringLiteral("NeuGc"), MonoisotopicMassCalculator::NeuGcDeltaMass()),
        MonoisotopicElement(QStringLiteral("Sodium"), MonoisotopicMassCalculator::SodiumDeltaMass(),
                            QStringList({ QStringLiteral("Na") })) };

GlycanEditorDialog::GlycanEditorDialog(QWidget *parent)
    : QDialog(parent)
{
    setMinimumSize(40, 40);
    initUi();
    setWindowTitle(QStringLiteral("Edit Glycan"));
}

GlycanEditorDialog::~GlycanEditorDialog()
{
}

void GlycanEditorDialog::initUi()
{
    QGridLayout *gridLayout = new QGridLayout(this);
    setLayout(gridLayout);

    const int editorWidth = fontMetrics().averageCharWidth() * 10;

    const int varCount = elements.count();

    // titles, editors and spacing
    {
        QIntValidator *intValidator = new QIntValidator(this);
        intValidator->setBottom(0);
        for (int i = 0; i < elements.count(); ++i) {
            QString element = elements[i].m_name;
            gridLayout->addWidget(new QLabel(element, this), 0, i);
            QLineEdit *editor = new QLineEdit(this);
            editor->setValidator(intValidator);
            editor->setFixedWidth(editorWidth);
            gridLayout->addWidget(editor, 1, i);
            connect(editor, &QLineEdit::textChanged,
                    [this](const QString &) { updateTotalMass(); });
            m_elementsEditors[i] = editor;
        }
        gridLayout->addItem(
            new QSpacerItem(editorWidth / 2, 1, QSizePolicy::Fixed, QSizePolicy::Maximum), 0,
            varCount);
        gridLayout->addWidget(new QLabel("Additional mass"), 0, varCount + 1);

        QDoubleValidator *doubleValidator = new QDoubleValidator(this);
        doubleValidator->setBottom(0.0);
        m_additionalMassEditor = new QLineEdit(this);
        m_additionalMassEditor->setValidator(doubleValidator);
        m_additionalMassEditor->setFixedWidth(editorWidth * 2);
        connect(m_additionalMassEditor, &QLineEdit::textChanged,
                [this](const QString &) { updateTotalMass(); });
        gridLayout->addWidget(m_additionalMassEditor, 1, varCount + 1);
    }

    // total delata mass
    {
        m_totalMassLabel = new QLabel(this);
        gridLayout->addWidget(m_totalMassLabel, 2, 0, 1, varCount + 2);
        updateTotalMass();
    }

    // init buttons
    {
        QPushButton *okButton = new QPushButton(QStringLiteral("Ok"), this);
        connect(okButton, &QPushButton::clicked, [this](bool) { Q_EMIT accepted(); });

        QPushButton *cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
        connect(cancelButton, &QPushButton::clicked, [this](bool) { Q_EMIT rejected(); });

        QHBoxLayout *buttonsLayout = new QHBoxLayout();
        buttonsLayout->addSpacerItem(
            new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Maximum));
        buttonsLayout->addWidget(okButton);
        buttonsLayout->addWidget(cancelButton);
        buttonsLayout->addSpacerItem(
            new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Maximum));

        gridLayout->addLayout(buttonsLayout, 3, 0, 1, varCount + 2);
    }
}

void GlycanEditorDialog::updateTotalMass()
{
    static const QString totalMassTemplate = QStringLiteral("Total delta mass: <b>%0</b>");

    double totalMass = 0;
    for (int i = 0; i < elements.count(); ++i) {
        totalMass += m_elementsEditors[i]->text().toInt() * elements[i].m_mass;
    }
    totalMass += m_additionalMassEditor->text().toDouble();

    m_totalMassLabel->setText(totalMassTemplate.arg(totalMass));
}

void GlycanEditorDialog::startEdit(const QString &glycan)
{
    open();

    const QStringList tokens = glycan.trimmed().split(QStringLiteral(" "));
    m_additionalMassEditor->setText(tokens.value(1));

    const QString elementsStr = tokens.value(0);
    for (int i = 0; i < elements.count(); ++i) {
        m_elementsEditors[i]->setText(QString());
        QStringList names({ elements[i].m_name });
        names.append(elements[i].m_equivalentNames);
        for (auto name : names) {
            QRegExp regexp(name + QStringLiteral("\\(([0-9]+)\\)"));
            regexp.indexIn(elementsStr);
            const int pos = regexp.indexIn(elementsStr);
            if (pos != -1) {
                m_elementsEditors[i]->setText(QString::number(regexp.cap(1).toInt()));
                break;
            }
        }
    }
}

bool GlycanEditorDialog::isGlycanValid(const QString &glycan)
{
    const QStringList tokens = glycan.trimmed().split(QStringLiteral(" "));

    const QString elementsStr = tokens.value(0);
    for (int i = 0; i < elements.count(); ++i) {
        QStringList names({ elements[i].m_name });
        names.append(elements[i].m_equivalentNames);
        for (auto name : names) {
            QRegExp regexp(name + QStringLiteral("\\(([0-9]+)\\)"));
            regexp.indexIn(elementsStr);
            const int pos = regexp.indexIn(elementsStr);
            if (pos != -1) {
                return true;
            }
        }
    }
    return false;
}

QString GlycanEditorDialog::glycan() const
{
    QString result;
    for (int i = 0; i < elements.count(); ++i) {
        const int value = m_elementsEditors[i]->text().toInt();
        if (value == 0) {
            continue;
        }
        result += elements[i].m_name + QStringLiteral("(") + QString::number(value)
            + QStringLiteral(")");
    }
    result += QStringLiteral(" ") + m_additionalMassEditor->text();
    return result;
}
