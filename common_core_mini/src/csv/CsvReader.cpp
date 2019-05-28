/*
 *  Copyright (c) 2008 - 2009, Israel Ekpo <israel.ekpo@israelekpo.com>
 *  All rights reserved.
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  Redistributions of source code must retain the above copyright notice,
 *  this list of conditions and the following disclaimer.
 *  Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution. Neither the name
 *  of Israel Ekpo nor the names of contributors may be used to endorse or
 *  promote products derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 *  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.
 *
 *  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "CsvReader.h"

namespace {
    const short MINIMUM_WORD_SIZE_WITH_QUOTATION = 2;
    const QLatin1String TWO_QUOTES = QLatin1String("\"\"");
    const QLatin1String SINGLE_QUOTE = QLatin1String("\"");
}

_PMI_BEGIN

class Q_DECL_HIDDEN CsvReader::Private
{
public:
    explicit Private(const QString &fileName);
    explicit Private(QIODevice *device);

    ~Private();

    QStringList readRow();
    void readAllRows(QList<QStringList> *rowList);

    bool open();

    void readSingleLine(QString *buffer);
    void fieldsFromLine(QStringList *row, const QString &line);
    void extractAndAddRowValue(QStringList *row, const QString &line, int fieldEnd, int fieldStart);

    QIODevice *device;
    QTextStream readStream;
    QChar quotationCharacter;
    QChar fieldSeparatorCharacter;
    // set deleteDevice to true if the member *device is owned and should be deleted in destructor
    bool deleteDevice = true; 
};

CsvReader::Private::Private(const QString &fileName)
    : CsvReader::Private(new QFile(fileName))
{
    deleteDevice = true;
}

CsvReader::Private::Private(QIODevice *dev)
    : device(dev)
    , quotationCharacter('"')
    , fieldSeparatorCharacter(',')
    , deleteDevice(false)
{
    Q_ASSERT(dev);
}

CsvReader::Private::~Private()
{
    device->close();
    if (deleteDevice) {
        delete device;
    }
}

bool CsvReader::Private::open()
{
    if (!device->open(QIODevice::ReadOnly)) {
        
        QFileDevice *fileDevice = qobject_cast<QFileDevice*>(device);
        if (fileDevice) {
            qWarning() << "Failed to open the file:" << fileDevice->fileName();
            qWarning() << "Error code:" << fileDevice->error();
        } else {
            qWarning() << "Failed to open device";
        }
        qWarning() << "Error Details:" << device->errorString();
        return false;
    }

    readStream.setDevice(device);
    readStream.setCodec("UTF-8");

    return true;
}

void CsvReader::Private::extractAndAddRowValue(QStringList *row, const QString &line, int fieldEnd,
                                               int fieldStart)
{
    int fieldWidth = fieldEnd - fieldStart;
    QString field = line.mid(fieldStart, fieldWidth);

    // Check for enclosure character around the field
    const QChar fieldFirstChar = field[0];
    QChar fieldLastChar;

    if (fieldWidth > 1) {
        fieldLastChar = field[fieldWidth - 1];
    }

    if (0 == fieldWidth || (MINIMUM_WORD_SIZE_WITH_QUOTATION == fieldWidth && fieldFirstChar == quotationCharacter
                            && fieldLastChar == quotationCharacter)) {
        row->push_back("");
    } else {

        int offset = 1;
        int defaultOffset = 0;

        // If the enclosure char is found at start of the field
        int startAdjustment = (fieldFirstChar == quotationCharacter) ? offset : defaultOffset;

        // Offset calculation at end changes adjustment changes if first adjustment found
        if (1 == startAdjustment) {
            offset = 2;
            defaultOffset = 1;
        }

        // If the enclosure char is found at end of the field
        int endAdjustment = (fieldLastChar == quotationCharacter) ? offset : defaultOffset;

        // Take field width by ignoring enclosure char at start and end
        fieldWidth = (fieldWidth > 2) ? (fieldWidth - endAdjustment) : fieldWidth;

        // Take actual field by ignoring enclosure character at start and end
        field = field.mid(startAdjustment, fieldWidth);

        // Remove double quotes with single quotes for presentation
        field = field.replace(TWO_QUOTES, SINGLE_QUOTE);
        row->append(field);
    }
}

void CsvReader::Private::fieldsFromLine(QStringList *row, const QString &line)
{
    const int lineLength(line.size());
    if (lineLength > 0) {
        bool toggleEnclosureChar = false;
        int fieldStart = 0;
        int fieldEnd = 0;
        int charPos = 0;

        while (charPos < lineLength) {
            QChar currChar = line[charPos];
            // We take field only when even no. of enclosure character encountered
            // E.g. 1,"2,3",4 where the second comma is not a token.
            // Output {1} {2,3} {4}
            if (currChar == quotationCharacter) {
                toggleEnclosureChar = !toggleEnclosureChar;
            }

            if (currChar == fieldSeparatorCharacter && !toggleEnclosureChar) {
                fieldEnd = charPos;
                extractAndAddRowValue(row, line, fieldEnd, fieldStart);

                // Set the start position of next field
                fieldStart = charPos + 1;
            }

            // Next character position
            charPos++;
        }

        extractAndAddRowValue(row, line, charPos, fieldStart);
    }
}

QStringList CsvReader::Private::readRow()
{
    QString line;
    QStringList currentRow;

    readSingleLine(&line);

    // Prepare vector containing fields in a single row
    fieldsFromLine(&currentRow, line);

    return currentRow;
}

void CsvReader::Private::readAllRows(QList<QStringList> *rowList)
{
    while (!readStream.atEnd()) {
        rowList->append(readRow());
    }
}

void CsvReader::Private::readSingleLine(QString *buffer)
{
    QChar character;
    bool toggleEnclosureChar = false;

    while (!readStream.atEnd()) {
        readStream >> character;

        if (character == quotationCharacter) {
            toggleEnclosureChar = !toggleEnclosureChar;
        }

        // This is line termination handling
        if ((character == '\r' || character == '\n') && !toggleEnclosureChar) {
            // If further whitespace just skip to next non-space character
            readStream.skipWhiteSpace();
            break;
        }

        buffer->append(character);
    }
}

CsvReader::CsvReader(const QString &fileName)
    : d(new Private(fileName))
{
}

CsvReader::CsvReader(QIODevice *device)
    : d(new Private(device))
{
}

CsvReader::~CsvReader()
{
}

QStringList CsvReader::readRow()
{
    return d->readRow();
}

void CsvReader::readAllRows(QList<QStringList> *rowList)
{
    d->readAllRows(rowList);
}

bool CsvReader::open()
{
    return d->open();
}

bool CsvReader::hasMoreRows()
{
    return !d->readStream.atEnd();
}

QChar CsvReader::quotationCharacter() const
{
    return d->quotationCharacter;
}

QChar CsvReader::fieldSeparatorChar() const
{
    return d->fieldSeparatorCharacter;
}

QString CsvReader::lineTerminationString() const
{
    return QString();
}

void CsvReader::setQuotationCharacter(const QChar &character)
{
    if (!character.isNull()) {
        d->quotationCharacter = character;
    }
}

void CsvReader::setFieldSeparatorChar(const QChar &separator)
{
    if (!separator.isNull()) {
        d->fieldSeparatorCharacter = separator;
    }
}

void CsvReader::setLineTerminationString(const QString &terminator)
{
    Q_UNUSED(terminator)
}

_PMI_END
