/*
 *  Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "CsvReader.h"
#include "CsvWriter.h"
#include <QFile>

//! @brief This is main entry point for the application CsvParserApp
int main()
{
    int result = 0;
    const QString m_dataPath(QFile::decodeName(PMI_TEST_FILES_DATA_DIR));
    const QString m_outputPath(QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR));
    QChar fieldSeparator = ',';

    // This part shows how CSV reader and writer works for single row read and write
    pmi::CsvReader reader(m_dataPath + "/Data/UCS LE.csv");

    reader.setQuotationCharacter('"');
    reader.setFieldSeparatorChar(fieldSeparator);

    if (reader.open()) {
        pmi::CsvWriter writer(m_outputPath + "/out.csv", "\n", fieldSeparator);
        if (writer.open()) {

            // Write csv file using row by row
            // This can be useful when the file has large data and
            // developer can use this function to write single row
            while (reader.hasMoreRows()) {
                const QStringList row(reader.readRow());
                writer.writeRow(row);
            }
        }
    } else {
        result = 1;
    }

    // This part shows how whole CSV read and writer works
    pmi::CsvReader readerOne(m_dataPath + "/Data/ImportExample.Byologic.csv");

    if (readerOne.open()) {
        QList<QStringList> rowList;

        while (readerOne.hasMoreRows()) {
            rowList.append(readerOne.readRow());
        }

        // Write csv file using static function by whole data in the list
        // This can be helpful when we have smaller data
        pmi::CsvWriter::writeFile(m_outputPath + "/out2.csv", rowList, "\r\n", ',');
    } else {
        result = 1;
    }

    return result;
}
