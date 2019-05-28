/*
 *  Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#ifndef CSV_WRITER_H
#define CSV_WRITER_H

#include <pmi_common_core_mini_export.h>
#include <pmi_core_defs.h>
#include <QScopedPointer>
#include <QStringList>

_PMI_BEGIN

//! @brief This class facilitates the CSV writing for given list of elements
class PMI_COMMON_CORE_MINI_EXPORT CsvWriter
{
public:
    explicit CsvWriter(const QString &filename, const QString &lineTermination = "\r\n",
                       const QChar &fieldSeparator = ',');
    ~CsvWriter();

    //! \return the status, if file is opened or not
    bool open();

    //! \return true, if single row is written in a file
    bool writeRow(const QStringList &row);

    //! \return true if whole file written successfully
    static bool writeFile(const QString &filename, const QList<QStringList> &rowList,
                          const QString &lineTermination, const QChar &fieldSeparator);

private:
    Q_DISABLE_COPY(CsvWriter)

    class Private;
    const QScopedPointer<Private> d;
};

_PMI_END

#endif CSV_WRITER_H // CSV_WRITER_H
