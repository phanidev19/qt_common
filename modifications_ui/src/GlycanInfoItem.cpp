/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "GlycanInfoItem.h"

#include "GlycanEditorDialog.h"
#include "GlycanInfoModel.h"
#include "GlycansUiWidget.h"
#include "ModificationCommon.h"
#include "UnimodModifications.h"
#include "UnimodParser.h"

#include <common_errors.h>

#include <QComboBox>
#include <QCompleter>
#include <QFileDialog>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

using namespace pmi;

static const int TARGET_COMBOBOX_MIN_SYMBOLS = 25;

GlycanInfoItem::GlycanInfoItem(GlycansUiWidget *uiWidget, GlycanInfoModel *glycanInfoModel)
    : m_uiWidget(uiWidget)
    , m_glycanInfoModel(glycanInfoModel)
{
    m_glycanInfoModel->setParent(this);
}

GlycanInfoItem::~GlycanInfoItem()
{
}

GlycanInfoModel *GlycanInfoItem::model()
{
    return m_glycanInfoModel.data();
}

void GlycanInfoItem::createWidgets()
{
    const int row = m_uiWidget->glycanRow(this);

    m_deleteButton.reset(new QPushButton(QStringLiteral("Delete"), m_uiWidget));
    m_deleteButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    connect(m_deleteButton.data(), &QPushButton::clicked, [this](bool) { deleteLater(); });

    layout()->addWidget(m_deleteButton.data(), m_uiWidget->glycanRow(this), 5);

    createGlycanTypeComboBox();
    if (model()->dbMode()) {
        createGlycanDbComboBox();
    } else {
        createGlycanEditor();
    }
    createFineControlComboBox();
}

void GlycanInfoItem::trySetCurrentGlycanType(QComboBox *glycanTypeComboBox)
{
    const Qt::ItemDataRole dataRole = Qt::UserRole;
    setCurrentComboBoxIndexByRootIndex(glycanTypeComboBox, dataRole);
}

void GlycanInfoItem::trySetCurrentGlycanDb(QComboBox *glycanDbComboBox)
{
    const Qt::ItemDataRole dataRole = Qt::UserRole;
    const QVariant currentGlycanDbData
        = m_glycanInfoModel->data(m_glycanInfoModel->glycanDbParentIndex(), dataRole);
    if (currentGlycanDbData.isValid() && currentGlycanDbData.type() == QVariant::Int) {
        setCurrentComboBoxIndexByRootIndex(glycanDbComboBox, dataRole);
    }
    if (currentGlycanDbData.isValid() && currentGlycanDbData.type() == QVariant::String) {
        glycanDbComboBox->setEditText(currentGlycanDbData.toString());
    }
}

void GlycanInfoItem::trySetCurrentGlycan(QLineEdit *glycanEditor)
{
    const Qt::ItemDataRole dataRole = Qt::UserRole;
    const QVariant currentGlycanData
        = m_glycanInfoModel->data(m_glycanInfoModel->glycanParentIndex(), dataRole);
    glycanEditor->setText(currentGlycanData.toString());
}

void GlycanInfoItem::trySetCurrentFine(QComboBox *fineComboBox, QComboBox *modMaxComboBox)
{
    const Qt::ItemDataRole dataRole = Qt::UserRole;
    if (setCurrentComboBoxIndexByRootIndex(fineComboBox, dataRole)) {
        const QVariant currentFineData
            = m_glycanInfoModel->data(m_glycanInfoModel->fineParentIndex(), dataRole);
        const bool modMaxVisible = (currentFineData.toInt()
                                    == static_cast<int>(FineControl::FineType::CommonGreaterThan2));
        if (modMaxVisible) {
            setCurrentComboBoxIndexByRootIndex(modMaxComboBox, dataRole);
        }
    }
}

QGridLayout *GlycanInfoItem::layout() const
{
    return m_uiWidget->glycanLayout();
}

void GlycanInfoItem::createGlycanTypeComboBox()
{
    m_glycanTypeComboBox.reset(new QComboBox(m_uiWidget));
    m_glycanTypeComboBox->setModel(m_glycanInfoModel.data());
    m_glycanTypeComboBox->setRootModelIndex(m_glycanInfoModel->glycanTypeParentIndex());

    auto onGlycanTypeIndexChanged = [this](int index) {
        m_glycanInfoModel->setData(m_glycanInfoModel->glycanTypeParentIndex(), index, Qt::UserRole);
    };
    connect(m_glycanTypeComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onGlycanTypeIndexChanged);

    layout()->addWidget(m_glycanTypeComboBox.data(), m_uiWidget->glycanRow(this), 0);

    trySetCurrentGlycanType(m_glycanTypeComboBox.data());
}

void GlycanInfoItem::createGlycanDbComboBox()
{
    m_glycanDbComboBox.reset(new QComboBox(m_uiWidget));
    m_glycanDbComboBox->setModel(m_glycanInfoModel.data());
    m_glycanDbComboBox->setRootModelIndex(m_glycanInfoModel->glycanDbParentIndex());
    m_glycanDbComboBox->setEditable(true);

    auto onGlycanDbIndexChanged = [this](int index) {
        const QModelIndex modelIndex
            = m_glycanInfoModel->index(index, 0, m_glycanInfoModel->glycanDbParentIndex());
        QVariant data = m_glycanInfoModel->data(modelIndex, Qt::UserRole);
        if (data.isNull() || data.type() != QVariant::Int) {
            data.setValue<int>(-1);
        }
        m_glycanInfoModel->setData(m_glycanInfoModel->glycanDbParentIndex(), data, Qt::UserRole);
        m_glycanDbComboBox->setEditText(
            m_glycanInfoModel->data(modelIndex, Qt::DisplayRole).toString());
    };
    connect(m_glycanDbComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onGlycanDbIndexChanged);

    auto onGlycanDbTextEdited = [this](const QString &text) {
        m_glycanInfoModel->setData(m_glycanInfoModel->glycanDbParentIndex(), text, Qt::UserRole);
    };
    connect(m_glycanDbComboBox->lineEdit(), &QLineEdit::textEdited, onGlycanDbTextEdited);

    layout()->addWidget(m_glycanDbComboBox.data(), m_uiWidget->glycanRow(this), 1);

    m_glycanEditButton.reset(new QPushButton(QStringLiteral("..."), m_uiWidget));
    m_glycanEditButton->setMaximumWidth(m_glycanEditButton->height());
    connect(m_glycanEditButton.data(), &QPushButton::clicked, [this](bool) {
        QString filename = QFileDialog::getOpenFileName(
            m_uiWidget, QStringLiteral("Select Glycans File"), QString(),
            QStringLiteral("Glycan files(*.txt);;All Files(*.*)"));
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }
        m_glycanInfoModel->setData(m_glycanInfoModel->glycanDbParentIndex(), filename,
                                   Qt::UserRole);
        trySetCurrentGlycanDb(m_glycanDbComboBox.data());
    });
    layout()->addWidget(m_glycanEditButton.data(), m_uiWidget->glycanRow(this), 2);

    trySetCurrentGlycanDb(m_glycanDbComboBox.data());
}

void GlycanInfoItem::createGlycanEditor()
{
    m_glycanEditor.reset(new QLineEdit(m_uiWidget));

    auto onModificationTextChanged = [this](const QString &text) {
        m_glycanInfoModel->setData(m_glycanInfoModel->glycanParentIndex(), text, Qt::UserRole);
    };
    connect(m_glycanEditor.data(), &QLineEdit::textChanged, onModificationTextChanged);

    layout()->addWidget(m_glycanEditor.data(), m_uiWidget->glycanRow(this), 1);

    m_glycanEditButton.reset(new QPushButton(QStringLiteral("..."), m_uiWidget));
    m_glycanEditButton->setMaximumWidth(m_glycanEditButton->height());
    connect(m_glycanEditButton.data(), &QPushButton::clicked, [this](bool) {
        m_uiWidget->glycanEditorDialog()->startEdit(m_glycanEditor->text());
        connect(m_uiWidget->glycanEditorDialog(), &GlycanEditorDialog::accepted, this,
                &GlycanInfoItem::onGlycanEdited);
        connect(m_uiWidget->glycanEditorDialog(), &GlycanEditorDialog::rejected, this,
                &GlycanInfoItem::onGlycanEditCanceled);
    });
    layout()->addWidget(m_glycanEditButton.data(), m_uiWidget->glycanRow(this), 2);

    trySetCurrentGlycan(m_glycanEditor.data());
}

void GlycanInfoItem::onGlycanEdited()
{
    m_glycanEditor->setText(m_uiWidget->glycanEditorDialog()->glycan());
    m_uiWidget->glycanEditorDialog()->hide();
    disconnect(m_uiWidget->glycanEditorDialog(), &GlycanEditorDialog::accepted, this,
               &GlycanInfoItem::onGlycanEdited);
    disconnect(m_uiWidget->glycanEditorDialog(), &GlycanEditorDialog::rejected, this,
               &GlycanInfoItem::onGlycanEditCanceled);
}

void GlycanInfoItem::onGlycanEditCanceled()
{
    m_uiWidget->glycanEditorDialog()->hide();
    disconnect(m_uiWidget->glycanEditorDialog(), &GlycanEditorDialog::accepted, this,
               &GlycanInfoItem::onGlycanEdited);
    disconnect(m_uiWidget->glycanEditorDialog(), &GlycanEditorDialog::rejected, this,
               &GlycanInfoItem::onGlycanEditCanceled);
}

void GlycanInfoItem::createFineControlComboBox()
{
    m_fineControlComboBox.reset(new QComboBox(m_uiWidget));
    m_fineControlComboBox->setModel(m_glycanInfoModel.data());
    m_fineControlComboBox->setRootModelIndex(m_glycanInfoModel->fineParentIndex());
    m_fineControlComboBox->setCurrentIndex(0);

    m_modMaxComboBox.reset(new QComboBox(m_uiWidget));
    m_modMaxComboBox->setModel(m_glycanInfoModel.data());
    m_modMaxComboBox->setRootModelIndex(m_glycanInfoModel->modMaxParentIndex());
    m_modMaxComboBox->hide();
    m_modMaxComboBox->setCurrentIndex(0);

    auto onFineIndexChanged = [this](int index) {
        const int row = m_uiWidget->glycanRow(this);
        const QModelIndex modelIndex
            = m_glycanInfoModel->index(index, 0, m_glycanInfoModel->fineParentIndex());
        const QVariant data = m_glycanInfoModel->data(modelIndex, Qt::UserRole);
        const FineControl::FineType fine = FineControl::fromInt(data.toInt());
        const bool modMaxVisible = fine == FineControl::FineType::CommonGreaterThan2;
        m_modMaxComboBox->setVisible(modMaxVisible);
        m_glycanInfoModel->setData(m_glycanInfoModel->fineParentIndex(), static_cast<int>(fine),
                                   Qt::UserRole);
        if (modMaxVisible) {
            layout()->addWidget(m_fineControlComboBox.data(), row, 3, 1, 1);
            layout()->addWidget(m_modMaxComboBox.data(), row, 4, 1, 1);
        } else {
            layout()->addWidget(m_fineControlComboBox.data(), row, 3, 1, 2);
        }
    };
    connect(m_fineControlComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onFineIndexChanged);

    auto onModMaxIndexChanged = [this](int index) {
        const QModelIndex modelIndex
            = m_glycanInfoModel->index(index, 0, m_glycanInfoModel->modMaxParentIndex());
        const QVariant data = m_glycanInfoModel->data(modelIndex, Qt::UserRole);
        const int modMax = data.toInt();
        m_glycanInfoModel->setData(m_glycanInfoModel->modMaxParentIndex(), modMax, Qt::UserRole);
    };
    connect(m_modMaxComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onModMaxIndexChanged);

    layout()->addWidget(m_fineControlComboBox.data(), m_uiWidget->glycanRow(this), 3, 1, 2);

    trySetCurrentFine(m_fineControlComboBox.data(), m_modMaxComboBox.data());
}
