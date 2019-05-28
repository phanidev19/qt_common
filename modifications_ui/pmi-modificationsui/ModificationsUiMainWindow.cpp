/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "ModificationsUiMainWindow.h"

#include "CinReader.h"
#include "ModificationsUiWidget.h"

#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <chrono>

static const std::chrono::milliseconds STDIN_READ_TIMEOUT = std::chrono::milliseconds(500);
static const int INITIAL_WINDOW_WIDTH = 100;
static const int INITIAL_WINDOW_HEIGHT = 30;

ModificationsUiMainWindow::ModificationsUiMainWindow(bool proteomeDiscoverer_2_0,
                                                     bool proteomeDiscoverer_1_4,
                                                     bool readDataFromStdin, QWidget *parent)
    : QMainWindow(parent)
    , m_proteomeDiscoverer_2_0(proteomeDiscoverer_2_0)
    , m_proteomeDiscoverer_1_4(proteomeDiscoverer_1_4)
    , m_modificationsUi(nullptr)
    , m_stopStdinReadingTimerId(-1)
{
    if (readDataFromStdin) {
        setMinimumSize(QSize(fontMetrics().averageCharWidth() * INITIAL_WINDOW_WIDTH,
                             fontMetrics().height() * INITIAL_WINDOW_HEIGHT));

        QLabel *label = new QLabel(
            QStringLiteral("Read data from standard input stream.\nPlease wait."), this);
        label->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        QFont font = label->font();
        font.setPointSizeF(label->font().pointSizeF() + 2);
        label->setFont(font);
        setCentralWidget(label);

        m_stopStdinReadingTimerId = startTimer(STDIN_READ_TIMEOUT.count(), Qt::PreciseTimer);
        m_cinReader.reset(new CinReader(&m_stdinBuffer, this));
        connect(m_cinReader.data(), &CinReader::finished, [this]() { m_cinReader.reset(nullptr); });
        m_cinReader->start();
    } else {
        buildUi();
    }
}

ModificationsUiMainWindow::~ModificationsUiMainWindow()
{
    if (!m_cinReader.isNull()) {
        m_cinReader->quit();
        m_cinReader->wait();
    }
}

QStringList ModificationsUiMainWindow::modStrings() const
{
    if (m_modificationsUi == nullptr) {
        return QStringList();
    }
    return m_modificationsUi->appliedModStrings();
}

void ModificationsUiMainWindow::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_stopStdinReadingTimerId) {
        if (!m_cinReader.isNull()) {
            m_cinReader->quit();
        }
        buildUi();

        killTimer(m_stopStdinReadingTimerId);
        m_stopStdinReadingTimerId = -1;
    }
}

void ModificationsUiMainWindow::buildUi()
{
    setMinimumSize(QSize(0, 0));

    m_modificationsUi
        = new ModificationsUiWidget(m_proteomeDiscoverer_2_0, m_proteomeDiscoverer_1_4, this);

    if (!m_stdinBuffer.isEmpty()) {
        // make a copy of m_stdinBuffer
        QByteArray buffer(m_stdinBuffer);
        QByteArrayList byteStrings = buffer.split('\n');
        QStringList modStrings;
        for (const QByteArray &bytes : byteStrings) {
            const QString strLine = QString::fromLatin1(bytes);
            if (!strLine.trimmed().isEmpty()) {
                modStrings << strLine;
            }
        }
        if (!modStrings.isEmpty()) {
            m_modificationsUi->setModStrings(modStrings);
        }
    }

    QWidget *prevCentralWidget = takeCentralWidget();
    if (prevCentralWidget != nullptr) {
        prevCentralWidget->deleteLater();
    }
    setCentralWidget(m_modificationsUi);

    connect(m_modificationsUi, &ModificationsUiWidget::rejected, this,
            &ModificationsUiMainWindow::close);
    if (m_proteomeDiscoverer_1_4) {
        connect(m_modificationsUi, &ModificationsUiWidget::saveAndExitClicked,
                [this](const QString &filename) { showInstructionsForCopyingToPD_1_4(filename); });
    } else {
        connect(m_modificationsUi, &ModificationsUiWidget::accepted, this,
                &ModificationsUiMainWindow::close);
    }
}

void ModificationsUiMainWindow::showInstructionsForCopyingToPD_1_4(const QString &savedFilename)
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(savedFilename);

    QDialog *instructionsDialog = new QDialog(this);
    instructionsDialog->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(instructionsDialog);
    instructionsDialog->setLayout(layout);
    instructionsDialog->setMinimumSize(200, 200);
    QLabel *label = new QLabel(instructionsDialog);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 2.0);
    label->setFont(font);
    const QString textTemplate
        = QStringLiteral("The modifications filename (%1) has been auto-saved to the clipboard.\n\n"
                         "Paste (Ctrl-V) the modifications filename into the location highlighted "
                         "by the red arrow.");
    label->setText(textTemplate.arg(savedFilename));
    layout->addWidget(label);
    QLabel *imageWidget = new QLabel(instructionsDialog);
    imageWidget->setPixmap(QStringLiteral(":resources/ModificationsFileParameter.png"));
    layout->addWidget(imageWidget);

    QDialogButtonBox *buttonsBox = new QDialogButtonBox(instructionsDialog);
    buttonsBox->setStandardButtons(QDialogButtonBox::Ok);
    buttonsBox->setCenterButtons(true);
    layout->addWidget(buttonsBox);

    QPushButton *okButton = buttonsBox->button(QDialogButtonBox::Ok);
    connect(okButton, &QPushButton::clicked, [this, instructionsDialog](bool) {
        instructionsDialog->close();
        close();
    });

    instructionsDialog->setModal(true);
    connect(instructionsDialog, &QDialog::accepted, this, &ModificationsUiMainWindow::close);
    connect(instructionsDialog, &QDialog::rejected, this, &ModificationsUiMainWindow::close);

    instructionsDialog->open();
}
