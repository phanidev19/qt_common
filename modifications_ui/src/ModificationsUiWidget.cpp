/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "ModificationsUiWidget.h"

#include "ModificationCommon.h"
#include "ModificationInfoItem.h"
#include "ModificationInfoModel.h"
#include "UnimodModifications.h"
#include "UnimodParser.h"
#include "pmi_modifications_ui_debug.h"

#include <common_errors.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QScrollArea>
#include <QScrollBar>

using namespace pmi;

static const int CUSTOM_ATTRIBUTES_EDITOR_HEIGHT = 10;

ModificationsUiWidget::ModificationsUiWidget(bool proteomeDiscoverer_2_0,
                                             bool proteomeDiscoverer_1_4, QWidget *parent)
    : QWidget(parent)
    , m_proteomeDiscoverer_2_0(proteomeDiscoverer_2_0)
    , m_proteomeDiscoverer_1_4(proteomeDiscoverer_1_4)
    , m_modificationsLayout(nullptr)
    , m_modificationsScrollArea(nullptr)
    , m_textEdit(nullptr)
    , m_doNotCreateEmptyRow(false)
{
    m_modifications.reset(new UnimodModifications());

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString unimodXmlName = QStringLiteral("data/unimod.xml");
    const QString unimodXmlPath = appDir.absoluteFilePath(unimodXmlName);

    UnimodParser parser;
    parser.parse(unimodXmlPath, m_modifications.data());

    initUi();

    addModificationInfoControl();

    const int newMinimumWidth = m_modificationsLayout->sizeHint().width();
    if (minimumWidth() < newMinimumWidth) {
        setMinimumWidth(newMinimumWidth);
    }
}

ModificationsUiWidget::~ModificationsUiWidget()
{
    removeAllModifications();
}

void ModificationsUiWidget::setModStrings(const QStringList &strings)
{
    m_appliedModStrings = strings;

    static const QString commentPrefix = QStringLiteral("%");

    removeAllModifications();
    m_textEdit->clear();

    for (QString str : strings) {
        ModificationInfoModel *model = ModificationInfoModel::createFromString(
            m_modifications, m_proteomeDiscoverer_2_0, m_proteomeDiscoverer_1_4, str);
        if (model == nullptr) {
            if (!str.startsWith(commentPrefix)) {
                m_textEdit->appendPlainText(str.trimmed());
            }
            continue;
        }
        addModificationInfoControl(model);
    }
    addModificationInfoControl();

    const int newMinimumWidth = m_modificationsLayout->sizeHint().width();
    if (minimumWidth() < newMinimumWidth) {
        setMinimumWidth(newMinimumWidth);
    }
}

QStringList ModificationsUiWidget::modStrings() const
{
    static const QString customTextComment = QStringLiteral("% Custom modification text below");

    QStringList result;

    for (auto item : m_modificationItems) {
        if (!item->model()->isCompleted()) {
            continue;
        }
        result << item->model()->toByonicString();
    }
    result << customTextComment;
    result.append(m_textEdit->toPlainText().trimmed().split('\n'));

    return result;
}

QStringList ModificationsUiWidget::appliedModStrings() const
{
    return m_appliedModStrings;
}

void ModificationsUiWidget::initUi()
{
    setLayout(new QVBoxLayout());

    initModificationsUi();

    layout()->addWidget(
        new QLabel(QStringLiteral("Enter custom modification text in fine control format:")));

    m_textEdit = new QPlainTextEdit();
    m_textEdit->setFixedHeight(m_textEdit->fontMetrics().height()
                               * CUSTOM_ATTRIBUTES_EDITOR_HEIGHT);
    layout()->addWidget(m_textEdit);

    initButtons();
}

void ModificationsUiWidget::initButtons()
{
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    QPushButton *loadButton = new QPushButton(QStringLiteral("Load ..."), this);
    connect(loadButton, &QPushButton::clicked, [this](bool) {
        QString filename = QFileDialog::getOpenFileName(
            this, QStringLiteral("Open file"), QString(),
            QStringLiteral("Modifications files(*.txt);;All Files(*.*)"));
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }
        QStringList modifications;
        while (!file.atEnd()) {
            modifications << QString::fromLatin1(file.readLine());
        }
        setModStrings(modifications);
    });
    QPushButton *saveButton = new QPushButton(QStringLiteral("Save ..."), this);
    connect(saveButton, &QPushButton::clicked, [this](bool) {
        QString filename = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save file"), QString(),
            QStringLiteral("Modifications files(*.txt);;All Files(*.*)"));

        const QStringList modifications = modStrings();
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return;
        }

        QTextStream out(&file);
        for (auto line : modifications) {
            out << line.toLatin1() << '\n';
        }

        if (m_proteomeDiscoverer_1_4) {
            m_appliedModStrings = modStrings();
            Q_EMIT saveAndExitClicked(filename);
        }
    });

    buttonsLayout->addWidget(loadButton);
    buttonsLayout->addWidget(saveButton);
    buttonsLayout->addSpacerItem(
        new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Maximum));

    if (!m_proteomeDiscoverer_1_4) {
        QPushButton *okButton = new QPushButton(QStringLiteral("Ok"), this);
        connect(okButton, &QPushButton::clicked, [this](bool) {
            m_appliedModStrings = modStrings();
            Q_EMIT accepted();
        });
        QPushButton *cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
        connect(cancelButton, &QPushButton::clicked, [this](bool) { Q_EMIT rejected(); });

        buttonsLayout->addWidget(okButton);
        buttonsLayout->addWidget(cancelButton);
    }

    layout()->addItem(buttonsLayout);
}

void ModificationsUiWidget::initModificationsUi()
{
    m_modificationsScrollArea = new QScrollArea();
    m_modificationsScrollArea->setWidgetResizable(true);
    m_modificationsScrollArea->setFrameStyle(QFrame::NoFrame);
    m_modificationsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_modificationsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QWidget *modificationsContainer = new QWidget();
    modificationsContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_modificationsLayout = new QGridLayout(modificationsContainer);

    m_modificationsLayout->addWidget(new QLabel(QStringLiteral("Modifications")), 0, 0);
    m_modificationsLayout->addWidget(new QLabel(QStringLiteral("Targets")), 0, 1);
    m_modificationsLayout->addWidget(new QLabel(QStringLiteral("Fine Control")), 0, 2);
    m_modificationsLayout->addItem(
        new QSpacerItem(1, 1, QSizePolicy::Maximum, QSizePolicy::Expanding), 1, 0);
    m_modificationsLayout->addItem(
        new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Maximum), 1, 6);

    m_modificationsScrollArea->setWidget(modificationsContainer);
    modificationsContainer->installEventFilter(this);

    layout()->addWidget(m_modificationsScrollArea);
}

QGridLayout *ModificationsUiWidget::modificationsLayout()
{
    return m_modificationsLayout;
}

int ModificationsUiWidget::modificationRow(ModificationInfoItem *item) const
{
    for (int i = 0; i < m_modificationItems.count(); ++i) {
        if (m_modificationItems[i] == item) {
            // first row is column captions
            return i + 1;
        }
    }
    return -1;
}

void ModificationsUiWidget::addModificationInfoControl()
{
    addModificationInfoControl(new ModificationInfoModel(m_modifications, m_proteomeDiscoverer_2_0,
                                                         m_proteomeDiscoverer_1_4));
}

void ModificationsUiWidget::addModificationInfoControl(ModificationInfoModel *model)
{
    if (!m_modificationItems.isEmpty()) {
        disconnect(m_modificationItems.last()->model(), &ModificationInfoModel::modelIsCompleted,
                   this,
                   static_cast<void (ModificationsUiWidget::*)()>(
                       &ModificationsUiWidget::addModificationInfoControl));
    }

    ModificationInfoItem *item = new ModificationInfoItem(this, model);
    m_modificationItems.push_back(item);
    insertRow(m_modificationsLayout, modificationRow(item));
    item->createWidgets();

    connect(m_modificationItems.last()->model(), &ModificationInfoModel::modelIsCompleted, this,
            static_cast<void (ModificationsUiWidget::*)()>(
                &ModificationsUiWidget::addModificationInfoControl));

    connect(m_modificationItems.last(), &ModificationInfoItem::destroyed, [this](QObject *object) {
        for (int i = 0; i < m_modificationItems.count(); ++i) {
            if (m_modificationItems[i] == object) {
                removeRow(m_modificationsLayout, modificationRow(m_modificationItems[i]));
                m_modificationItems.removeAt(i);
                break;
            }
        }
        if (!m_doNotCreateEmptyRow
            && (m_modificationItems.isEmpty()
                || m_modificationItems.last()->model()->isCompleted())) {
            addModificationInfoControl();
        }
    });

    QWidget *modificationsWidget = m_modificationsLayout->parentWidget();
    modificationsWidget->resize(modificationsWidget->sizeHint());
}

void ModificationsUiWidget::removeAllModifications()
{
    QScopedValueRollback<bool> doNotCreateEmptyRow(m_doNotCreateEmptyRow, true);
    for (int i = m_modificationItems.size() - 1; i >= 0; --i) {
        delete m_modificationItems[i];
    }
    m_modificationItems.clear();
}

bool ModificationsUiWidget::eventFilter(QObject *object, QEvent *event)
{
    QWidget *modificationsContainer = m_modificationsScrollArea->widget();
    if (object == modificationsContainer && event->type() == QEvent::Resize) {
        setMinimumWidth(modificationsContainer->minimumSizeHint().width()
                        + m_modificationsScrollArea->verticalScrollBar()->width());
    }
    return false;
}
