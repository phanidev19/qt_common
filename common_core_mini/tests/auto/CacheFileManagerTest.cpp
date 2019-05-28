/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include <CacheFileManager.h>
#include <PMiTestUtils.h>

#include <pmi_core_defs.h>

#include <QDir>
#include <QFile>

_PMI_BEGIN

static const QLatin1String TEST_ROOT_FOLDER_NAME("CacheFileManagerTest");

static const QLatin1String HASH_V1_NAME("1");

static const QLatin1String TEST_SOURCE_NAME("Source.txt");
static const QLatin1String TEST_SOURCE_HASH_V_1("38ca8a306619f2573acfe0ebdace045a3f14841b");

static const QLatin1String SEARCH_PATH_1("search_path_1");
static const QLatin1String SEARCH_PATH_2("search_path_2");
static const QLatin1String SEARCH_PATH_3("search_path_3");

static const QLatin1String EXISTING_SUFFIX_1(".suffix1.txt");
static const QLatin1String EXISTING_SUFFIX_2(".suffix2.txt");
static const QLatin1String EXISTING_SUFFIX_3(".suffix3.txt");
static const QLatin1String NOT_EXISTING_SUFFIX(".suffix4.txt");

static const QLatin1String EXISTING_SUFFIX_OLD(".suffix1-old.txt");
static const QLatin1String NOT_EXISTING_SUFFIX_OLD(".suffix2-old.txt");

class CacheFileManagerTest : public QObject
{
    Q_OBJECT

public:
    CacheFileManagerTest();

private Q_SLOTS:
    void testHashV1ExistingFileSearch1();
    void testHashV1ExistingFileSearch2();
    void testHashV1ExistingFileSearch3();
    void testNewPathCreation();
    void testOldExistingFileSearch();
    void testOldPathCreation();

private:
    QSharedPointer<CacheFileManager> createCacheFileManager() const;
    QSharedPointer<CacheFileManager> createDefaultCacheFileManager() const;

private:
    const QDir m_dataPathDir;
    const QString m_sourcePath;
    const QStringList m_searchPaths;
};

CacheFileManagerTest::CacheFileManagerTest()
    : m_dataPathDir(
          QDir(QFile::decodeName(PMI_TEST_FILES_DATA_DIR)).absoluteFilePath(TEST_ROOT_FOLDER_NAME))
    , m_sourcePath(m_dataPathDir.absoluteFilePath(TEST_SOURCE_NAME))
    , m_searchPaths({ m_dataPathDir.absoluteFilePath(SEARCH_PATH_1),
                      m_dataPathDir.absoluteFilePath(SEARCH_PATH_2),
                      m_dataPathDir.absoluteFilePath(SEARCH_PATH_3) })
{
}

static QString createExpectedPath(const QString &rootPath, const QString &versionName,
                                  const QString &hash, const QString &suffix)
{
    return QDir(rootPath).absoluteFilePath(versionName + QLatin1String("-") + hash + suffix);
}

void CacheFileManagerTest::testHashV1ExistingFileSearch1()
{
    const QString expectedPath = createExpectedPath(m_searchPaths[0], HASH_V1_NAME,
                                                    TEST_SOURCE_HASH_V_1, EXISTING_SUFFIX_1);
    const QSharedPointer<CacheFileManager> cacheFileManager = createCacheFileManager();
    QString actualPath;

    QCOMPARE(cacheFileManager->findOrCreateCachePath(EXISTING_SUFFIX_1, &actualPath), kNoErr);
    QCOMPARE(actualPath, expectedPath);
}

void CacheFileManagerTest::testHashV1ExistingFileSearch2()
{
    const QString expectedPath = createExpectedPath(m_searchPaths[1], HASH_V1_NAME,
                                                    TEST_SOURCE_HASH_V_1, EXISTING_SUFFIX_2);
    const QSharedPointer<CacheFileManager> cacheFileManager = createCacheFileManager();
    QString actualPath;

    QCOMPARE(cacheFileManager->findOrCreateCachePath(EXISTING_SUFFIX_2, &actualPath), kNoErr);
    QCOMPARE(actualPath, expectedPath);
}

void CacheFileManagerTest::testHashV1ExistingFileSearch3()
{
    const QString expectedPath = createExpectedPath(m_searchPaths[2], HASH_V1_NAME,
                                                    TEST_SOURCE_HASH_V_1, EXISTING_SUFFIX_3);
    const QSharedPointer<CacheFileManager> cacheFileManager = createCacheFileManager();
    QString actualPath;

    QCOMPARE(cacheFileManager->findOrCreateCachePath(EXISTING_SUFFIX_3, &actualPath), kNoErr);
    QCOMPARE(actualPath, expectedPath);
}

void CacheFileManagerTest::testNewPathCreation()
{
    const QString expectedPath = createExpectedPath(m_searchPaths[0], HASH_V1_NAME,
                                                    TEST_SOURCE_HASH_V_1, NOT_EXISTING_SUFFIX);
    const QSharedPointer<CacheFileManager> cacheFileManager = createCacheFileManager();
    QString actualPath;

    QCOMPARE(cacheFileManager->findOrCreateCachePath(NOT_EXISTING_SUFFIX, &actualPath), kNoErr);
    QCOMPARE(actualPath, expectedPath);
}

void CacheFileManagerTest::testOldExistingFileSearch()
{
    const QString expectedPath
        = m_dataPathDir.absoluteFilePath(TEST_SOURCE_NAME + EXISTING_SUFFIX_OLD);
    const QSharedPointer<CacheFileManager> cacheFileManager = createDefaultCacheFileManager();
    QString actualPath;

    QCOMPARE(cacheFileManager->findOrCreateCachePath(EXISTING_SUFFIX_OLD, &actualPath), kNoErr);
    QCOMPARE(actualPath, expectedPath);
}

void CacheFileManagerTest::testOldPathCreation()
{
    const QString expectedPath
        = m_dataPathDir.absoluteFilePath(TEST_SOURCE_NAME + NOT_EXISTING_SUFFIX_OLD);
    const QSharedPointer<CacheFileManager> cacheFileManager = createDefaultCacheFileManager();
    QString actualPath;

    QCOMPARE(cacheFileManager->findOrCreateCachePath(NOT_EXISTING_SUFFIX_OLD, &actualPath), kNoErr);
    QCOMPARE(actualPath, expectedPath);
}

QSharedPointer<CacheFileManager> CacheFileManagerTest::createCacheFileManager() const
{
    QSharedPointer<CacheFileManager> cacheFileManager(new CacheFileManager());

    cacheFileManager->setSearchPaths(m_searchPaths);
    cacheFileManager->setSavePath(m_searchPaths.front());
    cacheFileManager->setSourcePath(m_sourcePath);

    return cacheFileManager;
}

QSharedPointer<CacheFileManager> CacheFileManagerTest::createDefaultCacheFileManager() const
{
    QSharedPointer<CacheFileManager> cacheFileManager(new CacheFileManager());

    cacheFileManager->setSourcePath(m_sourcePath);

    return cacheFileManager;
}

_PMI_END

QTEST_APPLESS_MAIN(pmi::CacheFileManagerTest)

#include "CacheFileManagerTest.moc"
