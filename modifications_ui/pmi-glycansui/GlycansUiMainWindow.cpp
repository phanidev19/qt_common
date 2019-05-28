/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "GlycansUiMainWindow.h"

#include "CinReader.h"
#include "GlycansUiWidget.h"

#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QGuiApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <chrono>

static const std::chrono::milliseconds STDIN_READ_TIMEOUT = std::chrono::milliseconds(1000);
static const int INITIAL_WINDOW_WIDTH = 100;
static const int INITIAL_WINDOW_HEIGHT = 30;

GlycansUiMainWindow::GlycansUiMainWindow(bool proteomeDiscoverer_2_0, bool proteomeDiscoverer_1_4,
                                         bool readDataFromStdin, const QStringList &glycanDbPaths,
                                         QWidget *parent)
    : QMainWindow(parent)
    , m_proteomeDiscoverer_2_0(proteomeDiscoverer_2_0)
    , m_proteomeDiscoverer_1_4(proteomeDiscoverer_1_4)
    , m_glycanDbPaths(glycanDbPaths)
    , m_glycansUi(nullptr)
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

GlycansUiMainWindow::~GlycansUiMainWindow()
{
    if (!m_cinReader.isNull()) {
        m_cinReader->quit();
        m_cinReader->wait();
    }
}

QStringList GlycansUiMainWindow::glycansStrings() const
{
    if (m_glycansUi == nullptr) {
        return QStringList();
    }
    return m_glycansUi->appliedGlycansStrings();
}

void GlycansUiMainWindow::timerEvent(QTimerEvent *event)
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

void GlycansUiMainWindow::buildUi()
{
    setMinimumSize(QSize(0, 0));

    m_glycansUi = new GlycansUiWidget(m_proteomeDiscoverer_2_0, m_proteomeDiscoverer_1_4, this);

    m_glycansUi->setGlycanDbPaths(m_glycanDbPaths);

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
            m_glycansUi->setGlycansStrings(modStrings);
        }
    }

    QWidget *prevCentralWidget = takeCentralWidget();
    if (prevCentralWidget != nullptr) {
        prevCentralWidget->deleteLater();
    }
    setCentralWidget(m_glycansUi);

    connect(m_glycansUi, &GlycansUiWidget::rejected, this, &GlycansUiMainWindow::close);
    if (m_proteomeDiscoverer_1_4) {
        connect(m_glycansUi, &GlycansUiWidget::saveAndExitClicked,
                [this](const QString &filename) { showInstructionsForCopyingToPD_1_4(filename); });
    } else {
        connect(m_glycansUi, &GlycansUiWidget::accepted, this, &GlycansUiMainWindow::close);
    }
}

void GlycansUiMainWindow::showInstructionsForCopyingToPD_1_4(const QString &savedFilename)
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
    const QString textTemplate = QStringLiteral(
        "The glycans filename (%1) has been auto-saved to the clipboard.\n\n"
        "Paste (Ctrl-V) the glycans filename into the location highlighted by the red arrow.");
    label->setText(textTemplate.arg(savedFilename));
    layout->addWidget(label);
    QLabel *imageWidget = new QLabel(instructionsDialog);
    imageWidget->setPixmap(QStringLiteral(":resources/GlycansFileParameter.png"));
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
    connect(instructionsDialog, &QDialog::accepted, this, &GlycansUiMainWindow::close);
    connect(instructionsDialog, &QDialog::rejected, this, &GlycansUiMainWindow::close);

    instructionsDialog->open();
}
