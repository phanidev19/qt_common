/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "MSReaderMainWindow.h"
#include "MSReader.h"
#include "MSReaderWidget.h"
#include <QApplication>
#include <QFileDialog>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>

namespace
{
const int MAINWINDOWS_MINIMUM_WIDTH = 800;
const int MAINWINDOWS_MINIMUM_HEIGHT = 600;
}


MSReaderMainWindow::MSReaderMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_menuBar(new QMenuBar(this))
{
    QMenu *fileMenu = new QMenu(tr("&File"), this);
    fileMenu->setObjectName("MSReaderFileMenu");
    fileMenu->addAction(tr("&Open"), this, SLOT(openFile()),
                        QKeySequence(tr("Ctrl+O", "File|Open")));
    fileMenu->addAction(tr("Open &Project Folder"), this, SLOT(openProjectFolder()),
                        QKeySequence(tr("Ctrl+P", "File|Open Project Folder")));
    fileMenu->addAction(tr("&Close"), this, SLOT(closeTab()),
                        QKeySequence(tr("Ctrl+W", "File|Close")));
    fileMenu->addAction(tr("E&xit"), qApp, SLOT(quit()));

    QMenu *editMenu = new QMenu(tr("&Edit"), this);
    editMenu->setObjectName("MSReaderEditMenu");

    m_displayCentroidDataAction = new QAction(tr("&Display centroid data"), editMenu);
    m_displayCentroidDataAction->setCheckable(true);
    connect(m_displayCentroidDataAction, &QAction::changed, this,
            &MSReaderMainWindow::centroidStateChanged);

    connect(m_tabWidget, &QTabWidget::currentChanged, this, &MSReaderMainWindow::currentTabChanged);

    editMenu->addAction(m_displayCentroidDataAction);

    m_xicDataAction = editMenu->addAction(tr("XIC &Data"), this, SLOT(xicData()),
                        QKeySequence(tr("Ctrl+D", "Edit|XIC Data")));
    m_xicDataAction->setEnabled(false);

    m_chromatogramAction = editMenu->addAction(
        tr("Get Chro&matogram"), this, SLOT(getChromatogram()),
        QKeySequence(tr("Ctrl+M", "Edit|Get Chromatogram")));
    m_chromatogramAction->setEnabled(false);

    m_menuBar->addMenu(fileMenu);
    m_menuBar->addMenu(editMenu);
    setMenuBar(m_menuBar);

    setCentralWidget(m_tabWidget);

    setMinimumWidth(MAINWINDOWS_MINIMUM_WIDTH);
    setMinimumWidth(MAINWINDOWS_MINIMUM_HEIGHT);
}

MSReaderMainWindow::~MSReaderMainWindow()
{
    pmi::MSReader::Instance()->releaseInstance();
}

void MSReaderMainWindow::openFile()
{
    const QString fileName(
        QFileDialog::getOpenFileName(this, tr("Open File..."), QDir::currentPath(),
                                     tr("Projects (*.d *.wiff *.scan *.byspec2 *.raw *.lcd *.msfaux)")));
    if (!fileName.isEmpty()) {
        openProject(fileName);
    }
}

void MSReaderMainWindow::openProjectFolder()
{
    const QString folderName(QFileDialog::getExistingDirectory(
        this, tr("Open Folder..."), QDir::currentPath(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks));

    if (!folderName.isEmpty()) {
        openProject(folderName);
    }
}

void MSReaderMainWindow::closeTab()
{
    setWindowTitle(qApp->applicationName());

    if (m_tabWidget->count() > 0) {
        MSReaderWidget *readerWidget = qobject_cast<MSReaderWidget *>(m_tabWidget->currentWidget());
        readerWidget->closeFile();
        readerWidget->deleteLater();
        m_tabWidget->removeTab(m_tabWidget->currentIndex());
    }

    if (m_tabWidget->count() == 0) {
        m_xicDataAction->setEnabled(false);
        m_chromatogramAction->setEnabled(false);
    }
}

void MSReaderMainWindow::centroidStateChanged()
{
    if (m_tabWidget->count() > 0) {
        MSReaderWidget *readerWidget = qobject_cast<MSReaderWidget *>(m_tabWidget->currentWidget());
        readerWidget->setCentroidState(m_displayCentroidDataAction->isChecked());
    }
}

void MSReaderMainWindow::currentTabChanged(int index)
{
    Q_UNUSED(index)

    if (m_tabWidget->count() > 0) {
        MSReaderWidget *readerWidget = qobject_cast<MSReaderWidget *>(m_tabWidget->currentWidget());
        m_displayCentroidDataAction->setChecked(readerWidget->centroidState());

        QString title(
            QString("%1 (%2)").arg(qApp->applicationName()).arg(readerWidget->filePath()));
        setWindowTitle(title);
    }
}

void MSReaderMainWindow::xicData()
{
    if (m_tabWidget->count() > 0) {
        MSReaderWidget *readerWidget = qobject_cast<MSReaderWidget *>(m_tabWidget->currentWidget());
        readerWidget->renderXICData();
    }
}

void MSReaderMainWindow::getChromatogram()
{
    if (m_tabWidget->count() > 0) {
        MSReaderWidget *readerWidget = qobject_cast<MSReaderWidget *>(m_tabWidget->currentWidget());
        readerWidget->getChromatogram();
    }
}

void MSReaderMainWindow::openProject(const QString &projectPath)
{
    MSReaderWidget *readerWidget = nullptr;
    QString fileName(QFileInfo(projectPath).fileName());

    if (m_tabWidget->count() == 0) {
        readerWidget = new MSReaderWidget(m_tabWidget);
        m_tabWidget->addTab(readerWidget, fileName);
    } else {
        readerWidget = qobject_cast<MSReaderWidget *>(m_tabWidget->currentWidget());

        if (!readerWidget->isEmpty()) {
            QMessageBox messageBox(this);
            messageBox.setWindowTitle("MSReaderApp");
            messageBox.setText(tr("Would you like to close current document?"));

            QPushButton *yesButton = messageBox.addButton(QMessageBox::Yes);
            QPushButton *newTabButton
                = messageBox.addButton(tr("New &Tab"), QMessageBox::ActionRole);
            messageBox.addButton(QMessageBox::No);

            messageBox.exec();

            if (messageBox.clickedButton() == yesButton) {
                readerWidget->closeFile();
                m_tabWidget->setTabText(m_tabWidget->currentIndex(), fileName);
            } else if (messageBox.clickedButton() == newTabButton) {
                readerWidget = new MSReaderWidget(m_tabWidget);
                m_tabWidget->addTab(readerWidget, fileName);
            } else {
                return;
            }
        }
    }

    m_tabWidget->setCurrentWidget(readerWidget);
    m_tabWidget->setTabToolTip(m_tabWidget->currentIndex(), projectPath);
    pmi::Err e = readerWidget->openFile(projectPath);

    if (pmi::kNoErr == e) {
        QString title(QString("%1 ( %2 )").arg(qApp->applicationName()).arg(projectPath));
        setWindowTitle(title);
        m_xicDataAction->setEnabled(true);
        m_chromatogramAction->setEnabled(true);
    } else {
        QString errorDescription(QString("%1:\nError code: %2").arg(projectPath).arg(e));
        QMessageBox::information(this, tr("Error opening project"), errorDescription);
    }
}
