/*
* Copyright (C) 2016 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include <QtTest>
#include "pmi_core_defs.h"
#include "RowNode.h"

_PMI_BEGIN

class RowNodeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testCreate();
    void testGetColumnIndexFromName();
    void testGetNameFromColumnIndex();
    void testAddNewChildMatchRootDataSize();
    void testIndexAsChildFromParent();
    void testGetRootIndex();

private:

    static RowNode createRootChildNode();
    // creates simple table with Name, Surname, Age and Weight and adds 3 persons there
    static RowNode createSimpleTable();
};

RowNode RowNodeTest::createSimpleTable()
{
    // test creation of row node and verify that it has content as expected
    // serves as example of how to create RowNode structure filled with data
    QStringList columns = { "Name", "Surname", "Age", "Weight" };

    VariantItems joe = VariantItems() << "Joe" << "Red" << 25 << 100.5;
    VariantItems noel = VariantItems() << "Noel" << "Blue" << 37 << 86.2;
    VariantItems jack = VariantItems() << "Jack" << "White" << 34 << 92.77;

    RowNode root;
    root.addData(columns);
    root.addChild(new RowNode(joe, &root));
    root.addChild(new RowNode(noel, &root));
    root.addChild(new RowNode(jack, &root));

    return root;
}

void RowNodeTest::testCreate()
{
    RowNode root = createSimpleTable();

    QStringList actualColumns = root.getRowDataToStringList();
    
    QStringList expectedColumns = { "Name", "Surname", "Age", "Weight" };
    VariantItems joe = VariantItems() << "Joe" << "Red" << 25 << 100.5;
    VariantItems noel = VariantItems() << "Noel" << "Blue" << 37 << 86.2;
    VariantItems jack = VariantItems() << "Jack" << "White" << 34 << 92.77;

    QCOMPARE(actualColumns, expectedColumns);
    
    QCOMPARE(root.getChildListSize(), 3);
    QCOMPARE(root.getChild(0)->getRowData().toVectorString(), joe.toVectorString());
    QCOMPARE(root.getChild(1)->getRowData().toVectorString(), noel.toVectorString());
    QCOMPARE(root.getChild(2)->getRowData().toVectorString(), jack.toVectorString());
}

void RowNodeTest::testGetColumnIndexFromName()
{
    RowNode root = createRootChildNode();

    QVector<RowNode *> nodes;
    nodes.push_back(&root);
    nodes.push_back(root.getChild(0));

    for (RowNode * node : nodes) {
        QCOMPARE(node->getColumnIndexFromName("Name"), 0);
        QCOMPARE(node->getColumnIndexFromName("Surname"), 1);
        QCOMPARE(node->getColumnIndexFromName("Age"), 2);
        QCOMPARE(node->getColumnIndexFromName("Weight"), 3);
    }
    
}

void RowNodeTest::testGetNameFromColumnIndex()
{
    RowNode root = createRootChildNode();

    QVector<RowNode *> nodes;
    nodes.push_back(&root);
    nodes.push_back(root.getChild(0));

    for (RowNode * node : nodes) {
        QCOMPARE(node->getNameFromColumnIndex(0), QString("Name"));
        QCOMPARE(node->getNameFromColumnIndex(1), QString("Surname"));
        QCOMPARE(node->getNameFromColumnIndex(2), QString("Age"));
        QCOMPARE(node->getNameFromColumnIndex(3), QString("Weight"));
    }

}

RowNode RowNodeTest::createRootChildNode()
{
    QStringList columns = { "Name", "Surname", "Age", "Weight" };
    VariantItems joe = VariantItems() << "Joe" << "Red" << 25 << 100.5;
    RowNode root;
    root.addData(columns);
    root.addChild(new RowNode(joe, &root));

    return root;
}

void RowNodeTest::testAddNewChildMatchRootDataSize()
{
    RowNode root;
    root.addData(QStringList() << "1" << "2" << "3");
    RowNode *child = root.addNewChildMatchRootDataSize();

    QCOMPARE(root.getChildListSize(), 1);
    QCOMPARE(root.getRowData().size(), 3);
    QCOMPARE(root.getRowDataToStringList().at(0), QLatin1String("1"));
    QCOMPARE(root.getRowDataToStringList().at(1), QLatin1String("2"));
    QCOMPARE(root.getRowDataToStringList().at(2), QLatin1String("3"));
    QCOMPARE(child->getChildListSize(), 0);
    QCOMPARE(child->getRowData().size(), 3);
    QCOMPARE(child->getRowData().size(), 3);
    QVERIFY(child->getRowData().at(0).isNull());
    QVERIFY(child->getRowData().at(1).isNull());
    QVERIFY(child->getRowData().at(2).isNull());
}

void RowNodeTest::testIndexAsChildFromParent()
{
    const RowNode root = createSimpleTable();
    QCOMPARE(root.indexAsChildFromParent(), -1);
    
    bool testVerbose = false;
    for (int i = 0; i < root.getChildListSize(); ++i) {
        RowNode *child = root.getChild(i);

        if (testVerbose) {
            qDebug() << child->getDataAt("Name").toString() << ": index"
                     << child->indexAsChildFromParent();
        }

        QCOMPARE(child->indexAsChildFromParent(), i);
    }
}

void RowNodeTest::testGetRootIndex()
{
    const RowNode root = createSimpleTable();
    QCOMPARE(root.getTreeLevel(), 0);
    QCOMPARE(root.getChildFirst()->getTreeLevel(), 1);

    root.getChildFirst()->addChild(new RowNode());
    QCOMPARE(root.getChildFirst()->getChildFirst()->getTreeLevel(), 2);
}

_PMI_END

QTEST_MAIN(pmi::RowNodeTest)

#include "RowNodeTest.moc"

