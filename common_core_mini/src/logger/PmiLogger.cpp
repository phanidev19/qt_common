/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Yong Joo Kil ykil@proteinmetrics.com
 * Author: Jaroslaw Staniek jstaniek@milosolutions.com
 */

#include "PmiLogger.h"
#include "LogStreamRedirector_p.h"
#include "PmiLoggerInstance_p.h"
#include "PmiMemoryInfo.h"
#include "pmi_common_core_mini_debug.h"
#include <config-pmi-pel.h>
#include <qt_utils.h>

#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QGlobalStatic>
#include <QMutexLocker>
#include <QSettings>
#include <QTextEdit>
#include <QUrl>

#include <cstdio>
#include <fstream>
# include <Windows.h> // OutputDebugString

namespace {

const int DEFAULT_MAX_LOG_DIALOG_ROW_COUNT = 1000;
const int DEFAULT_MAX_LOG_FILE_SIZE = 500*1024;
const int DEFAULT_MAX_LOG_FILES_COUNT = 5;
const int ABSOLUTE_MIN_LOG_FILES_COUNT = 2; //!< At least there should be pmi_log.txt and one
                                            //! pmi_log_{timestamp}.txt file. Otherwise it would
                                            //! be impossible to rename current file to old one.
const int NO_LOGGING = 0;
const int UNLIMITED_LOG_FILE_SIZE = -1;
const int UNLIMITED_LOG_DIALOG_ROW_COUNT = -1;
const QLatin1String CURRENT_LOG_FILENAME("pmi_log.txt");
const QString ARCHIVED_LOG_FILENAME_PATTERN(QLatin1String("pmi_log_%1.txt"));

const QString kDebugMessagePattern(QLatin1String("DebugMessagePattern"));
const QString kMaxLogDialogRowCount(QLatin1String("MaxLogDialogRowCount"));
const QString kMaxLogFilesCount(QLatin1String("MaxLogFilesCount"));
const QString kMaxLogFileSize(QLatin1String("MaxLogFileSize"));

//! @def REMOVE_PATH_FROM_FILE_IN_LOGS
//! If defined, complex paths such as "..\..\qt_apps_libs\common_pmap\src\common\IsotopePlot.cpp" are removed from logs
//! and only filenames are kept. Paths are not useful assuming filenames are unique and categories in use.
#define REMOVE_PATH_FROM_FILE_IN_LOGS

#define PMI_LOG_PROCESS_MEMORY
#ifdef PMI_LOG_PROCESS_MEMORY
# define PMI_PROCESS_MEMORY_PLACEHOLDER "{mem}" // without "%' because Qt would eat it
#endif

#define PMI_MESSAGE_TIME_FORMAT "h:mm:ss.zzz"
#define PMI_MESSAGE_MEMORY_PREFIX "m:"

struct ColorMap {
    const QColor values[5];
};

Q_GLOBAL_STATIC_WITH_ARGS(ColorMap, g_colorForType,
    ({
        Qt::black, // QtDebugMsg
        QColor(0x0090ff), // QtWarningMsg
        QColor(0xff4040), // QtCriticalMsg & QtSystemMsg
        QColor(0xff40ff), // QtFatalMsg
        QColor(0xb0b0b0) // QtInfoMsg
     })
)

//! @see Logger::setRedirectionToOldMessageHandlerEnabled()
bool g_redirectToOldMessageHandler = false;

// ---

//! Static instance
Q_GLOBAL_STATIC(pmi::LoggerInstance, g_log)

bool g_logEnabled = true;

//! Protect g_log instance
QMutex g_logMutex;

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QMutexLocker locker(&g_logMutex);
    g_log->emitMessage(type, &context, msg);
}

QHash<QtMsgType, QChar> messageTypeSymbols = { { QtDebugMsg, QLatin1Char('D') },
                                               { QtWarningMsg, QLatin1Char('W') },
                                               { QtCriticalMsg, QLatin1Char('C') },
                                               { QtFatalMsg, QLatin1Char('F') },
                                               { QtInfoMsg, QLatin1Char('I') } };

} // namespace

_PMI_BEGIN

QString LoggerInstance::logFileName() const
{
    if (m_maxLogFileSize == NO_LOGGING) {
        return QString();
    }
    bool requiresWriting = true;

    QString initFile = getAppDataLocationFile(CURRENT_LOG_FILENAME, requiresWriting);
    QFileInfo fi(initFile);
    if (fi.exists() && m_maxLogFileSize != UNLIMITED_LOG_FILE_SIZE && fi.size() >= m_maxLogFileSize) {
        // archive the log file
        QString archivedFile = getAppDataLocationFile(ARCHIVED_LOG_FILENAME_PATTERN.arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")), requiresWriting);
        if (!QFile::rename(initFile, archivedFile)) {
            warningCoreMini() << "Could not archive log file from" << initFile << "to" << archivedFile;
            warningCoreMini() << "Logging to file will be disabled";
            return QString();
        }
    }

    // remove extra archive
    QDir dir(fi.dir());
    const QStringList listOfHistoryFiles(dir.entryList(QStringList(ARCHIVED_LOG_FILENAME_PATTERN.arg('*')),
                                         QDir::Files, QDir::Name));
    for (int i = 0; i < listOfHistoryFiles.size() - m_maxLogFilesCount + 1; i++) {
        if (!dir.remove(listOfHistoryFiles.at(i))) {
            warningCoreMini() << "Could not remove old log file" << dir.absoluteFilePath(listOfHistoryFiles.at(i));
        }
    }

    return initFile;
}

LoggerInstance::LoggerInstance()
    : m_outRedirector(new LogStreamRedirector(this, &std::cout, QtDebugMsg))
    , m_errRedirector(new LogStreamRedirector(this, &std::cerr, QtWarningMsg))
{
    if (g_log.exists()) {
        qFatal("Do not instantiate LoggerInstance");
    }
    if (!g_logEnabled) {
        qFatal("Do not instantiate LoggerInstance because it is disabled");
    }
}

void LoggerInstance::init(QWidget *window, const QString &settingsFile)
{
    initSettings(settingsFile);
    if (window) {
        dialog = new QDialog(window,
                               Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowMaximizeButtonHint
                                   | Qt::WindowCloseButtonHint | Qt::WindowTitleHint);
        logWindow = new Ui_PmiLoggerDialog;
        logWindow->setupUi(dialog);
    }

    QString logFile = logFileName();

    //Also, write to console.  Why not?
    if (!logFile.isEmpty()) {
        writeToFile(logFile);
    }
    // NOTE (jstaniek): set the custom messagehandler here, after the class is fully set up.
    // DO NOT add any code after this line.
    // https://proteinmetrics.atlassian.net/browse/ML-1895
    m_oldMessageHandler = qInstallMessageHandler(myMessageOutput);
    if (m_messagePattern.isEmpty()) {
        // Set defaults so user can alter it easily
        m_messagePattern = defaultMessagePattern();
    }
    qSetMessagePattern(m_messagePattern);

    if (logWindow) {
        connect(logWindow->closeLogWindow, &QPushButton::clicked, this, &LoggerInstance::hideLogDialog);
        connect(logWindow->clearText, &QPushButton::clicked, this, &LoggerInstance::clearLogText);
        connect(logWindow->copyText, &QPushButton::clicked, this, &LoggerInstance::copyLogText);
        connect(logWindow->openLogFolder, &QPushButton::clicked, this, &LoggerInstance::openLogFolder);
        connectToLogBrowser(logWindow->logBrowser);
    }
}

LoggerInstance::~LoggerInstance()
{
    m_file.close();
    qInstallMessageHandler(m_oldMessageHandler);
    m_oldMessageHandler = nullptr;
    delete logWindow;
}

bool LoggerInstance::isRedirectionToOldMessageHandlerEnabled() const
{
    return g_redirectToOldMessageHandler && m_oldMessageHandler;
}

QString LoggerInstance::defaultMessagePattern() const
{
    return QString::fromLatin1("%{if-debug}D%{endif}%{if-info}I%{endif}%{if-warning}W%{endif}"
                               "%{if-critical}C%{endif}%{if-fatal}F%{endif} "
                               "[%{time " PMI_MESSAGE_TIME_FORMAT "}] ")
#ifdef PMI_LOG_PROCESS_MEMORY
        // memory information can be added only when redirection is disabled
        + (isRedirectionToOldMessageHandlerEnabled()
               ? QLatin1String()
               : QLatin1String(PMI_MESSAGE_MEMORY_PREFIX PMI_PROCESS_MEMORY_PLACEHOLDER)) // tiny prefix "m:" to make it readable
#endif
        + QString::fromLatin1("%{if-category}%{category} %{endif}"
#ifdef QT_MESSAGELOGCONTEXT // only makes sense if file/line/function logging is enabled
                              "%{file}:%{line} "
# ifdef PMI_CONFIG_IS_DEBUG
                              "%{function}() "
# endif
#endif
                              "%{message}");
}

void LoggerInstance::initSettings(const QString &settingsFile)
{
    // defaults
    m_maxLogDialogRowCount = DEFAULT_MAX_LOG_DIALOG_ROW_COUNT;
    m_maxLogFilesCount = DEFAULT_MAX_LOG_FILES_COUNT;
    m_maxLogFileSize = DEFAULT_MAX_LOG_FILE_SIZE;

    // override from settings
    QSettings settings(settingsFile, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return;
    }
    m_settingsFile = settingsFile;
    m_messagePattern = settings.value(kDebugMessagePattern).toString();
    bool ok;
    int i = settings.value(kMaxLogDialogRowCount).toInt(&ok);
    if (ok) {
        m_maxLogDialogRowCount = i;
    } else {
        settings.setValue(kMaxLogDialogRowCount, m_maxLogDialogRowCount);
    }
    i = settings.value(kMaxLogFilesCount).toInt(&ok);
    if (ok) {
        m_maxLogFilesCount = qMax(i, ABSOLUTE_MIN_LOG_FILES_COUNT);
    } else {
        settings.setValue(kMaxLogFilesCount, m_maxLogFilesCount);
    }
    qlonglong longValue = settings.value(kMaxLogFileSize).toLongLong(&ok);
    if (ok) {
        m_maxLogFileSize = longValue;
    } else {
        settings.setValue(kMaxLogFileSize, m_maxLogFileSize);
    }
}

void LoggerInstance::showLogDialog()
{
    if (logWindow) {
        logWindow->logBrowser->moveCursor(QTextCursor::End);
        dialog->show();
    }
}

void LoggerInstance::hideLogDialog()
{
    if (dialog) {
        dialog->hide();
    }
}

void LoggerInstance::writeToFile(const QString& name)
{
    m_file.close();
    m_file.setFileName(name);
    m_file.open(QIODevice::Text | QIODevice::Append);
#ifdef Q_PROCESSOR_X86_64
    const QString arch = QStringLiteral("x64");
#else
    const QString arch = QStringLiteral("x86");
#endif
    m_stream.setDevice(&m_file);
    m_stream << "//////////////////////////////////////\n"
                "// Starting log\n"
                "// \n"
                "// Current time: " << QDateTime::currentDateTime().toString("yyyy-MM-dd hh.mm.ss") << endl
             << "// Application compile time: " __DATE__ " " __TIME__ << endl
             << "// Application name: " << QCoreApplication::applicationName() << " "
                                        << QCoreApplication::instance()->applicationVersion() << " " << arch << endl
             << "// Application path: " << QDir::toNativeSeparators(QApplication::applicationFilePath()) << endl
             << "//////////////////////////////////////"
             << endl;
}

void LoggerInstance::emitMessage(QtMsgType type, const QMessageLogContext *context,
                                    const QString &message)
{
    if (m_showStatus) {
        m_showStatus = false;
        QString statusMessage;
        if (m_maxLogFileSize == NO_LOGGING) {
            statusMessage
                = tr("*** MaxLogFileSize is set to %1 in \"%2\" so logging to file is disabled")
                      .arg(NO_LOGGING)
                      .arg(QDir::toNativeSeparators(m_settingsFile));
        } else if (m_file.isOpen()) {
            statusMessage
                = tr("*** Logging to file \"%1\"").arg(QDir::toNativeSeparators(m_file.fileName()));
        } else {
            statusMessage = tr("*** Logging is disabled");
        }
        // use this because can't use QDebug which is by now locked with mutex
        emitMessage(QtInfoMsg, nullptr, statusMessage);
    }

    QString formattedMessage;
    if (context) {
    /*
     * Context is present if the message comes from qDebug/qWarning/etc.
     * streams and not present if the message comes from redirection of the std::cerr/cout streams.
     *
     * If @a context is provided this function behaves like qFormatLogMessage() but also
     * removes path from file (if REMOVE_PATH_FROM_FILE_IN_LOGS is present) and fills the process
     * memory information (if PMI_LOG_PROCESS_MEMORY is present).
     * Current message pattern is used that was set by LoggerInstance::initSettings().
     */
#ifdef REMOVE_PATH_FROM_FILE_IN_LOGS
        const char* realFile;
        if (context->file) {
            realFile = strrchr(context->file, QDir::separator().cell());
            if (realFile) {
                ++realFile;
            } else {
                realFile = context->file;
            }
        } else {
            realFile = nullptr;
        }
        const QMessageLogContext newContext(realFile, context->line, context->function, context->category);
        const QMessageLogContext *realContext = &newContext;
#else
        const QMessageLogContext *realContext = context;
#endif
        if (isRedirectionToOldMessageHandlerEnabled()) {
            (*m_oldMessageHandler)(type, *realContext, message);
        } else {
            formattedMessage = qFormatLogMessage(type, *realContext, message) + QLatin1Char('\n');
#ifdef PMI_LOG_PROCESS_MEMORY
            const int pos = formattedMessage.indexOf(QLatin1String(PMI_PROCESS_MEMORY_PLACEHOLDER));
            if (pos != -1) {
                formattedMessage.replace(pos, int(qstrlen(PMI_PROCESS_MEMORY_PLACEHOLDER)),
                               pmi::MemoryInfo::getProcessMemoryString() + QLatin1Char(' '));
            }
#endif
        }
    } else { // message without context
       /*
        * If @a context is not provided this functions returns message in a simplified form.
        * The format is equivalent to defaultMessagePattern() but only contains elements that are
        * available without the context, that is:
        * - D/W/C/F character for Debug/Warning/etc. depending on @a type (actually D for cout and W for cerr)
        * - timestamp as in the %{time} placeholder of the defaultMessagePattern()
        * - memory as in the placeholder PMI_PROCESS_MEMORY_PLACEHOLDER
        * - @a message
        *
        * In the simplified form elements are separated by space characters and paths are not removed from
        * the message even if REMOVE_PATH_FROM_FILE_IN_LOGS is present in order to avoid loss of information.
        */
        if (isRedirectionToOldMessageHandlerEnabled()) {
            (*m_oldMessageHandler)(type, QMessageLogContext(), message);
        } else {
            formattedMessage = messageTypeSymbols[type] + QLatin1String(" [")
                + QDateTime::currentDateTime().toString(QLatin1String(PMI_MESSAGE_TIME_FORMAT))
                + QLatin1String("] " PMI_MESSAGE_MEMORY_PREFIX) + pmi::MemoryInfo::getProcessMemoryString()
                + QLatin1Char(' ') + message + QLatin1Char('\n');
        }
    }

    if (m_file.isOpen()) {
        m_stream << formattedMessage;
        m_stream.flush(); // costly but ensures that on potential crash full log is saved
    }

    bool outputToDebugger = false;

    if (!isRedirectionToOldMessageHandlerEnabled()) {
#if defined(PMI_CONFIG_IS_DEBUG) && defined(Q_OS_WIN) || defined(MSVC_IDE)
// All cases (Debug means Debug or RWDI builds):
//                  Debug without Debugger  Debug with Debugger  Release
//                  --------------------------------------------------------------
// MSVS             OutputDebugString       OutputDebugString    OutputDebugString
// Creator Windows  fprintf                 OutputDebugString    fprintf
// Creator !Windows fprintf                 fprintf              fprintf
//
// To see Debug output without Debugger in MSVS use the DebugView tool

# if defined(MSVC_IDE)
        outputToDebugger = true;
# else
        outputToDebugger = IsDebuggerPresent();
# endif
        if (outputToDebugger) {
            // Debug needed in the Debugger channel for:
            // - Qt Creator while debugging
            // - MSVS IDE (always)
            // Otherwise the IDEs won't show the debug.
            OutputDebugString(reinterpret_cast<const wchar_t *>(formattedMessage.utf16()));
        }
#endif // PMI_CONFIG_IS_DEBUG && Q_OS_WIN || MSVC_IDE

#if !defined(PMI_CONFIG_IS_DEBUG) || !defined(MSVC_IDE)
        // Only for non-debug builds or non-MSVS debug builds running without debugger or non-Windows builds.
        // Bonus: nice colors.
        if (!outputToDebugger) {
            // Standard err/out is used when debugger is not present
            FILE* stdstream;
            switch (type) {
            case QtDebugMsg:
            case QtInfoMsg:
                stdstream = stdout;
                break;
            case QtWarningMsg:
            case QtCriticalMsg:
            case QtFatalMsg:
                stdstream = stderr;
                break;
            }
            if (fprintf(stdstream, "%s", formattedMessage.toUtf8().constData()) >= 0) {
                fflush(stdstream); // costly but ensures that on potential crash full log is displayed
            }
        }
#endif // !PMI_CONFIG_IS_DEBUG || !MSVC_IDE
    } // !redirect

    if (g_log.exists() && g_log->hasLogBrowsers()) {
        g_log->setLogBrowsersTextColor(g_colorForType->values[type]);
        formattedMessage.chop(1); // remove \n
        g_log->appendTextToLogBrowsers(formattedMessage);
    }
#ifdef PMI_CONFIG_IS_DEBUG
    switch (type) {
    case QtWarningMsg:
    case QtCriticalMsg:
    case QtFatalMsg:
        if (false) {
            __debugbreak();
        }
        break;
    default:;
    }
#endif
}

bool LoggerInstance::hasLogBrowsers() const
{
    return !m_logBrowsers.isEmpty();
}

void LoggerInstance::setLogBrowsersTextColor(const QColor &color)
{
    auto arg(Q_ARG(QColor, color));
    for(QPointer<QTextBrowser> browser : m_logBrowsers) {
        if (browser) {
            // thread safe communication:
            QMetaObject::invokeMethod(browser, "setTextColor", Qt::AutoConnection, arg);
        }
    }
}

void LoggerInstance::appendTextToLogBrowsers(const QString &text)
{
    auto arg(Q_ARG(QString, text));
    for(QPointer<QTextBrowser> browser : m_logBrowsers) {
        if (browser) {
            // thread safe communication:
            QMetaObject::invokeMethod(browser, "append", Qt::AutoConnection, arg);
        }
    }
}

void LoggerInstance::connectToLogBrowser(QTextBrowser* browser)
{
    QMutexLocker locker(&g_logMutex);
    if (!browser || m_logBrowsers.contains(browser)) {
        return;
    }
    m_logBrowsers.append(browser);
    if (m_maxLogDialogRowCount != UNLIMITED_LOG_DIALOG_ROW_COUNT && m_maxLogDialogRowCount > 0) {
        browser->document()->setMaximumBlockCount(m_maxLogDialogRowCount);
    }
}

void LoggerInstance::disconnectFromLogBrowser(QTextBrowser* browser)
{
    QMutexLocker locker(&g_logMutex);
    m_logBrowsers.removeAll(browser);
}

void LoggerInstance::openLogFolder()
{
    QString url = getAppDataLocationFile(CURRENT_LOG_FILENAME,true);
    QFileInfo fi(url);
    QDesktopServices::openUrl(QUrl(QString("file:///") + fi.dir().path()));
}

void LoggerInstance::clearLogText()
{
    logWindow->logBrowser->clear();
}

void LoggerInstance::copyLogText()
{
    QApplication::clipboard()->setText(logWindow->logBrowser->document()->toPlainText());
}

// --

//static
void Logger::setEnabled(QWidget *window, const QString &settingsFile)
{
    if (!g_logEnabled) {
        return;
    }
    if (!g_log.exists()) {
        g_log->init(window, settingsFile);
    }
}

//static
void Logger::setRedirectionToOldMessageHandlerEnabled(bool enabled)
{
    g_redirectToOldMessageHandler = enabled;
}

//static
bool Logger::isRedirectionToOldMessageHandlerEnabled()
{
    return g_redirectToOldMessageHandler;
}

//static
void Logger::setDisabled()
{
    if (g_log.exists()) {
        warningCoreMini() << "Loging cannot be disabled because it has been already enabled with "
                             "Logger::setEnabled(). Call Logger::setDisabled() earlier.";
    } else {
        g_logEnabled = false;
    }
}

//static
void Logger::connectToLogBrowser(QTextBrowser* browser)
{
    if (!g_logEnabled || !g_log.exists()) {
        return;
    }
    g_log->connectToLogBrowser(browser);
}

//static
void Logger::disconnectFromLogBrowser(QTextBrowser* browser)
{
    if (!g_logEnabled || !g_log.exists()) {
        return;
    }
    g_log->disconnectFromLogBrowser(browser);
}

//static
void Logger::connectToShowLogDialog(QAction *action)
{
    if (!g_logEnabled || !g_log.exists() || !action) {
        return;
    }
    QObject::connect(action, &QAction::triggered, g_log, &LoggerInstance::showLogDialog,
                     Qt::UniqueConnection);
}

//static
QDialog *Logger::dialog()
{
    if (!g_logEnabled || !g_log.exists()) {
        return nullptr;
    }
    return g_log->dialog;
}

_PMI_END

#include "moc_PmiLoggerInstance_p.cpp"
