/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef MSREADERMAINWINDOW_H
#define MSREADERMAINWINDOW_H

#include <QMainWindow>

class MSReaderMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MSReaderMainWindow(QWidget *parent = 0);
    ~MSReaderMainWindow();

public slots:
    void openFile();
    void openProjectFolder();
    void closeTab();
    void centroidStateChanged();
    void currentTabChanged(int index);
    void xicData();
    void getChromatogram();

private:
    void openProject(const QString &projectPath);

    QTabWidget *m_tabWidget;
    QMenuBar *m_menuBar;
    QAction *m_displayCentroidDataAction;
    QAction *m_xicDataAction;
    QAction *m_chromatogramAction;
};

#endif // MSREADERMAINWINDOW_H
