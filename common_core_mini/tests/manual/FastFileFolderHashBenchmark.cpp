/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include <FastFileFolderHash.h>
#include <HashUtils.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QFileInfo>

#include <chrono>

using namespace pmi;

static const char *DEFAULT_ALGORITHM_NAME = "Sha1";
static const int DEFAULT_SAMPLE_COUNT = 7;
static const qint64 DEFAULT_SAMPLE_SIZE = 16;
static const int DEFAULT_MIN_SIZE_FOR_FAST_HASH_MULTIPLIER = 7;
static const int DEFAULT_MAX_FILE_COUNT_IN_FOLDER_TO_USE = 7;

namespace
{

struct Args {
    QString path;
    bool useQCryptographicHashDirectly;
    QCryptographicHash::Algorithm algorithm;
    int sampleCount;
    qint64 sampleSize;
    int minSizeForFastHashMultiplier;
    int maxFileCountInFolderToUse;
};
}
static Args readArgs();

namespace
{

struct Hash {
    QByteArray hash;
    Err error;
};
}
static Hash createFastHash(const QString &path, const Args &args);
static Hash createQCryptographicHash(const QString &path, const Args &args);

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const Args args = readArgs();

    const auto start = std::chrono::steady_clock::now();

    const Hash hash = !args.useQCryptographicHashDirectly
        ? createFastHash(args.path, args)
        : createQCryptographicHash(args.path, args);

    const auto end = std::chrono::steady_clock::now();
    const auto diff = end - start;

    if (hash.error != kNoErr) {
        qDebug() << "error during hash calculation";
    } else {
        qDebug() << "hash:" << hash.hash;
        qDebug() << qSetRealNumberPrecision(10) << "created in"
                 << std::chrono::duration<double, std::milli>(diff).count() << "ms";
    }

    return hash.error == kNoErr ? 0 : 1;
}

static Hash createFastHash(const QString &path, const Args &args)
{
    Hash result;

    result.error = FastFileFolderHash::hash(path, args.algorithm, args.sampleCount, args.sampleSize,
                                            args.minSizeForFastHashMultiplier,
                                            args.maxFileCountInFolderToUse, &result.hash);
    result.hash = result.hash.toHex();

    return result;
}

static Hash createQCryptographicHash(const QString &path, const Args &args)
{
    Hash result;
    const QFileInfo fileInfo(path);

    if (!fileInfo.isFile()) {
        result.error = kFileOpenError;

        return result;
    }

    QFile file(path);

    if (!file.open(QIODevice::ReadOnly)) {
        result.error = kFileOpenError;

        return result;
    }

    QCryptographicHash hash(args.algorithm);

    hash.addData(&file);

    result.error = kNoErr;
    result.hash = hash.result().toHex();

    return result;
}

static QCryptographicHash::Algorithm tryGetAlgorithm(const QCommandLineParser &parser,
                                                     const QCommandLineOption &algorithmOption);
static int tryGetSampleCount(const QCommandLineParser &parser,
                             const QCommandLineOption &sampleCountOption);
static qint64 tryGetSampleSize(const QCommandLineParser &parser,
                               const QCommandLineOption &sampleSizeOption);
static int
tryGetMinSizeForFastHashMultiplier(const QCommandLineParser &parser,
                                   const QCommandLineOption &minSizeForFastHashMultiplierOption);
static int
tryGetMaxFileCountInFolderToUse(const QCommandLineParser &parser,
                                const QCommandLineOption &maxFileCountInFolderToUseOption);

static QCommandLineOption addUseQCryptographicHashDirectlyOption(QCommandLineParser *parser);
static QCommandLineOption addAlgorithmOption(QCommandLineParser *parser);
static QCommandLineOption addSampleCountOption(QCommandLineParser *parser);
static QCommandLineOption addSampleSizeOption(QCommandLineParser *parser);
static QCommandLineOption addMinSizeForFastHashMultiplierOption(QCommandLineParser *parser);
static QCommandLineOption addMaxFileCountInFolderToUseOption(QCommandLineParser *parser);
static void addPathPositionalArgument(QCommandLineParser *parser);

static Args readArgs()
{
    QCommandLineParser parser;

    parser.addHelpOption();
    const auto useQCryptographicHashDirectlyOption
        = addUseQCryptographicHashDirectlyOption(&parser);
    const auto algorithmOption = addAlgorithmOption(&parser);
    const auto sampleCountOption = addSampleCountOption(&parser);
    const auto sampleSizeOption = addSampleSizeOption(&parser);
    const auto minSizeForFastHashMultiplierOption = addMinSizeForFastHashMultiplierOption(&parser);
    const auto maxFileCountInFolderToUseOption = addMaxFileCountInFolderToUseOption(&parser);
    addPathPositionalArgument(&parser);
    parser.process(*QCoreApplication::instance());

    const QStringList rawArgs = parser.positionalArguments();

    if (rawArgs.size() != 1) {
        parser.showHelp(1);
    }

    return { rawArgs.at(0),
             parser.isSet(useQCryptographicHashDirectlyOption),
             tryGetAlgorithm(parser, algorithmOption),
             tryGetSampleCount(parser, sampleCountOption),
             tryGetSampleSize(parser, sampleSizeOption),
             tryGetMinSizeForFastHashMultiplier(parser, minSizeForFastHashMultiplierOption),
             tryGetMaxFileCountInFolderToUse(parser, maxFileCountInFolderToUseOption) };
}

static QCryptographicHash::Algorithm tryGetAlgorithm(const QCommandLineParser &parser,
                                                     const QCommandLineOption &algorithmOption)
{
    const QByteArray algorithmName = parser.value(algorithmOption).toLatin1();

    if (isSupportedHashAlgorithmName(algorithmName)) {
        return hashAlgorithmByNameUnsafe(algorithmName);
    }

    qDebug() << "unsupported algorithm value detected:" << algorithmName;
    qDebug() << "used default:" << DEFAULT_ALGORITHM_NAME;

    return hashAlgorithmByNameUnsafe(DEFAULT_ALGORITHM_NAME);
}

static int tryGetSampleCount(const QCommandLineParser &parser,
                             const QCommandLineOption &sampleCountOption)
{
    bool sampleCountParseOk = false;
    const int sampleCount = parser.value(sampleCountOption).toInt(&sampleCountParseOk);

    if (sampleCountParseOk && sampleCount > 0) {
        return sampleCount;
    }

    qDebug() << "unsupported sample_count value detected:" << sampleCount;
    qDebug() << "used default:" << DEFAULT_SAMPLE_COUNT;

    return DEFAULT_SAMPLE_COUNT;
}

static qint64 tryGetSampleSize(const QCommandLineParser &parser,
                               const QCommandLineOption &sampleSizeOption)
{
    bool sampleSizeParseOk = false;
    const qint64 sampleSize = parser.value(sampleSizeOption).toInt(&sampleSizeParseOk);

    if (sampleSizeParseOk && sampleSize > 0) {
        return sampleSize * 1024;
    }

    qDebug() << "unsupported sample_size value detected:" << sampleSize;
    qDebug() << "used default:" << DEFAULT_SAMPLE_SIZE;

    return DEFAULT_SAMPLE_SIZE * 1024;
}

static int
tryGetMinSizeForFastHashMultiplier(const QCommandLineParser &parser,
                                   const QCommandLineOption &minSizeForFastHashMultiplierOption)
{
    bool minSizeForFastHashMultiplierParseOk = false;
    const int minSizeForFastHashMultiplier = parser.value(minSizeForFastHashMultiplierOption)
                                                 .toInt(&minSizeForFastHashMultiplierParseOk);

    if (minSizeForFastHashMultiplierParseOk && minSizeForFastHashMultiplier > 0) {
        return minSizeForFastHashMultiplier;
    }

    qDebug() << "unsupported min_size_for_fast_hash_multiplier value detected:"
                << minSizeForFastHashMultiplier;
    qDebug() << "used default:" << DEFAULT_MIN_SIZE_FOR_FAST_HASH_MULTIPLIER;

    return DEFAULT_MIN_SIZE_FOR_FAST_HASH_MULTIPLIER;
}

static int
tryGetMaxFileCountInFolderToUse(const QCommandLineParser &parser,
                                const QCommandLineOption &maxFileCountInFolderToUseOption)
{
    bool maxFileCountInFolderToUseParseOk = false;
    const int maxFileCountInFolderToUse
        = parser.value(maxFileCountInFolderToUseOption).toInt(&maxFileCountInFolderToUseParseOk);

    if (maxFileCountInFolderToUseParseOk && maxFileCountInFolderToUse > 0) {
        return maxFileCountInFolderToUse;
    }

    qDebug() << "unsupported max_file_count_in_folder_to_use value detected:"
                << maxFileCountInFolderToUse;
    qDebug() << "used default:" << DEFAULT_MAX_FILE_COUNT_IN_FOLDER_TO_USE;

    return DEFAULT_MAX_FILE_COUNT_IN_FOLDER_TO_USE;
}

static void addPathPositionalArgument(QCommandLineParser *parser)
{
    parser->addPositionalArgument(QLatin1String("path"),
                                  QLatin1String("input path to create hash (file or folder)"));
}

static QCommandLineOption addUseQCryptographicHashDirectlyOption(QCommandLineParser *parser)
{
    const QCommandLineOption useQCryptographicHashDirectlyOption(
        QLatin1String("q"),
        QLatin1String("use QCryptographicHash directly (only a single file is supported)"));

    parser->addOption(useQCryptographicHashDirectlyOption);

    return useQCryptographicHashDirectlyOption;
}

static QCommandLineOption addAlgorithmOption(QCommandLineParser *parser)
{
    QStringList algorithmNames;

    for (const auto &algorithmName : supportedHashAlgorithmNames()) {
        algorithmNames.push_back(QString::fromLatin1(algorithmName));
    }

    const QCommandLineOption algorithmOption(
        QLatin1String("a"),
        QString(QLatin1String("hash algorithm (supported values: %1; default: %2)"))
            .arg(algorithmNames.join(QLatin1String(", ")))
            .arg(QLatin1String(DEFAULT_ALGORITHM_NAME)),
        QLatin1String("algorithm"), QLatin1String(DEFAULT_ALGORITHM_NAME));

    parser->addOption(algorithmOption);

    return algorithmOption;
}

static QCommandLineOption addSampleCountOption(QCommandLineParser *parser)
{
    const QCommandLineOption sampleCountOption(
        QLatin1String("c"),
        QString(QLatin1String("sample count for fast hash (default: %1)"))
            .arg(DEFAULT_SAMPLE_COUNT),
        QLatin1String("sample_count"), QString::number(DEFAULT_SAMPLE_COUNT));

    parser->addOption(sampleCountOption);

    return sampleCountOption;
}

static QCommandLineOption addSampleSizeOption(QCommandLineParser *parser)
{
    const QCommandLineOption sampleSizeOption(
        QLatin1String("s"),
        QString(QLatin1String("sample size for fast hash in kb (default: %1)"))
            .arg(DEFAULT_SAMPLE_SIZE),
        QLatin1String("sample_size"), QString::number(DEFAULT_SAMPLE_SIZE));

    parser->addOption(sampleSizeOption);

    return sampleSizeOption;
}

static QCommandLineOption addMinSizeForFastHashMultiplierOption(QCommandLineParser *parser)
{
    const QCommandLineOption minSizeForFastHashMultiplierOption(
        QLatin1String("m"),
        QString(QLatin1String("min size for fast hash multiplier (default: %1)"))
            .arg(DEFAULT_MIN_SIZE_FOR_FAST_HASH_MULTIPLIER),
        QLatin1String("min_size_for_fast_hash_multiplier"),
        QString::number(DEFAULT_MIN_SIZE_FOR_FAST_HASH_MULTIPLIER));

    parser->addOption(minSizeForFastHashMultiplierOption);

    return minSizeForFastHashMultiplierOption;
}

static QCommandLineOption addMaxFileCountInFolderToUseOption(QCommandLineParser *parser)
{
    const QCommandLineOption maxFileCountInFolderToUseOption(
        QLatin1String("f"),
        QString(QLatin1String("max file count in folder to use (default: %1)"))
            .arg(DEFAULT_MAX_FILE_COUNT_IN_FOLDER_TO_USE),
        QLatin1String("max_file_count_in_folder_to_use"),
        QString::number(DEFAULT_MAX_FILE_COUNT_IN_FOLDER_TO_USE));

    parser->addOption(maxFileCountInFolderToUseOption);

    return maxFileCountInFolderToUseOption;
}
