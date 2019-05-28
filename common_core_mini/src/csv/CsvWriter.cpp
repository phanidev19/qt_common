/*
 *  Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "CsvWriter.h"
#include <QDebug>
#include <QFile>
#include <QTextStream>

namespace {
const QChar DOUBLE_QUOTES('\"');
}

_PMI_BEGIN

class Q_DECL_HIDDEN CsvWriter::Private
{
public:
    Private(const QString &filename, const QString &lineTermination, const QChar &fieldSeparator);
    ~Private();

    bool open();
    bool writeRow(const QStringList &row);

    static bool writeFile(const QString &filename, const QList<QStringList> &rowList,
                          const QString &lineTermination, const QChar &fieldSeparator);
    static QString prepareStringToWrite(const QStringList &row, const QString &lineTerminationChar,
                                        const QChar &fieldDelimiterChar);

    QFile fileHandle;
    QTextStream outputStream;
    QString lineTerminationChar;
    QChar fieldSeparatorChar;
};

CsvWriter::Private::Private(const QString &filename, const QString &lineTermination,
                            const QChar &fieldSeparator)
    : fileHandle(filename)
    , lineTerminationChar(lineTermination)
    , fieldSeparatorChar(fieldSeparator)
{
}

CsvWriter::Private::~Private()
{
    fileHandle.close();
}

bool CsvWriter::Private::open()
{
    if (!fileHandle.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open the file:" << fileHandle.fileName();
        qWarning() << "Error code:" << fileHandle.error()
                   << "Error Details:" << fileHandle.errorString();
        return false;
    }

    outputStream.setDevice(&fileHandle);
    outputStream.setCodec("UTF-8");
    return true;
}

bool CsvWriter::Private::writeRow(const QStringList &row)
{
    if (!row.isEmpty()) {
        outputStream << prepareStringToWrite(row, lineTerminationChar, fieldSeparatorChar)
                       << "\r\n";
    }

    return QFileDevice::NoError == fileHandle.error();
}

QString CsvWriter::Private::prepareStringToWrite(const QStringList &row,
                                                 const QString &lineTerminationChar,
                                                 const QChar &fieldDelimiterChar)
{
    QStringList stringList;
    for (const QString &data : row) {
        QString element(data);

        // Replace one double quotes with two double quotes
        element = element.replace(DOUBLE_QUOTES, "\"\"");

        // Add double quotes around the element if it has field termination character or line breaks (CRLF), double quotes
        // according to https://tools.ietf.org/html/rfc4180#page-2
        if (element.contains(fieldDelimiterChar) || element.contains(lineTerminationChar)
                || element.contains(DOUBLE_QUOTES)) {
            element.prepend('"');
            element.append('"');
        }

        stringList.append(element);
    }

    // Make single string with CSV presentation and return it
    return stringList.join(fieldDelimiterChar);
}

bool CsvWriter::Private::writeFile(const QString &filename, const QList<QStringList> &rowList,
                                   const QString &lineTermination, const QChar &fieldSeparator)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");

    // Write data into the file
    for (const QStringList &row : rowList) {
        if (!row.isEmpty()) {
            out << prepareStringToWrite(row, lineTermination, fieldSeparator) << "\r\n";
        }
    }

    file.close();
    return true;
}

//===================================================================================================

CsvWriter::CsvWriter(const QString &filename, const QString &lineTermination,
                     const QChar &fieldSeparator)
    : d(new Private(filename, lineTermination, fieldSeparator))
{
}

CsvWriter::~CsvWriter()
{
}

bool CsvWriter::open()
{
    return d->open();
}

bool CsvWriter::writeRow(const QStringList &row)
{
    return d->writeRow(row);
}

bool CsvWriter::writeFile(const QString &filename, const QList<QStringList> &rowList,
                          const QString &lineTermination, const QChar &fieldSeparator)
{
    return Private::writeFile(filename, rowList, lineTermination, fieldSeparator);
}

_PMI_END
