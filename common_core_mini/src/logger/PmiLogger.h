/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil ykil@proteinmetrics.com
 * Author: Jaroslaw Staniek jstaniek@milosolutions.com
 */

#ifndef PMILOGGER_H
#define PMILOGGER_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>

#include <QString>

class QAction;
class QDialog;
class QTextBrowser;
class QWidget;

_PMI_BEGIN

/**
 * Extended logging features to PMI applications and libraries
 *
 * The logger integrates with Qt debug and logging categories features and offers the following
 * extras:
 * - predefines rich pattern for log messages
 * - and adds memory information to log lines
 * - saves the log to text files of the current application
 * - rotates log files
 * - offers configuration of logging behavior via application's ini file
 * - offers built-in application's Log dialog, that can be made accessible e.g. by Help -> Logs action
 * - sends output to standard error or output channels, in a way dependent on current envirnment
 * - on Windows sends output to debugging channel if there is debugging session
 *
 * Logging provided by this class is usually initialized on startup of applications, not in
 * individual libraries. The logging applies to global scope, per process. Applications do not have
 * to be graphical ones but this module links to QtGui. Libraries used by applications do not need
 * access to the Logger API or use it, their debug or warning messages are handled by the Logger
 * transparently.
 *
 * Processes that do not initialize logging using this class fall back to logging features offered
 * by Qt.
 *
 * @note All public methods of the Logger are thread-safe.
 * @note To disable this logger after it has been initialized, do not use
 *       qInstallMessageHandler(nullptr) but instead use Logger::setDisabled().
 *
 * @since 2018.03.29
 */
class PMI_COMMON_CORE_MINI_EXPORT Logger
{
public:
    /**
     * Enables and initializes the logger if it has not yet been initialized.
     *
     * @a window parameter provides parent for the built-in log dialog. If it is @c nullptr the log
     * dialog will not be created. @a settingsFile is a file with path containing application-level
     * logging settings. If it is not a valid INI file, default logging settings are used.
     *
     * Subsequent calls to setEnabled() do nothing.
     *
     * @note CoreMainWindow class from QAL initializes the logger automatically so applications
     * that use CoreMainWindow do not have to do that on their own.
     *
     * @note If under certain conditions disabling the logger is needed, use setDisabled() instead
     * of setEnabled(). Logger that has been disabled using setDisabled() can't be enabled
     * again afterwards.
     */
    static void setEnabled(QWidget *window = nullptr, const QString &settingsFile = QString());

    /**
     * Disables the logger.
     *
     * @note Logger that has been already initialized with setEnabled() can't be disabled anymore.
     *       Once the logger is disabled, it cannot be enabled.
     *
     * Subsequent calls to setDisabled() do nothing.
     */
    static void setDisabled();

    /**
     * Connects browser @a browser to the logger.
     *
     * If the General/MaxLogDialogRowCount config option (limitation of rows in the built-in log
     * dialog, 1000 by default) is set for the application to value greater than 0 then
     * QTextDocument::setMaximumBlockCount() will be called for the browser.
     *
     * Once the browser is connected to the logger, new messages of the logger will be appended
     * to the browser. Ownership of the browser is not transferred, it can be safely removed at any
     * time. This method does nothing if connectToLogBrowser() has already been called for the same
     * browser.
     *
     * @note Only works after setEnabled() is properly called.
     */
    static void connectToLogBrowser(QTextBrowser* browser);

    /**
     * Connects @a action to method that shows the built-in log dialog.
     *
     * Before showing the dialog, for user's convenience cursor moves to the last line of the log.
     * The dialog is displayed using the QDialog::show() so it is not modal.
     *
     * @note Only works after setEnabled() is properly called.
     */
    static void connectToShowLogDialog(QAction *action);

    /**
     * Disconnects browser @a browser from the logger so messages will no longer be displayed there.
     *
     * @note Only works after setEnabled() is properly called.
     */
    static void disconnectFromLogBrowser(QTextBrowser* browser);

    /**
     * Returns the built-in log dialog object.
     *
     * @c nullptr is returned if:
     * - the Logger has not been initialized with setEnabled(QWidget *window) where window is not @c nullptr
     * - or if setDisabled() was called
     */
    static QDialog *dialog();

    /**
     * Returns @c true is redirection to old (original) message handler is enabled
     *
     * @see setRedirectionToOldMessageHandlerEnabled()
     */
    static bool isRedirectionToOldMessageHandlerEnabled();

    /**
     * Controls redirection to old (original) message handler
     *
     * Redirection is required by QtTest applications because QtTest customizes message handler
     * not only for message format but also to implement QTest::ignoreMessage().
     * Without redirection such features no longer work so tests that depend on them can fail.
     *
     * PmiTestApplication from QAL automatically calls setRedirectionToOldMessageHandlerEnabled(true)
     * so e.g. PMI_TEST_MAIN_WITH_ARGS should be used with PmiTestApplication.
     *
     * By default redirection is disabled.
     */
    static void setRedirectionToOldMessageHandlerEnabled(bool set);
};

_PMI_END

#endif
