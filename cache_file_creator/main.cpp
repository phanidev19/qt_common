/*
 * Copyright (C) 2017 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Ivan Skiba ivan.skiba@redgreenorange.com
 */

#include <ComInitializer.h>
#include <MSReader.h>

#include <QCommandLineParser>
#include <QCoreApplication>

using namespace pmi;

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    ComInitializer comInitializer;

    parser.addHelpOption();
    parser.addPositionalArgument(QLatin1String("file_name"),
                                 QString("input file name to create cache"));
    parser.process(app);

    const QStringList args = parser.positionalArguments();

    if (args.count() != 1) {
        parser.showHelp(1);
    }

    if (MSReader::Instance()->openFile(args.at(0)) != kNoErr) {
        return 1;
    }

    if (MSReader::Instance()->createPrefixSumCache() != kNoErr) {
        return 1;
    }

    return 0;
}
