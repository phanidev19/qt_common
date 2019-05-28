/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "GlycansUiWidget.h"

#include "GlycanEditorDialog.h"
#include "GlycanInfoItem.h"
#include "GlycanInfoModel.h"
#include "ModificationCommon.h"
#include "UnimodModifications.h"
#include "pmi_modifications_ui_debug.h"

#include <common_errors.h>

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
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

struct GridItemPosition {
    int row = 0;
    int col = 0;
    int rowSpan = 1;
    int colSpan = 1;
};

GlycansUiWidget::GlycansUiWidget(bool proteomeDiscoverer_2_0, bool proteomeDiscoverer_1_4,
                                 QWidget *parent)
    : QWidget(parent)
    , m_proteomeDiscoverer_2_0(proteomeDiscoverer_2_0)
    , m_proteomeDiscoverer_1_4(proteomeDiscoverer_1_4)
    , m_glycansLayout(nullptr)
    , m_glycansScrollArea(nullptr)
    , m_textEdit(nullptr)
    , m_doNotCreateEmptyRow(false)
    , m_glycanEditorDialog(nullptr)
{
    initUi();

    addGlycanDbInfoControl();
    addGlycanNonDbInfoControl();

    const int newMinimumWidth = m_glycansLayout->sizeHint().width();
    if (minimumWidth() < newMinimumWidth) {
        setMinimumWidth(newMinimumWidth);
    }
}

GlycansUiWidget::~GlycansUiWidget()
{
    removeAllGlycans();
}

void GlycansUiWidget::setGlycanDbPaths(const QStringList &paths)
{
    m_glycanDbFiles.clear();

    for (auto path : paths) {
        if (QFileInfo(path).isDir()) {
            QDirIterator it(path, QDir::Files);

            while (it.hasNext()) {
                const QString item = it.next();
                const QString suffix = QFileInfo(item).suffix().toLower();

                if (suffix != QStringLiteral("txt")) {
                    continue;
                }

                m_glycanDbFiles << qMakePair(item, QFileInfo(item).completeBaseName());
            }
        } else {
            m_glycanDbFiles << qMakePair(path, QFileInfo(path).completeBaseName());
        }
    }

    for (auto item : m_glycanDbItems) {
        item->model()->setGlycanDbFiles(m_glycanDbFiles);
    }
}

void GlycansUiWidget::setGlycansStrings(const QStringList &strings)
{
    m_appliedModStrings = strings;

    static const QString commentPrefix = QStringLiteral("%");

    removeAllGlycans();
    m_textEdit->clear();

    for (QString str : strings) {
        GlycanInfoModel *model = GlycanInfoModel::createFromString(m_proteomeDiscoverer_2_0,
                                                                   m_proteomeDiscoverer_1_4, str);
        if (model == nullptr) {
            if (!str.startsWith(commentPrefix)) {
                m_textEdit->appendPlainText(str.trimmed());
            }
            continue;
        }
        addGlycanInfoControl(model);
    }
    addGlycanDbInfoControl();
    addGlycanNonDbInfoControl();

    const int newMinimumWidth = m_glycansLayout->sizeHint().width();
    if (minimumWidth() < newMinimumWidth) {
        setMinimumWidth(newMinimumWidth);
    }
}

QStringList GlycansUiWidget::glycansStrings() const
{
    static const QString customTextComment = QStringLiteral("% Custom modification text below");

    QStringList result;

    for (auto item : m_glycanDbItems) {
        if (!item->model()->isCompleted()) {
            continue;
        }
        result << item->model()->toByonicString();
    }
    for (auto item : m_glycanItems) {
        if (!item->model()->isCompleted()) {
            continue;
        }
        result << item->model()->toByonicString();
    }
    result << customTextComment;
    result.append(m_textEdit->toPlainText().trimmed().split('\n'));

    return result;
}

QStringList GlycansUiWidget::appliedGlycansStrings() const
{
    return m_appliedModStrings;
}

GlycanEditorDialog *GlycansUiWidget::glycanEditorDialog()
{
    if (m_glycanEditorDialog == nullptr) {
        m_glycanEditorDialog = new GlycanEditorDialog(this);
    }
    return m_glycanEditorDialog;
}

void GlycansUiWidget::initUi()
{
    setLayout(new QVBoxLayout());

    initGlycansUi();

    layout()->addWidget(
        new QLabel(QStringLiteral("Enter custom glycan text in fine control format:")));

    m_textEdit = new QPlainTextEdit();
    m_textEdit->setFixedHeight(m_textEdit->fontMetrics().height()
                               * CUSTOM_ATTRIBUTES_EDITOR_HEIGHT);
    layout()->addWidget(m_textEdit);

    initButtons();
}

void GlycansUiWidget::initButtons()
{
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    QPushButton *loadButton = new QPushButton(QStringLiteral("Load ..."), this);
    connect(loadButton, &QPushButton::clicked, [this](bool) {
        QString filename
            = QFileDialog::getOpenFileName(this, QStringLiteral("Open file"), QString(),
                                           QStringLiteral("Glycan files(*.txt);;All Files(*.*)"));
        QFile file(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }
        QStringList modifications;
        while (!file.atEnd()) {
            modifications << QString::fromLatin1(file.readLine());
        }
        setGlycansStrings(modifications);
    });
    QPushButton *saveButton = new QPushButton(QStringLiteral("Save ..."), this);
    connect(saveButton, &QPushButton::clicked, [this](bool) {
        QString filename
            = QFileDialog::getSaveFileName(this, QStringLiteral("Save file"), QString(),
                                           QStringLiteral("Glycan files(*.txt);;All Files(*.*)"));

        const QStringList modifications = glycansStrings();
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            return;
        }

        QTextStream out(&file);
        for (auto line : modifications) {
            out << line.toLatin1() << '\n';
        }

        if (m_proteomeDiscoverer_1_4) {
            m_appliedModStrings = glycansStrings();
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
            m_appliedModStrings = glycansStrings();
            Q_EMIT accepted();
        });
        QPushButton *cancelButton = new QPushButton(QStringLiteral("Cancel"), this);
        connect(cancelButton, &QPushButton::clicked, [this](bool) { Q_EMIT rejected(); });

        buttonsLayout->addWidget(okButton);
        buttonsLayout->addWidget(cancelButton);
    }

    layout()->addItem(buttonsLayout);
}

void GlycansUiWidget::initGlycansUi()
{
    m_glycansScrollArea = new QScrollArea();
    m_glycansScrollArea->setWidgetResizable(true);
    m_glycansScrollArea->setFrameStyle(QFrame::NoFrame);
    m_glycansScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_glycansScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_glycansScrollArea->setMinimumHeight(fontMetrics().height() * 15);

    QWidget *glycansContainer = new QWidget();
    m_glycansLayout = new QGridLayout(glycansContainer);
    glycansContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_glycansScrollArea->setWidget(glycansContainer);
    glycansContainer->installEventFilter(this);

    layout()->addWidget(m_glycansScrollArea);

    int startRow = 0;
    for (bool dbArea : { true, false }) {
        const QString glycanTitleText
            = dbArea ? QStringLiteral("Glycan database") : QStringLiteral("Glycan");

        m_glycansLayout->addWidget(new QLabel(QStringLiteral("Glycan type")), startRow, 0);
        m_glycansLayout->addWidget(new QLabel(glycanTitleText), startRow, 1, 1, 2);
        m_glycansLayout->addWidget(new QLabel(QStringLiteral("Fine Control")), startRow, 3, 1, 2);

        QSpacerItem *spacer = dbArea
            ? (new QSpacerItem(1, fontMetrics().height() * 2, QSizePolicy::Expanding,
                               QSizePolicy::Fixed))
            : (new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Expanding));
        m_glycansLayout->addItem(spacer, startRow + 1, 6);

        startRow += 2;
    }
}

QGridLayout *GlycansUiWidget::glycanLayout()
{
    return m_glycansLayout;
}

int GlycansUiWidget::glycanRow(GlycanInfoItem *item) const
{
    int rowShift = 0;
    // first row is caption
    rowShift++;
    for (int i = 0; i < m_glycanDbItems.count(); ++i) {
        if (m_glycanDbItems[i] == item) {
            return i + rowShift;
        }
    }
    // all dbItems
    rowShift += m_glycanDbItems.count();
    // spacer and caption
    rowShift += 2;
    for (int i = 0; i < m_glycanItems.count(); ++i) {
        if (m_glycanItems[i] == item) {
            return i + rowShift;
        }
    }
    return -1;
}

void GlycansUiWidget::addGlycanDbInfoControl()
{
    addGlycanInfoControl(
        new GlycanInfoModel(m_proteomeDiscoverer_2_0, m_proteomeDiscoverer_1_4, true));
}

void GlycansUiWidget::addGlycanNonDbInfoControl()
{
    addGlycanInfoControl(
        new GlycanInfoModel(m_proteomeDiscoverer_2_0, m_proteomeDiscoverer_1_4, false));
}

void GlycansUiWidget::addGlycanInfoControl(GlycanInfoModel *model)
{
    const bool dbMode = model->dbMode();
    model->setGlycanDbFiles(m_glycanDbFiles);

    auto &glycanItems = dbMode ? m_glycanDbItems : m_glycanItems;
    auto callbackPtr = dbMode
        ? static_cast<void (GlycansUiWidget::*)()>(&GlycansUiWidget::addGlycanDbInfoControl)
        : static_cast<void (GlycansUiWidget::*)()>(&GlycansUiWidget::addGlycanNonDbInfoControl);

    if (!glycanItems.isEmpty()) {
        disconnect(glycanItems.last()->model(), &GlycanInfoModel::modelIsCompleted, this,
                   callbackPtr);
    }

    GlycanInfoItem *item = new GlycanInfoItem(this, model);
    glycanItems.push_back(item);
    insertRow(glycanLayout(), glycanRow(item));
    item->createWidgets();

    connect(glycanItems.last()->model(), &GlycanInfoModel::modelIsCompleted, this, callbackPtr);

    connect(glycanItems.last(), &GlycanInfoItem::destroyed, [this, dbMode](QObject *object) {
        auto &glycanItems = dbMode ? m_glycanDbItems : m_glycanItems;
        for (int i = 0; i < glycanItems.count(); ++i) {
            GlycanInfoItem *item = glycanItems[i];
            if (item == object) {
                removeRow(glycanLayout(), glycanRow(item));
                glycanItems.removeAt(i);
                break;
            }
        }
        if (!m_doNotCreateEmptyRow
            && (glycanItems.isEmpty() || glycanItems.last()->model()->isCompleted())) {
            addGlycanInfoControl(
                new GlycanInfoModel(m_proteomeDiscoverer_2_0, m_proteomeDiscoverer_1_4, dbMode));
        }
    });

    QWidget *modificationsWidget = glycanLayout()->parentWidget();
    modificationsWidget->resize(modificationsWidget->sizeHint());
}

void GlycansUiWidget::removeAllGlycans()
{
    QScopedValueRollback<bool> doNotCreateEmptyRow(m_doNotCreateEmptyRow, true);
    for (int i = m_glycanDbItems.size() - 1; i >= 0; --i) {
        delete m_glycanDbItems[i];
    }
    m_glycanDbItems.clear();
    for (int i = m_glycanItems.size() - 1; i >= 0; --i) {
        delete m_glycanItems[i];
    }
    m_glycanItems.clear();
}

bool GlycansUiWidget::eventFilter(QObject *object, QEvent *event)
{
    QWidget *glycansContainer1 = m_glycansScrollArea->widget();
    if ((object == glycansContainer1) && event->type() == QEvent::Resize) {
        setMinimumWidth(glycansContainer1->minimumSizeHint().width()
                        + m_glycansScrollArea->verticalScrollBar()->width());
    }
    return false;
}
