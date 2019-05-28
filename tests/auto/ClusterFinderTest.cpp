/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include <pmi_core_defs.h>

#include <PMiTestUtils.h>

#include "ClusterFinder.h"
#include "ProgressBarInterface.h"
#include "QtSqlUtils.h"
#include <QDir>
#include <QtTest>

_PMI_BEGIN

class ClusterFinderTest : public QObject
{
    Q_OBJECT
public:
    ClusterFinderTest(const QStringList &args)
        : m_testDataBasePath(args[0])
    {
    }

    private Q_SLOTS :
        void testFindGroupNumber();

private:
    QDir m_testDataBasePath;
};

class DebuggingProgressBar : public ProgressBarInterface
{
public:
    DebuggingProgressBar()
        : m_max(0)
        , m_at(0)
    {
    }

    void push(int max) override
    {
        qDebug() << "Start";
        m_max = max;
        m_at = 0;
    }

    void pop() override { m_max = 0; }

    double incrementProgress(int inc = 1) override
    {
        m_at += inc;
        if ((m_at % 100) == 0) {
            qDebug() << m_at << "/" << m_max;
        }
        return m_at;
    }

    void refreshUI() override {}

    bool userCanceled() const override { return false; }

    void setText(const QString &text) override { qDebug() << text; }

private:
    int m_max;
    int m_at;
};

void ClusterFinderTest::testFindGroupNumber()
{
    const QString fileName = "NIST_1448_features_2e5.db3";

    const QString filePath = m_testDataBasePath.filePath(fileName);
    QVERIFY(QFileInfo(filePath).exists());

    QSqlDatabase db;
    Err e = kNoErr;
    e = pmi::addDatabaseAndOpen("test", filePath, db);
    QCOMPARE(e, kNoErr);

    FinderFeaturesDao dao(&db);

    e = dao.resetGrouping();
    QCOMPARE(e, kNoErr);

    int ungrouped = -1;
    e = dao.ungroupedFeatureCount(&ungrouped);
    QCOMPARE(e, kNoErr);
    QVERIFY(ungrouped > 0);

    ClusterFinder finder(&db);

    QSharedPointer<DebuggingProgressBar> progress;
    e = finder.run(progress);
    QCOMPARE(e, kNoErr);

    e = dao.ungroupedFeatureCount(&ungrouped);
    QCOMPARE(e, kNoErr);
    QCOMPARE(ungrouped, 0);

    e = finder.exportToInsilicoPeptideCsv(
        m_testDataBasePath.filePath("grouped_insilico_peptides.csv"));
    QCOMPARE(e, kNoErr);

}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::ClusterFinderTest, QStringList() << "Remote Data Folder")

#include "ClusterFinderTest.moc"
