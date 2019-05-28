/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "ModificationInfoItem.h"

#include "ModificationComboBox.h"
#include "ModificationCommon.h"
#include "ModificationInfoModel.h"
#include "ModificationsUiWidget.h"
#include "MultiselectComboBox.h"
#include "UnimodModifications.h"

#include <common_errors.h>

#include <QComboBox>
#include <QCompleter>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>

using namespace pmi;

static const int TARGET_COMBOBOX_MIN_SYMBOLS = 25;

ModificationInfoItem::ModificationInfoItem(ModificationsUiWidget *uiWidget,
                                           ModificationInfoModel *modificationInfoModel)
    : m_uiWidget(uiWidget)
    , m_modificationInfoModel(modificationInfoModel)
{
    m_modificationInfoModel->setParent(this);
}

ModificationInfoItem::~ModificationInfoItem()
{
}

ModificationInfoModel *ModificationInfoItem::model()
{
    return m_modificationInfoModel.data();
}

void ModificationInfoItem::createWidgets()
{
    const int row = m_uiWidget->modificationRow(this);

    // tools
    QMenu *buttonMenu = new QMenu(m_uiWidget);

    m_invertModificationAction.reset(buttonMenu->addAction(QStringLiteral("Invert modification")));
    m_invertModificationAction->setEnabled(false);
    m_invertModificationAction->setCheckable(true);
    connect(m_invertModificationAction.data(), &QAction::toggled, m_modificationInfoModel.data(),
            &ModificationInfoModel::setInverted);
    auto checkModificationRootDataChanged
        = [this](const QModelIndex &topLeft, const QModelIndex &bottomRight,
                 const QVector<int> &roles) {
              if (topLeft == m_modificationInfoModel->modificationParentIndex()
                  && bottomRight == m_modificationInfoModel->modificationParentIndex()
                  && (roles.isEmpty() || roles.contains(Qt::UserRole))) {
                  QVariant data = m_modificationInfoModel->data(
                      m_modificationInfoModel->modificationParentIndex(), Qt::UserRole);
                  m_invertModificationAction->setEnabled(data.type() == QVariant::Int
                                                         && data.toInt() >= 0);
              }
          };
    connect(m_modificationInfoModel.data(), &ModificationInfoModel::dataChanged,
            checkModificationRootDataChanged);
    checkModificationRootDataChanged(m_modificationInfoModel->modificationParentIndex(),
                                     m_modificationInfoModel->modificationParentIndex(),
                                     QVector<int>({ Qt::UserRole }));

    QAction *addAttributeAction = buttonMenu->addAction(QStringLiteral("Add attribute"));
    connect(addAttributeAction, &QAction::triggered, addAttributeAction, &QAction::setEnabled);
    connect(addAttributeAction, &QAction::triggered, this,
            &ModificationInfoItem::createAttributeEditor);
    const QVariant currentAttributeData = m_modificationInfoModel->data(
        m_modificationInfoModel->attributeParentIndex(), Qt::UserRole);
    if (!currentAttributeData.toString().isEmpty()) {
        addAttributeAction->trigger();
    }

    QAction *deleteAction = buttonMenu->addAction(QStringLiteral("Delete"));
    connect(deleteAction, &QAction::triggered, [this]() { deleteLater(); });

    m_menuButton.reset(new QPushButton(m_uiWidget));
    m_menuButton->setText(QStringLiteral("..."));
    m_menuButton->setMenu(buttonMenu);
    m_menuButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);
    m_menuButton->setMaximumWidth(m_menuButton->height());

    layout()->addWidget(m_menuButton.data(), m_uiWidget->modificationRow(this), 4);

    createModificationsComboBox();
    createTargetsComboBox();
    createFineControlComboBox();
}

void ModificationInfoItem::trySetCurrentModification(QComboBox *modificationsComboBox)
{
    const Qt::ItemDataRole dataRole = Qt::UserRole;
    const QVariant currentModificationData = m_modificationInfoModel->data(
        m_modificationInfoModel->modificationParentIndex(), dataRole);
    if (currentModificationData.isValid() && currentModificationData.type() == QVariant::Int) {
        setCurrentComboBoxIndexByRootIndex(modificationsComboBox, dataRole);
        m_invertModificationAction->setChecked(m_modificationInfoModel->inverted());
    }
    if (currentModificationData.isValid() && currentModificationData.type() == QVariant::String) {
        modificationsComboBox->setEditText(currentModificationData.toString());
    }
}

void ModificationInfoItem::trySetCurrentFine(QComboBox *fineComboBox, QComboBox *modMaxComboBox)
{
    const Qt::ItemDataRole dataRole = Qt::UserRole;
    if (setCurrentComboBoxIndexByRootIndex(fineComboBox, dataRole)) {
        const QVariant currentFineData
            = m_modificationInfoModel->data(m_modificationInfoModel->fineParentIndex(), dataRole);
        const bool modMaxVisible = (currentFineData.toInt()
                                    == static_cast<int>(FineControl::FineType::CommonGreaterThan2));
        if (modMaxVisible) {
            setCurrentComboBoxIndexByRootIndex(modMaxComboBox, dataRole);
        }
    }
}

void ModificationInfoItem::trySetAttribute(QLineEdit *attributeEditor)
{
    const QVariant currentAttributeData = m_modificationInfoModel->data(
        m_modificationInfoModel->attributeParentIndex(), Qt::UserRole);
    attributeEditor->setText(currentAttributeData.toString());
}

QGridLayout *ModificationInfoItem::layout() const
{
    return m_uiWidget->modificationsLayout();
}

void ModificationInfoItem::createModificationsComboBox()
{
    m_modificationsComboBox.reset(new ModificationComboBox(m_uiWidget));
    m_modificationsComboBox->setModel(m_modificationInfoModel.data());
    m_modificationsComboBox->setRootModelIndex(m_modificationInfoModel->modificationParentIndex());
    m_modificationsComboBox->setEditable(true);

    auto onModificationIndexChanged = [this](int index) {
        const QModelIndex modelIndex = m_modificationInfoModel->index(
            index, 0, m_modificationInfoModel->modificationParentIndex());
        const QVariant data = m_modificationInfoModel->data(modelIndex, Qt::UserRole);
        m_modificationInfoModel->setData(m_modificationInfoModel->modificationParentIndex(), data,
                                         Qt::UserRole);
        m_modificationsComboBox->setEditText(
            m_modificationInfoModel->data(modelIndex, Qt::DisplayRole).toString());
    };
    connect(m_modificationsComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onModificationIndexChanged);

    auto onModificationTextEdited = [this](const QString &text) {
        m_modificationInfoModel->setData(m_modificationInfoModel->modificationParentIndex(), text,
                                         Qt::UserRole);
    };
    connect(m_modificationsComboBox->lineEdit(), &QLineEdit::textEdited, onModificationTextEdited);

    layout()->addWidget(m_modificationsComboBox.data(), m_uiWidget->modificationRow(this), 0);

    trySetCurrentModification(m_modificationsComboBox.data());
}

void ModificationInfoItem::createTargetsComboBox()
{
    m_targetsComboBox.reset(new MultiselectComboBox(m_uiWidget));

    m_targetsComboBox->setModel(m_modificationInfoModel.data());
    m_targetsComboBox->setRootModelIndex(m_modificationInfoModel->targetParentIndex());
    m_targetsComboBox->setFixedWidth(m_targetsComboBox->fontMetrics().averageCharWidth()
                                     * TARGET_COMBOBOX_MIN_SYMBOLS);
    auto checkTargetRootDataChanged
        = [this](const QModelIndex &topLeft, const QModelIndex &bottomRight,
                 const QVector<int> &roles) {
              if (topLeft == m_modificationInfoModel->targetParentIndex()
                  && bottomRight == m_modificationInfoModel->targetParentIndex()
                  && (roles.isEmpty() || roles.contains(Qt::DisplayRole))) {
                  m_targetsComboBox->update();
              }
          };
    connect(m_modificationInfoModel.data(), &ModificationInfoModel::dataChanged,
            checkTargetRootDataChanged);

    layout()->addWidget(m_targetsComboBox.data(), m_uiWidget->modificationRow(this), 1);
}

void ModificationInfoItem::createFineControlComboBox()
{
    m_fineControlComboBox.reset(new QComboBox(m_uiWidget));
    m_fineControlComboBox->setModel(m_modificationInfoModel.data());
    m_fineControlComboBox->setRootModelIndex(m_modificationInfoModel->fineParentIndex());
    m_fineControlComboBox->setCurrentIndex(0);

    m_modMaxComboBox.reset(new QComboBox(m_uiWidget));
    m_modMaxComboBox->setModel(m_modificationInfoModel.data());
    m_modMaxComboBox->setRootModelIndex(m_modificationInfoModel->modMaxParentIndex());
    m_modMaxComboBox->hide();
    m_modMaxComboBox->setCurrentIndex(0);

    auto onFineIndexChanged = [this](int index) {
        const QModelIndex modelIndex
            = m_modificationInfoModel->index(index, 0, m_modificationInfoModel->fineParentIndex());
        const QVariant data = m_modificationInfoModel->data(modelIndex, Qt::UserRole);
        const FineControl::FineType fine = FineControl::fromInt(data.toInt());
        const bool modMaxVisible = fine == FineControl::FineType::CommonGreaterThan2;
        m_modMaxComboBox->setVisible(modMaxVisible);
        m_modificationInfoModel->setData(m_modificationInfoModel->fineParentIndex(),
                                         static_cast<int>(fine), Qt::UserRole);
        if (modMaxVisible) {
            layout()->addWidget(m_fineControlComboBox.data(), m_uiWidget->modificationRow(this), 2,
                                1, 1);
            layout()->addWidget(m_modMaxComboBox.data(), m_uiWidget->modificationRow(this), 3, 1,
                                1);
        } else {
            layout()->addWidget(m_fineControlComboBox.data(), m_uiWidget->modificationRow(this), 2,
                                1, 2);
        }
    };
    connect(m_fineControlComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onFineIndexChanged);

    auto onModMaxIndexChanged = [this](int index) {
        const QModelIndex modelIndex = m_modificationInfoModel->index(
            index, 0, m_modificationInfoModel->modMaxParentIndex());
        const QVariant data = m_modificationInfoModel->data(modelIndex, Qt::UserRole);
        const int modMax = data.toInt();
        m_modificationInfoModel->setData(m_modificationInfoModel->modMaxParentIndex(), modMax,
                                         Qt::UserRole);
    };
    connect(m_modMaxComboBox.data(),
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            onModMaxIndexChanged);

    layout()->addWidget(m_fineControlComboBox.data(), m_uiWidget->modificationRow(this), 2, 1, 2);

    trySetCurrentFine(m_fineControlComboBox.data(), m_modMaxComboBox.data());
}

void ModificationInfoItem::createAttributeEditor()
{
    m_attributeEditor.reset(new QLineEdit(m_uiWidget));
    layout()->addWidget(m_attributeEditor.data(), m_uiWidget->modificationRow(this), 5);
    connect(m_attributeEditor.data(), &QLineEdit::textChanged, [this](const QString &text) {
        m_modificationInfoModel->setData(m_modificationInfoModel->attributeParentIndex(), text,
                                         Qt::UserRole);
    });

    trySetAttribute(m_attributeEditor.data());
}
