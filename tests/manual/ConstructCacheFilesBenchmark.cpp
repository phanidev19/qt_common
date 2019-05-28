/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include <MSReader.h>

#include <VendorPathChecker.h>

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDirIterator>

#include <chrono>

using namespace pmi;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;

    parser.addHelpOption();
    parser.addPositionalArgument(QLatin1String("folder_to_process"),
                                 QString("input folder to process files from and create cache"));
    const QCommandLineOption threadCountOption(
        QStringList() << QLatin1String("t") << QLatin1String("thread_count"),
        QString("specify thread count (and related process count) for load balancing"),
        QLatin1String("thread_count"), QString(QLatin1String("%1")).arg(0));
    parser.addOption(threadCountOption);
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.count() != 1) {
        parser.showHelp(1);
    }

    const QString folderToProcess = args.at(0);
    const int threadCount = parser.value(threadCountOption).toInt();
    QDirIterator it(folderToProcess);
    QStringList files;

    while (it.hasNext()) {
        const QString file = it.next();

        if (VendorPathChecker::isMassSpectrumPath(file,
                                                  VendorPathChecker::CheckMethodCheckPathExists)) {
            files << file;
        }
    }

    const auto start = std::chrono::steady_clock::now();

    const Err error = MSReader::constructCacheFiles(files, threadCount);

    const auto end = std::chrono::steady_clock::now();
    const auto diff = end - start;

    qDebug() << "MSReader::constructCacheFiles() result:" << error;
    qDebug() << threadCount << "threads (processes);" << std::chrono::duration<double>(diff).count()
             << "sec";

    return error == kNoErr ? 0 : 1;
}
