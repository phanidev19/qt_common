/*
 *  Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "CsvReader.h"
#include "CsvWriter.h"
#include <QtTest/QtTest>

_PMI_BEGIN

//! @brief This class helps to unit test CsvWritter class
class CsvWriterTest : public QObject
{
public:
    CsvWriterTest();
    ~CsvWriterTest();
    Q_OBJECT
private Q_SLOTS:
    //! Test Different combination of CSV
    void testCaseCSV();

private:
    //! Test written CSV with CsvReader and cross check
    void testCSVFile(const QString& filePath, const QList<QStringList>& rowList, const QChar& fieldSeparator);

    //! Test writing of single row and list of rows
    void testCsvWriter(const QList<QStringList>& rowList);

    //! Test writing of list of rows
    void testCsvWriterFullList(const QList<QStringList>& rowList);

    //! Test writing of single row
    void testCsvWriterSingleRow(const QList<QStringList>& rowList);

    QString m_dataPath;
    QString m_outputFullListCsvPath;
    QString m_outputSingleRowCsvPath;
};

CsvWriterTest::CsvWriterTest()
    : m_dataPath(QFile::decodeName(PMI_TEST_FILES_DATA_DIR))
    , m_outputFullListCsvPath(m_dataPath + "/CsvData/TestOutputFullList.csv")
    , m_outputSingleRowCsvPath(m_dataPath + "/CsvData/TestOutputSingleRow.csv")
{
}

CsvWriterTest::~CsvWriterTest()
{
    QVERIFY2(QFile(m_outputFullListCsvPath).remove(), "File Should be removed after test case completes");
    QVERIFY2(QFile(m_outputSingleRowCsvPath).remove(), "File Should be removed after test case completes");
}

void CsvWriterTest::testCSVFile(const QString &filePath, const QList<QStringList> &rowList,
                                const QChar &fieldSeparator)
{
    CsvReader reader(filePath);
    reader.setQuotationCharacter('"');
    reader.setFieldSeparatorChar(fieldSeparator);

    int listIndex(0);
    QVERIFY2(reader.open(), "File should open successfully");

    while (reader.hasMoreRows()) {
        QStringList row(reader.readRow());
        QStringList rowToValidate = rowList[listIndex++];

        int size(row.size());
        QCOMPARE(rowToValidate.size(), size);

        for (int index(0); index < size; ++index) {
            QCOMPARE(row[index].compare(rowToValidate[index]), 0);
        }
    }
}

void CsvWriterTest::testCsvWriter(const QList<QStringList>& rowList)
{
    testCsvWriterFullList(rowList);
    testCsvWriterSingleRow(rowList);
}

void CsvWriterTest::testCsvWriterFullList(const QList<QStringList>& rowList)
{
    QVERIFY2(CsvWriter::writeFile(m_outputFullListCsvPath, rowList, "\r\n", ','),
             "Whole file should be written succesfully");
    testCSVFile(m_outputFullListCsvPath, rowList, ',');
}

void CsvWriterTest::testCsvWriterSingleRow(const QList<QStringList>& rowList)
{
    {
        CsvWriter writer(m_outputSingleRowCsvPath);
        QVERIFY2(writer.open(), "File should open");

        for (const QStringList& row : rowList) {
            QVERIFY2(writer.writeRow(row), "Row should be written in file");
        }
    }

    testCSVFile(m_outputSingleRowCsvPath, rowList, ',');
}

void CsvWriterTest::testCaseCSV()
{
    QList<QStringList> rowList;
    QStringList row;
    row.push_back("ТУФХЦЧШЩsdf\",\"123");
    row.push_back("testing");
    rowList.append(row);

    row.clear();
    row.push_back("sdf");
    row.push_back("123");
    row.push_back("");
    rowList.append(row);

    testCsvWriter(rowList);

    row.clear();
    row.push_back("ઔકખગઘઙચછજઝઞટs\"\"");
    row.push_back("123,\"");
    row.push_back("");
    rowList.append(row);

    testCsvWriter(rowList);

    row.clear();
    row.push_back(" 洛烙珞落  s\"\"");
    row.push_back("123,\"   ");
    row.push_back("    ");
    rowList.append(row);
    testCsvWriter(rowList);
}

_PMI_END

QTEST_APPLESS_MAIN(pmi::CsvWriterTest)

#include "CsvWriterTest.moc"
