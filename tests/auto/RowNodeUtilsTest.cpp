/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "EntryDefMap.h"
#include "RowNode.h"
#include "RowNodeUtils.h"

#include <pmi_core_defs.h>

#include <QPair>
#include <QtTest>

#include <random>

Q_DECLARE_METATYPE(RowNode*)
Q_DECLARE_METATYPE(ColumnFriendlyName::GroupingMethod)
Q_DECLARE_METATYPE(ColumnFriendlyName::Type)

_PMI_BEGIN

class RowNodeUtilsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testGetRange_data();
    void testGetRange();

    void testMakeConcatList_data();
    void testMakeConcatList();
    void testGroupSummaryUpdate_data();
    void testGroupSummaryUpdate();
    void testGroupSummaryListUpdate();
    void benchmarkMakeConcatListLargeSet();
    void benchmarkMakeConcatListManySmallSets();
    void testReadWriteToJSON_RowNodeList();

};


void RowNodeUtilsTest::testGetRange_data()
{
    QTest::addColumn<VariantItems>("input");
    QTest::addColumn< QPair<double, double> >("minmax");

    QTest::newRow("zero-zero") << VariantItems({ 0.0, 0.0 }) << QPair<double, double>(0.0, 0.0);
    QTest::newRow("common-case") << VariantItems({0.0, 1000.0, 500.0}) << QPair<double, double>(0.0, 1000.0);
    QTest::newRow("empty-values-case") << VariantItems({"", 1200.0, 350.0}) << QPair<double, double>(350.0, 1200.0);
}

void RowNodeUtilsTest::testGetRange()
{
    ColumnFriendlyName::FriendlyNameStruct score = ColumnFriendlyName::FriendlyNameStruct("Score", "MS2 Search Score", ColumnFriendlyName::Type_Decimal, 2, ColumnFriendlyName::Range);

    double min;
    double max;
    QFETCH(VariantItems, input);
    
    bool ok = getRange(input, &min, &max, &QVariant::toDouble);
    QVERIFY(ok);


    typedef QPair<double, double> MinMax;
    QFETCH(MinMax, minmax);

    QCOMPARE(min, minmax.first);
    QCOMPARE(max, minmax.second);
}

void RowNodeUtilsTest::testMakeConcatList_data()
{
    QTest::addColumn<VariantItems>("input");
    QTest::addColumn<QString>("expectedOutput");
    QTest::addColumn<ColumnFriendlyName::Type>("type");
    QTest::addColumn<ColumnFriendlyName::GroupingMethod>("groupingMethod");
    QTest::addColumn<QVariant>("previousValue");

    const VariantItems set_with_duplicate = VariantItems({ 1, 2, 2, 3, 5});
    const QString list_with_duplicate_keep_duplicates = QStringLiteral("1; 2; 2; 3; 5");
    const QString list_with_duplicate_no_duplicates = QStringLiteral("1; 2; 3; 5");

    const ColumnFriendlyName::Type TYPE_INT = ColumnFriendlyName::Type_Integer;
    const ColumnFriendlyName::GroupingMethod METHOD_KEEP_DUPLICATES = ColumnFriendlyName::ListKeepDuplicates;
    const ColumnFriendlyName::GroupingMethod METHOD_NO_DUPLICATES = ColumnFriendlyName::List;

    QTest::newRow("list with duplicates, config keep duplicates and no previous value")
            << set_with_duplicate << list_with_duplicate_keep_duplicates
            << TYPE_INT << METHOD_KEEP_DUPLICATES << QVariant();


    QTest::newRow("list with duplicates, config no duplicates and no previous value")
            << set_with_duplicate << list_with_duplicate_no_duplicates
            << TYPE_INT << METHOD_NO_DUPLICATES << QVariant();

    QTest::newRow("list with duplicates, config keep duplicates and the same previous value")
            << set_with_duplicate << list_with_duplicate_keep_duplicates
            << TYPE_INT << METHOD_KEEP_DUPLICATES << QVariant(list_with_duplicate_keep_duplicates);


    QTest::newRow("list with duplicates, config keep duplicates and different previous value")
            << set_with_duplicate << list_with_duplicate_keep_duplicates
            << TYPE_INT << METHOD_KEEP_DUPLICATES << QVariant(QStringLiteral("1; 2; 6; 3; 5"));

}

void RowNodeUtilsTest::testMakeConcatList()
{
    QFETCH(VariantItems, input);
    QFETCH(QString, expectedOutput);
    QFETCH(ColumnFriendlyName::Type, type);
    QFETCH(ColumnFriendlyName::GroupingMethod, groupingMethod);
    QFETCH(QVariant, previousValue);

    ColumnFriendlyName::FriendlyNameStruct config = ColumnFriendlyName::FriendlyNameStruct(
                QString(), QString(), type, 0, groupingMethod);

    rn::GroupSummary gs;
    const QString result = gs.makeValue(input, previousValue, config).toString();
    QCOMPARE(result, expectedOutput);
}

void RowNodeUtilsTest::testGroupSummaryUpdate_data()
{
    QTest::addColumn<RowNode*>("root");
    QTest::addColumn<RowNode*>("groupParent");
    QTest::addColumn<ColumnFriendlyName::Type>("columnType");
    QTest::addColumn<ColumnFriendlyName::GroupingMethod>("groupingType");
    QTest::addColumn<QString>("expectedResult");

    struct Nodes {
        RowNode* root = nullptr;
        RowNode* groupParent = nullptr;
    };

    auto createRowNode = [] (const VariantItems &data) {
        Nodes nodes;
        nodes.root = new RowNode();
        nodes.root->addData(VariantItems() << "col");
        nodes.groupParent = nodes.root->addNewChildMatchRootDataSize();
        for (const QVariant &item: data) {
            RowNode *leaf = nodes.groupParent->addNewChildMatchRootDataSize();
            leaf->getDataAt(0) = item;
        }
        return nodes;
    };

    {
        Nodes nodes = createRowNode(VariantItems() << 2 << 2 << 3 << 4);

        QTest::newRow("list of ints with duplicate - config no duplicates")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::List
            << "2; 3; 4";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 2 << 2 << 3 << 4);

        QTest::newRow("list of ints with duplicate - config keep duplicates")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::ListKeepDuplicates
            << "2; 2; 3; 4";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << -1 << 0 << 3 << 4);

        QTest::newRow("range of ints with negative")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::Range
            << "-1 - 4";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 5 << QVariant() << 3 << 4);

        QTest::newRow("range of ints with empty")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::Range
            << "3 - 5";
    }

    {
        Nodes nodes = createRowNode(VariantItems() <<  -1 << 0 << 3 << 4);

        QTest::newRow("sum of ints with negative")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::Sum
            << "6";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << QVariant() << 7 << 3 << 4);

        QTest::newRow("sum of ints with empty")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::Sum
            << "14";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 1 << 2 << 3 << 4);

        QTest::newRow("ints DashIfDifferent - different")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DashIfDifferent
            << "--";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 1 << 1 << 1 << 1);

        QTest::newRow("ints DashIfDifferent - the same")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DashIfDifferent
            << "1";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 1 << QVariant() << 1 << 1);

        QTest::newRow("ints DashIfDifferent - the same with null")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DashIfDifferent
            << "--";
    }

    {
        Nodes nodes = createRowNode(VariantItems() <<  1 << QVariant() << 1 << 1);

        QTest::newRow("ints DashIfDifferentIgnoreEmptyOrNULL - the same with null")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DashIfDifferentIgnoreEmptyOrNULL
            << "1";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 1 << 1 << 1 << 1);

        QTest::newRow("ints DashIfDifferentIgnoreEmptyOrNULL - the same without null")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DashIfDifferentIgnoreEmptyOrNULL
            << "1";
    }

    {
        Nodes nodes = createRowNode(VariantItems() <<  1 << 2 << 1 << 1);

        QTest::newRow("ints DashIfDifferentIgnoreEmptyOrNULL - different without null")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DashIfDifferentIgnoreEmptyOrNULL
            << "--";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 1 << 2 << 3 << 5);

        QTest::newRow("ints DoNothing")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::DoNothing
            << "";
    }

    {
        Nodes nodes = createRowNode(VariantItems() <<  1 << 1 << 1 << 1);

        QTest::newRow("ints Counter_ShowOriginalFirst - no original")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::Counter_ShowOriginalFirst
            << " (4)";
    }

    {
        Nodes nodes = createRowNode(VariantItems() << 1 << 1 << 2 << 3);

        nodes.groupParent->getDataAt(0) = 3;

        QTest::newRow("ints Counter_ShowOriginalFirst - with original")
            << nodes.root << nodes.groupParent
            << ColumnFriendlyName::Type_Integer << ColumnFriendlyName::Counter_ShowOriginalFirst
            << "3 (4)";
    }
}

void RowNodeUtilsTest::testGroupSummaryUpdate()
{
    QFETCH(RowNode*, root);
    QFETCH(RowNode*, groupParent);
    QFETCH(ColumnFriendlyName::Type, columnType);
    QFETCH(ColumnFriendlyName::GroupingMethod, groupingType);
    QFETCH(QString, expectedResult);

    rn::GroupSummary gs;
    gs.set("col", { QString(), QString(), columnType, 0, groupingType });

    gs.update(root);

    QCOMPARE(groupParent->getDataAt(0).toString(), expectedResult);
}

void RowNodeUtilsTest::testGroupSummaryListUpdate()
{
    const QString kId = QStringLiteral("Id");
    const QString kDuplicates = QStringLiteral("duplicates");
    const QString kUnique = QStringLiteral("unique");

    RowNode *root = new RowNode();
    root->addData(VariantItems() << kId << kDuplicates << kUnique);

    rn::GroupSummary gs;
    gs.set(kId, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::DoNothing});
    gs.set(kDuplicates, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::ListKeepDuplicates});
    gs.set(kUnique, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::List});

    RowNode *groupParent = root->addNewChildMatchRootDataSize();
    RowNode *leaf00 = groupParent->addNewChildMatchRootDataSize();
    leaf00->getDataAt(kId) = 0;
    leaf00->getDataAt(kDuplicates) = 0;
    leaf00->getDataAt(kUnique) = 0;

    RowNode *leaf01 = groupParent->addNewChildMatchRootDataSize();
    leaf01->getDataAt(kId) = 0;
    leaf01->getDataAt(kDuplicates) = 1;
    leaf01->getDataAt(kUnique) = 1;

    RowNode *leaf02 = groupParent->addNewChildMatchRootDataSize();
    leaf02->getDataAt(kId) = 0;
    leaf02->getDataAt(kDuplicates) = 2;
    leaf02->getDataAt(kUnique) = 2;

    gs.update(root);

    // for now we have no duplicates
    QCOMPARE(groupParent->getDataAt(kDuplicates).toString(), QStringLiteral("0; 1; 2"));
    QCOMPARE(groupParent->getDataAt(kUnique).toString(), QStringLiteral("0; 1; 2"));

    // now we alter columns so that there is duplicate
    leaf02->getDataAt(kDuplicates) = 1;
    leaf02->getDataAt(kUnique) = 1;

    gs.update(root);

    QCOMPARE(groupParent->getDataAt(kDuplicates).toString(), QStringLiteral("0; 1; 1"));
    QCOMPARE(groupParent->getDataAt(kUnique).toString(), QStringLiteral("0; 1"));


    //real life example
    {
        const QString kProteinId = QStringLiteral("ProteinId");
        const QString kProteinCount = QStringLiteral("ProteinCount");
        const QString kGaussWidth = QStringLiteral("GaussWidth");

        rn::GroupSummary gs;
        gs.set(kId, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::DoNothing});
        gs.set(kProteinId, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::ListKeepDuplicates});
        gs.set(kProteinCount, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::ListKeepDuplicates});
        gs.set(kGaussWidth, {QString(), QString(), ColumnFriendlyName::Type_Integer, 0, ColumnFriendlyName::List});


        RowNode *root = new RowNode();
        root->addData(VariantItems() << kId << kProteinId << kProteinCount << kGaussWidth);

        RowNode *child0 = root->addNewChildMatchRootDataSize();
        child0->getDataAt(kId) = 1;
        child0->getDataAt(kGaussWidth) = 5;

        RowNode *leaf00 = child0->addNewChildMatchRootDataSize();
        leaf00->getDataAt(kId) = 1;
        leaf00->getDataAt(kProteinId) = 1;
        leaf00->getDataAt(kProteinCount) = 1;
        leaf00->getDataAt(kGaussWidth) = 5;


        RowNode *child1 = root->addNewChildMatchRootDataSize();
        child1->getDataAt(kId) = 2;
        child1->getDataAt(kGaussWidth) = 10;

        RowNode *leaf10 = child1->addNewChildMatchRootDataSize();
        leaf10->getDataAt(kId) = 2;
        leaf10->getDataAt(kProteinId) = 1;
        leaf10->getDataAt(kProteinCount) = 2;
        leaf10->getDataAt(kGaussWidth) = 10;


        RowNode *leaf11 = child1->addNewChildMatchRootDataSize();
        leaf11->getDataAt(kId) = 2;
        leaf11->getDataAt(kProteinId) = 1;
        leaf11->getDataAt(kProteinCount) = 1;
        leaf11->getDataAt(kGaussWidth) = 10;

        gs.update(root);

        QCOMPARE(child1->getDataAt(kProteinId).toString(), QStringLiteral("1; 1"));
        QCOMPARE(child1->getDataAt(kProteinCount).toString(), QStringLiteral("2; 1"));
    }
}

void RowNodeUtilsTest::benchmarkMakeConcatListLargeSet()
{
    const int precision = 3;
    const int sizeOfSample = 10000;
    std::default_random_engine re;
    std::uniform_real_distribution<double> dist(std::numeric_limits<double>::min(),
                                                std::numeric_limits<double>::max());

    VariantItems input;
    for (int i = 0; i < sizeOfSample; ++i) {
        input << QVariant(QString::number(dist(re), 'f', precision));
    }

    ColumnFriendlyName::FriendlyNameStruct config = ColumnFriendlyName::FriendlyNameStruct(
        "Score", "MS2 Search Score", ColumnFriendlyName::Type_Decimal, 2,
        ColumnFriendlyName::List);

    rn::GroupSummary gs;

    QBENCHMARK {
        gs.makeValue(input, QVariant(), config);
    }
}

void RowNodeUtilsTest::benchmarkMakeConcatListManySmallSets()
{
    const int precision = 3;
    const int sizeOfOneSample = 20;
    const int numberOfSamples = 100;
    std::default_random_engine re;
    std::uniform_real_distribution<double> dist(std::numeric_limits<double>::min(),
                                                std::numeric_limits<double>::max());

    QVector<VariantItems> input;
    for (int i = 0; i < numberOfSamples; ++i) {
        VariantItems items;
        for (int j = 0; j < sizeOfOneSample; ++j) {
            items << QVariant(QString::number(dist(re), 'f', precision));
        }
        input << items;
    }

    ColumnFriendlyName::FriendlyNameStruct config = ColumnFriendlyName::FriendlyNameStruct(
        "Score", "MS2 Search Score", ColumnFriendlyName::Type_Decimal, 2,
        ColumnFriendlyName::List);

    rn::GroupSummary gs;

    QBENCHMARK {
        for (const VariantItems &items: qAsConst(input)) {
            gs.makeValue(items, QVariant(), config);
        }
    }
}

QStringList typeNames(const RowNode *node)
{
    QStringList result;
    for (int i = 0; i < node->getRowData().size(); ++i) {
        QString typeName = QString::fromUtf8(node->getDataAt(i).typeName());
        result.push_back(typeName);
    }
    return result;
}

void RowNodeUtilsTest::testReadWriteToJSON_RowNodeList()
{
    // test so that reading and writing is symmetrical and not lossy wrt to types, see LT-3856

    // prepare environment
    const QString dirPath
        = QFile::decodeName(PMI_TEST_FILES_OUTPUT_DIR) + QString("/RowNodeUtilsTest");
    QDir dir(dirPath);
    if (!QFileInfo::exists(dirPath)) {
        QVERIFY(dir.mkpath(dirPath));
    }
    const QString filePath = dir.filePath("hereAndThere.json");

    // create some RowNode
    RowNode expected;
    expected.setRowData(VariantItems{ "QString", "int", "double", "bool" });
    RowNode *child = expected.addNewChildMatchRootDataSize();
    child->setRowData(VariantItems{ "Qt is cute!", 0, 3.14, false });

    QList<const RowNode *> outputList = { &expected };

    // write to json
    Err e = kNoErr;
    e = pmi::writeToJSON_RowNodeList(outputList, filePath);
    QCOMPARE(e, kNoErr);

    // read from json
    QList<RowNode> inputList;
    pmi::readFromJSON_RowNodeList(filePath, inputList);

    QCOMPARE(inputList.size(), 1);
    RowNode actual = inputList.at(0);

    // check if the rownode is loaded correctly and type is preserved
    QCOMPARE(actual.getRowData().toVectorString(), expected.getRowData().toVectorString());

    QCOMPARE(actual.getChildListSize(), expected.getChildListSize());
    QVERIFY(actual.getChildListSize() == 1);

    // expected types
    QStringList expectedTypes = typeNames(expected.getChildFirst());

    // actual types
    QStringList actualTypes = typeNames(actual.getChildFirst());
    QEXPECT_FAIL("", "It returns wrong types for int and bool", Continue);
    if (actualTypes != expectedTypes) {
        qDebug() << "Actual" << actualTypes;
        qDebug() << "Expected" << expectedTypes;
    }

    QCOMPARE(expectedTypes, actualTypes);

    QVERIFY(QFile::remove(filePath));
}

_PMI_END

Q_DECLARE_METATYPE(VariantItems)

QTEST_MAIN(pmi::RowNodeUtilsTest)

#include "RowNodeUtilsTest.moc"

