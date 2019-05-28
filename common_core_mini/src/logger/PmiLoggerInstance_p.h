/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil ykil@proteinmetrics.com
 * Author: Jaroslaw Staniek jstaniek@milosolutions.com
 */

#ifndef PMILOGGERINSTANCE_H
#define PMILOGGERINSTANCE_H

#include <QDialog>
#include <QFile>
#include <QPointer>
#include <QTextStream>

#include "ui_PmiLogger.h"
#include <pmi_core_defs.h>

class QAction;
class QTextBrowser;
class LogStreamRedirector;

_PMI_BEGIN

//! Implementation of the logger instance
class LoggerInstance : public QObject
{
    Q_OBJECT
public:
    LoggerInstance();

    void init(QWidget *window, const QString &settingsFile);

    ~LoggerInstance();

    void emitMessage(QtMsgType type, const QMessageLogContext *context, const QString &message);

    //! @return true if the logger has log browsers
    bool hasLogBrowsers() const;

    void connectToLogBrowser(QTextBrowser* browser);

    //! Disconnects browser @a browser from the logger so messages will no longer be displayed there
    void disconnectFromLogBrowser(QTextBrowser* browser);

    //! Connects @a action to method that shows the log dialog.
    void connectToShowLogDialog(QAction *action);

    //! Sets text color in all log browsers
    //! This method is thread-safe.
    void setLogBrowsersTextColor(const QColor &color);

    //! Appends text to all log browsers
    //! This method is thread-safe.
    void appendTextToLogBrowsers(const QString &text);

    /**
     * Returns @c true if redirection to old message handler is enabled
     *
     * @see Logger::isRedirectionToOldMessageHandlerEnabled()
     */
    bool isRedirectionToOldMessageHandlerEnabled() const;

public Q_SLOTS:
    void showLogDialog();

    void hideLogDialog();

    void openLogFolder();

    void clearLogText();

    void copyLogText();

    void writeToFile(const QString& name);

public:
    Ui_PmiLoggerDialog* logWindow = nullptr;

    QPointer<QDialog> dialog;

private:
    void initSettings(const QString &settingsFile);

    /**
     * Returns default message log pattern
     *
     * @note It is ignored if QT_MESSAGE_PATTERN environment variable is set.
     */
    QString defaultMessagePattern() const;

    QString logFileName() const;

    QString m_settingsFile;

    QFile m_file;

    QTextStream m_stream;

    int m_maxLogDialogRowCount; //!< Maximum number of rows in the debug dialog
                                //!< -1 means unlimited, 0 means disabled
    int m_maxLogFilesCount; //!< maximum number of log files, others older are removed
                            //!< -1 means unlimited, 0 means disabled
    qint64 m_maxLogFileSize; //!< maximum size of each log file, new are created when needed
                             //!< -1 means unlimited
    QString m_messagePattern;

    bool m_showStatus = true;

    QtMessageHandler m_oldMessageHandler = nullptr;

    QVector<QPointer<QTextBrowser>> m_logBrowsers;
    QScopedPointer<LogStreamRedirector> m_outRedirector;
    QScopedPointer<LogStreamRedirector> m_errRedirector;
};

_PMI_END

#endif
