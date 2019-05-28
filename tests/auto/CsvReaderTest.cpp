/*
 *  Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential
 *
 *  Author  : Yashpalsinh Gohil yash.gohiil@gmail.com
 */

#include "CsvReader.h"

#include <QBuffer>
#include <QtTest/QtTest>

#include <PmiQtCommonConstants.h>

_PMI_BEGIN

//! @brief This class helps to unit test CsvReader class
class CsvReaderTest : public QObject
{
    Q_OBJECT
public:
    CsvReaderTest();
private Q_SLOTS:
    //! Test files with windows line ending format
    void testWindowsFormatCSV();

    //! Test files with MAC line ending format
    void testMacFormatCSV();

    //! Test files with Linux line ending format
    void testLinuxFormatCSV();

    //! These files are having tab separated values mostly .txt files
    void testTabSeparatorValue();

    //! These files are having mix line termination
    void testMixedLineTermintation();

    //! Verifies that one can read in-memory QByteArray data
    void testParseInMemoryData();

private:
    //! Generic implementation which finally test the file's contents
    void testCSVFile(const QString &filePath, const QList<QStringList> &rowToValidate,
                     const QChar &fieldSeparator);

    //! Path for the directory which includes data, This will be all .csv and .txt
    QString m_dataPath;
};

CsvReaderTest::CsvReaderTest()
    : m_dataPath(QFile::decodeName(PMI_TEST_FILES_DATA_DIR))
{
}

void CsvReaderTest::testCSVFile(const QString &filePath, const QList<QStringList> &rowToValidate,
                                const QChar &fieldSeparator)
{
    CsvReader reader(filePath);
    reader.setQuotationCharacter('"');
    QVERIFY2(reader.quotationCharacter() == '"', "Check quotation character");

    reader.setFieldSeparatorChar(fieldSeparator);
    QVERIFY2(reader.fieldSeparatorChar() == fieldSeparator, "Check field Separator");

    QVERIFY2(reader.open(), "File should be opened succesfully");

    int rowIndex = 0;
    while (reader.hasMoreRows()) {
        QStringList row(reader.readRow());
        int size(row.size());
        QStringList currentRow(rowToValidate.at(rowIndex));

        QVERIFY2(currentRow.size() == size, "Both should have same no. of elements");

        for (int index(0); index < size; ++index) {
            QCOMPARE(row[index].compare(currentRow[index]), 0);
        }
        ++rowIndex;
    }

    QVERIFY2(rowIndex == rowToValidate.size(), "Testin file and local list both should contain same number of rows");
}

void CsvReaderTest::testWindowsFormatCSV()
{
    QStringList row;
    QList<QStringList> rowList;
    row.push_back("sdfఖగఘఙచఛజఝ\",\"123");
    row.push_back("testing");

    const QString m_dataPath = QFile::decodeName(PMI_TEST_FILES_DATA_DIR);
    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test1.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test6.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test7.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test8.windows.csv", rowList, ',');

    // To unit test mixed case data
    testCSVFile(m_dataPath + "/CsvData/Test1.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test6.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test7.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test8.windows.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("sdf");
    row.push_back("洛烙珞落123");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test2.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test5.windows.csv", rowList, ',');

    // To unit test mixed case data
    testCSVFile(m_dataPath + "/CsvData/Test2.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test5.windows.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("sdf\",\"123");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test3.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test4.windows.csv", rowList, ',');

    // To unit test mixed case data
    testCSVFile(m_dataPath + "/CsvData/Test3.windows.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test4.windows.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("\"");
    row.push_back("\"\"");
    row.push_back("\"hello\"");
    row.push_back("\",\"hi\"");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test1.windows.Space.Quotes.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("  space    ");
    row.push_back("    \"   \" hi ,\"   ");
    row.push_back("");
    row.push_back("hi");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test2.windows.Space.Quotes.csv", rowList, ',');
}

void CsvReaderTest::testMixedLineTermintation()
{
    QStringList row;
    QList<QStringList> rowList;

    row.push_back("\"");
    row.push_back("\"\"");
    row.push_back("\"hello\"");
    row.push_back("\",\"hi\"");

    rowList.append(row);
    row.clear();

    row.push_back("sdf");
    row.push_back("洛烙珞落123");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test1.windows.Space.QuotesMultiline.MixedTerminationCase.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("  space    ");
    row.push_back("    \"   \" hi ,\"   ");
    row.push_back("");
    row.push_back("hi");

    rowList.append(row);
    row.clear();

    row.push_back("");
    row.push_back("sdf\",\"123");
    row.push_back("testingઔકખગઘઙચછજઝઞટ");
    row.push_back("");
    row.push_back("");

    rowList.append(row);

    row.erase(row.begin() + 4);
    row.push_back("testingɯɰɱɲɳɴɵɶɷɸ");
    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test2.windows.Space.Quotes.Multiline.MixedTerminationCase.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test3.windows.Space.Quotes.Multiline.MixedTerminationCase.csv", rowList, ',');
}

void CsvReaderTest::testParseInMemoryData()
{
    QByteArray csv = QByteArrayLiteral("Index,Value\n10,3.14");
    QBuffer buffer(&csv);

    CsvReader reader(&buffer);
    QVERIFY(reader.open());
    
    QVERIFY(reader.hasMoreRows());
    QStringList firstRow = reader.readRow();
    QCOMPARE(firstRow, QStringList({ "Index", kValue }));
        
    QStringList secondRow = reader.readRow();
    QCOMPARE(secondRow, QStringList({ "10", "3.14" }));
}

void CsvReaderTest::testMacFormatCSV()
{
    QStringList row;
    QList<QStringList> rowList;
    row.push_back("sdf\",\"123");
    row.push_back("洛烙珞落testing");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test1.mac.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test4.mac.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test7.mac.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("sdf\",\"123");
    row.push_back("testing");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test2.mac.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test5.mac.csv", rowList, ',');

    row.clear();
    rowList.clear();
    row.push_back("");
    row.push_back("sdf\",\"123");
    row.push_back("testing");
    row.push_back("");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test3.mac.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test6.mac.csv", rowList, ',');
}

void CsvReaderTest::testLinuxFormatCSV()
{
    QStringList row;
    QList<QStringList> rowList;
    row.push_back("");
    row.push_back("sdf\",\"123");
    row.push_back("testingઔકખગઘઙચછજઝઞટ");
    row.push_back("");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test1.linux.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test2.linux.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test3.linux.csv", rowList, ',');

    row.erase(row.begin() + 4);
    row.push_back("testingɯɰɱɲɳɴɵɶɷɸ");
    rowList.clear();

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test4.linux.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test5.linux.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test6.linux.csv", rowList, ',');
    testCSVFile(m_dataPath + "/CsvData/Test7.linux.csv", rowList, ',');
}

void CsvReaderTest::testTabSeparatorValue()
{
    QStringList row;
    QList<QStringList> rowList;
    row.push_back("123\"\"");
    row.push_back("123");
    row.push_back(" ɯɰɱɲɳɴɵɶɷɸ\"123123\"");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test1.tab.windows.txt", rowList, '\t');
    testCSVFile(m_dataPath + "/CsvData/Test3.tab.windows.txt", rowList, '\t');
    testCSVFile(m_dataPath + "/CsvData/Test1.tab.linux.txt", rowList, '\t');

    row.clear();
    rowList.clear();
    row.push_back("123\",\"ТУФХЦЧШЩ'''''123");
    row.push_back(" \"123'''123\"");
    row.push_back("");

    rowList.append(row);
    testCSVFile(m_dataPath + "/CsvData/Test2.tab.windows.txt", rowList, '\t');
    testCSVFile(m_dataPath + "/CsvData/Test4.tab.windows.txt", rowList, '\t');
    testCSVFile(m_dataPath + "/CsvData/Test2.tab.linux.txt", rowList, '\t');
}

_PMI_END

QTEST_APPLESS_MAIN(pmi::CsvReaderTest)

#include "CsvReaderTest.moc"
