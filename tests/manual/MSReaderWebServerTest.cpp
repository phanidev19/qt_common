/*
 * Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */
/*
 * This file is based on a qhttpengine/examples/fileserver example:
 *
 * Copyright (c) 2015 Nathan Osman
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "MSReaderWebServerTest.h"

#include <MSReader.h>
#include <config-pmi_qt_common.h>

#include <ComInitializer.h>

#include <QHttpEngine/QHttpServer>

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QUrl>

#include <Windows.h>

static const char* DEFAULT_HTTP_ADDRESS = "127.0.0.1";
static const char* DEFAULT_HTTP_PORT = "8000";
static QString DEFAULT_DIRECTORY_TO_SERVE() { return QDir::homePath(); }

// Template for listing directory contents
static const QString WEB_PAGE_TEMPLATE = QLatin1String(
        "<!DOCTYPE html>"
        "<html>"
          "<head>"
            "<meta charset=\"utf-8\">"
            "<title>%1</title>"
            "<style>"
            "body { font-family: \"Helvetica Neue\", Helvetica, Arial, sans-serif; }\n"
            "table {"
                "border-collapse: collapse;"
                "width: 100%;"
            "}\n"
            "th, td {"
                "text-align: left;"
                "padding: 8px;"
            "}\n"
            "td.number, th.number {"
                "text-align: right;"
            "}\n"
            "tr:nth-child(odd){background-color: #f2f2f2}"
            "</style>"
          "</head>"
          "<body>"
            "<h1><b>%1</b></h1>\n"
            "%2\n"
            "<p><em>RAW Files Web Interface version &quot;%3&quot;</em><br/>"
            "<small>Served in %4 sec.</small></p>"
          "</body>"
        "</html>");

class MSReaderWebHandler::Private
{
public:
    Private() {}
    QDir directoryToServe;
    QStringList nameFilters;
    QElapsedTimer processingTime;
};

MSReaderWebHandler::MSReaderWebHandler(const QString &dir, QObject *parent)
    : QHttpHandler(parent), d(new Private)
{
    d->directoryToServe = QDir(dir);
    d->nameFilters = QStringList() << QLatin1String("*.raw");
}

MSReaderWebHandler::~MSReaderWebHandler()
{
    delete d;
}

void MSReaderWebHandler::process(QHttpSocket *socket, const QString &path)
{
    d->processingTime.start();
    if (!d->directoryToServe.exists()) {
        socket->writeError(QHttpSocket::InternalServerError);
        return;
    }
    const QString decodedPath = QUrl::fromPercentEncoding(path.toUtf8());
    qDebug() << decodedPath << socket->method() << socket->headers();

    if (socket->method() != "GET") {
        return;
    }
    QFileInfo info(d->directoryToServe.absolutePath() + '/' + decodedPath);
    qDebug() << d->directoryToServe.absolutePath() << info.path();
    if (info.isReadable()) {
        if (info.isDir()) {
            processDirectory(socket, decodedPath, info.absoluteFilePath());
        } else if (info.isFile()) {
            processFile(socket, decodedPath, info.absoluteFilePath());
        }
    } else {
        socket->writeError(QHttpSocket::NotFound, tr("%1 not found").arg(decodedPath.toHtmlEscaped()).toUtf8());
        return;
    }
}

void MSReaderWebHandler::processDirectory(QHttpSocket *socket, const QString &relativePath,
                                          const QString &absolutePath)
{
    // Add entries for each of the supported files
    QDir dir(absolutePath);
    QString contents;
    const QString relativePathHtml(relativePath.toHtmlEscaped());
    int index = 0;
    for(const QFileInfo &info : dir.entryInfoList(d->nameFilters,
                                                                  QDir::Files | QDir::NoDotAndDotDot,
                                                                  QDir::Name))
    {
        const QString fileHtml(info.fileName().toHtmlEscaped());
        contents += QString::fromLatin1(
                        "<tr><td><a href=\"%1\">%2</a></td><td class=\"number\">%3</td></tr>\n")
                               .arg(relativePathHtml + '/' + fileHtml)
                               .arg(info.fileName().toHtmlEscaped())
                               .arg(tr("%1 MiB").arg(QString::number(double(info.size()) / 1024 / 1024, 'g', 3)));
        ++index;
    }
    if (index > 0) {
        contents.prepend(QLatin1String("<table><tr><th>Name</th><th class=\"number\">Size</th></tr>\n"));
        contents.append(QLatin1String("</table>"));
    } else {
        contents = QLatin1String("<p>No files found.</p>");
    }
    sendContents(socket, '/' + relativePathHtml, contents);
}

void MSReaderWebHandler::sendContents(QHttpSocket *socket, const QString &title, const QString &contents)
{
    // Build the response and convert the string to UTF-8
    QByteArray data = WEB_PAGE_TEMPLATE
            .arg(title)
            .arg(contents)
            .arg(PMI_QT_COMMON_VERSION_STRING " " PMI_QT_COMMON_GIT_VERSION)
            .arg(QString::number(double(d->processingTime.elapsed()) / 1000, 'g', 2))
            .toUtf8();

    // Set the headers and write the content
    socket->setHeader("Content-Type", "text/html");
    socket->setHeader("Content-Length", QByteArray::number(data.length()));
    socket->write(data);
    socket->close();
}

void MSReaderWebHandler::processFile(QHttpSocket *socket, const QString &relativePath, const QString &absolutePath)
{
    pmi::MSReader *reader = pmi::MSReader::Instance();
    const QString relativePathHtml(relativePath.toHtmlEscaped());
    if (pmi::kNoErr != reader->openFile(absolutePath)) {
        socket->writeError(QHttpSocket::InternalServerError, tr("Failed to open file &quot;%1&quot;")
                           .arg(relativePathHtml).toUtf8());
        return;
    }
    pmi::point2dList points;
    QString contents;
    int index = 0;
    if (pmi::kNoErr == reader->getScanData(1, &points, false)) {
        for(const point2d &p : points) {
            contents += QString::fromLatin1("<tr><td class=\"number\">%1</td>"
                                            "<td class=\"number\">%2</td>"
                                            "<td class=\"number\">%3</td></tr>\n")
                                            .arg(index + 1)
                                            .arg(p.x(), 0, 'g', 6).arg(p.y(), 0, 'g', 6);
            ++index;
        }
        if (index > 0) {
            contents.prepend(QString::fromLatin1("<p>Found %1 points:</p><table><tr><th class=\"number\">#</th>"
                                           "<th class=\"number\">X</th><th class=\"number\">Y</th></tr>\n")
                                           .arg(points.size()));
            contents.append(QLatin1String("</table>"));
        }
    }
    if (index == 0) {
        contents = QLatin1String("<p>No points found.</p>");
    }

    if (pmi::kNoErr != reader->closeFile()) {
        socket->writeError(QHttpSocket::InternalServerError, tr("Failed to close file &quot;%1&quot;")
                           .arg(relativePathHtml).toUtf8());
        return;
    }
    sendContents(socket, '/' + relativePathHtml, contents);
}

// ----

int main(int argc, char * argv[])
{
    QCoreApplication a(argc, argv);
    pmi::ComInitializer comInitializer;

    // Build the command-line options
    QCommandLineParser parser;
    QCommandLineOption addressOption(
        QStringList() << "a" << "address",
        "address to bind to",
        "address",
        DEFAULT_HTTP_ADDRESS
    );
    parser.addOption(addressOption);
    QCommandLineOption portOption(
        QStringList() << "p" << "port",
        "port to listen on",
        "port",
        DEFAULT_HTTP_PORT
    );
    parser.addOption(portOption);
    QCommandLineOption dirOption(
        QStringList() << "d" << "directory",
        "directory with .raw files to serve",
        "directory",
        DEFAULT_DIRECTORY_TO_SERVE()
    );
    parser.addOption(dirOption);
    parser.addHelpOption();

    // Parse the options that were provided
    parser.process(a);

    // Obtain the values
    QHostAddress address = QHostAddress(parser.value(addressOption));
    quint16 port = parser.value(portOption).toInt();
    QString dir = parser.value(dirOption);

    // Create the MSReader handler and server
    MSReaderWebHandler handler(dir);
    QHttpServer server(&handler);

    int result;
    // Attempt to listen on the specified port
    if(server.listen(address, port)) {
        result = a.exec();
    } else {
        qWarning("Unable to listen on the specified port.");
        result = 1;
    }
    return result;
}
