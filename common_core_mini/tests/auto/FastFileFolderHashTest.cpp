/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include <FastFileFolderHash.h>
#include <HashUtils.h>
#include <PMiTestUtils.h>

_PMI_BEGIN

class FastFileFolderHashTest : public QObject
{
    Q_OBJECT

public:
    explicit FastFileFolderHashTest(const QStringList &args);

private Q_SLOTS:
    void testHashResult();

private:
    const QString m_path;
    const QByteArray m_algorithmName;
    const int m_sampleCount;
    const qint64 m_sampleSize;
    const int m_minSizeForFastHashMultiplier;
    const int m_maxFileCountInFolderToUse;
    const QByteArray m_expectedHashValue;
};

FastFileFolderHashTest::FastFileFolderHashTest(const QStringList &args)
    : m_path(args[0])
    , m_algorithmName(args[1].toLatin1())
    , m_sampleCount(args[2].toInt())
    , m_sampleSize(static_cast<qint64>(args[3].toInt()) * 1024)
    , m_minSizeForFastHashMultiplier(args[4].toInt())
    , m_maxFileCountInFolderToUse(args[5].toInt())
    , m_expectedHashValue(args[6].toLatin1())
{
}

void FastFileFolderHashTest::testHashResult()
{
    QVERIFY(isSupportedHashAlgorithmName(m_algorithmName));

    const QCryptographicHash::Algorithm algorithm = hashAlgorithmByNameUnsafe(m_algorithmName);

    if (QFileInfo(m_path).isDir()) {
        FastFileFolderHash fastHash(algorithm, m_sampleCount, m_sampleSize,
                                    m_minSizeForFastHashMultiplier, m_maxFileCountInFolderToUse);

        fastHash.setExtensionPartsToSkip(QStringList() << "-stamp");
        QCOMPARE(fastHash.addPath(m_path), kNoErr);

        const QByteArray actualFastHashValue = fastHash.result().toHex();

        QCOMPARE(actualFastHashValue, m_expectedHashValue);

        return;
    }

    if (QFileInfo(m_path).isFile()) {
        QByteArray actualFastHashValue;

        QCOMPARE(FastFileFolderHash::hash(m_path, algorithm, m_sampleCount, m_sampleSize,
                                          m_minSizeForFastHashMultiplier,
                                          m_maxFileCountInFolderToUse, &actualFastHashValue),
                 kNoErr);
        actualFastHashValue = actualFastHashValue.toHex();
        QCOMPARE(actualFastHashValue, m_expectedHashValue);

        QFile actualFile(m_path);

        QVERIFY(actualFile.open(QIODevice::ReadOnly));

        if (actualFile.size() < m_sampleSize * m_minSizeForFastHashMultiplier * m_sampleCount) {
            QCryptographicHash hash(algorithm);

            QVERIFY(hash.addData(&actualFile));

            const QByteArray actualHashValue = hash.result().toHex();

            QCOMPARE(actualFastHashValue, actualHashValue);
        }

        return;
    }

    QFAIL("unsupported input data");
}

_PMI_END

PMI_TEST_GUILESS_MAIN_WITH_ARGS(pmi::FastFileFolderHashTest,
                                QStringList()
                                    << QLatin1String("path") << QLatin1String("algorithm")
                                    << QLatin1String("sample_count") << QLatin1String("sample_size")
                                    << QLatin1String("min_size_for_fast_hash_multiplier")
                                    << QLatin1String("max_file_count_in_folder_to_use")
                                    << QLatin1String("expected_hash_value"))

#include "FastFileFolderHashTest.moc"
