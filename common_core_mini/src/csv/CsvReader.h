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

#ifndef CSV_PARSER_H
#define CSV_PARSER_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>
#include <QScopedPointer>
#include <QStringList>

class QFile;
class QTextStream;
class QIODevice;

_PMI_BEGIN

//! @brief This class is facilitates CSV reading and returns list of elements in a row
class PMI_COMMON_CORE_MINI_EXPORT CsvReader
{
public:
    //! \brief Creates a new CSV reader that reads from file
    explicit CsvReader(const QString &fileName);

    //! \brief Creates a new CSV reader that reads from device. 
    //!
    //! It does not take the ownership of the \a device
    explicit CsvReader(QIODevice *device);
    
    //! \brief Destructs the reader
    ~CsvReader();

    //! Reads a single row and returns list of elements
    QStringList readRow();

    //! Reads all the rows and prepares list of list of elements /a rowList
    void readAllRows(QList<QStringList> *rowList);

    //! \return the status, if file is opened or not
    bool open();

    //! \return true if any rows exist to read
    bool hasMoreRows();

    //! \return character, set as quotation
    QChar quotationCharacter() const;

    //! \return character, set as field separator
    QChar fieldSeparatorChar() const;

    //! \returns string, set as line termination string typically, /r /n and /r/n
    Q_DECL_DEPRECATED_X("lineTerminationString is deprecated. Please remove from code.")
    QString lineTerminationString() const;

    //! Set as quotation /a character
    void setQuotationCharacter(const QChar &character);

    //! Set as field separator character /a separator
    void setFieldSeparatorChar(const QChar &separator);

    //! Set Line termination string /a terminator
    Q_DECL_DEPRECATED_X("setLineTerminationString is deprecated. Please remove from code.")
    void setLineTerminationString(const QString &terminator);

private:
    class Private;
    Q_DISABLE_COPY(CsvReader)

    const QScopedPointer<Private> d;
};

_PMI_END

#endif /* CSV_PARSER_H */
