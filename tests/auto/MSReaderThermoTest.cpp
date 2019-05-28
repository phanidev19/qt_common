/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Alex Prikhodko alex.prikhodko@proteinmetrics.com
 */

#include <MSReader.h>
#include <MSReaderThermo.h>
#include <PMiTestUtils.h>

using namespace pmi;
using namespace pmi::msreader;

class MSReaderThermoTest : public QObject
{
    Q_OBJECT;

public:
    //! @a path points to the .d data dir
    explicit MSReaderThermoTest(const QStringList &args)
        : QObject()
    {
        const QString arg = args[0].trimmed();
        const QFileInfo fileInfo(arg);
        // args[0] can be either a plain file name or a full file path
        const bool isPath = fileInfo.exists();
        const QString filename = isPath ? fileInfo.fileName() : arg;

        m_path = isPath
            ? arg
            : QString("%1/MSReaderThermoTest/%2").arg(PMI_TEST_FILES_OUTPUT_DIR).arg(filename);
        qDebug() << "MsReaderThermoTest args:" << args;
    }

    ~MSReaderThermoTest() {}

private Q_SLOTS:
    void initTestCase()
    {
        m_reader.reset(new MSReaderThermo());

        QCOMPARE(m_reader->openFile(path()), kNoErr);
    }

    void cleanupTestCase() { m_reader->closeFile(); }

    void testMSReaderInstance()
    {
        MSReader *ms = MSReader::Instance();
        QVERIFY(ms);
    }

    void testOpenDirect()
    {
        QFileInfo fi(path());
        QVERIFY(fi.isFile());
        QVERIFY(fi.exists());

        MSReaderThermo reader;
        QCOMPARE(reader.classTypeName(), MSReaderBase::MSReaderClassTypeThermo);
        QVERIFY(reader.canOpen(path()));
        QCOMPARE(reader.openFile(path()), kNoErr);
        QVERIFY(!reader.getFilename().isEmpty());
        QCOMPARE(reader.closeFile(), kNoErr);
    }

    void testOpenViaMSReader()
    {
        QFileInfo fi(path());
        QVERIFY(fi.isFile());
        QVERIFY(fi.exists());

        MSReader *ms = MSReader::Instance();
        QVERIFY(ms);
        QVERIFY(!ms->isOpen());
        QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
        QVERIFY(ms->getFilename().isEmpty());

        QCOMPARE(ms->openFile(path()), kNoErr);
        QVERIFY(ms->isOpen());
        QVERIFY(!ms->getFilename().isEmpty());

        QCOMPARE(ms->closeFile(), kNoErr);
        QVERIFY(!ms->isOpen());
        QCOMPARE(ms->classTypeName(), MSReaderBase::MSReaderClassTypeUnknown);
    }

private:
    QString path() const { return m_path; }

private:
    QString m_path;
    QSharedPointer<MSReaderThermo> m_reader;
};

PMI_TEST_GUILESS_MAIN_WITH_ARGS(MSReaderThermoTest, QStringList() << "path")

#include "MSReaderThermoTest.moc"
