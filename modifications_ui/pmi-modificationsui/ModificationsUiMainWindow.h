/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef MODIFICATIONSUIMAINWINDOW_H
#define MODIFICATIONSUIMAINWINDOW_H

#include <QMainWindow>

class CinReader;
class ModificationsUiWidget;

/*!
 * \brief ModificationsUiMainWindow is a wrapper for ModificationsUiWidget to use it as
 * standalone application
 */
class ModificationsUiMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ModificationsUiMainWindow(bool proteomeDiscoverer_2_0 = false,
                                       bool proteomeDiscoverer_1_4 = false,
                                       bool readDataFromStdin = false, QWidget *parent = 0);
    ~ModificationsUiMainWindow();

    QStringList modStrings() const;

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void buildUi();
    void showInstructionsForCopyingToPD_1_4(const QString& savedFilename);

private:
    bool m_proteomeDiscoverer_2_0 = false;
    bool m_proteomeDiscoverer_1_4 = false;

    ModificationsUiWidget *m_modificationsUi = nullptr;

    QScopedPointer<CinReader, QScopedPointerDeleteLater> m_cinReader;
    int m_stopStdinReadingTimerId = -1;
    QByteArray m_stdinBuffer;
};

#endif // MODIFICATIONSUIMAINWINDOW_H
