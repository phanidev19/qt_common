/*
 *  Copyright (C) 2019 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef GLYCANSUIMAINWINDOW_H
#define GLYCANSUIMAINWINDOW_H

#include <QMainWindow>

#include <memory>

class CinReader;
class GlycansUiWidget;

/*!
 * \brief GlycansUiMainWindow is a wrapper for GlycansUiWidget to use it as standalone
 * application
 */
class GlycansUiMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit GlycansUiMainWindow(bool proteomeDiscoverer_2_0 = false,
                                 bool proteomeDiscoverer_1_4 = false,
                                 bool readDataFromStdin = false,
                                 const QStringList &glycanDbPaths = QStringList(),
                                 QWidget *parent = 0);
    ~GlycansUiMainWindow();

    QStringList glycansStrings() const;

protected:
    virtual void timerEvent(QTimerEvent *event) override;

private:
    void buildUi();
    void showInstructionsForCopyingToPD_1_4(const QString& savedFilename);

private:
    bool m_proteomeDiscoverer_2_0 = false;
    bool m_proteomeDiscoverer_1_4 = false;

    QStringList m_glycanDbPaths;

    GlycansUiWidget *m_glycansUi = nullptr;

    QScopedPointer<CinReader, QScopedPointerDeleteLater> m_cinReader;
    int m_stopStdinReadingTimerId = -1;
    QByteArray m_stdinBuffer;
};

#endif // GLYCANSUIMAINWINDOW_H
