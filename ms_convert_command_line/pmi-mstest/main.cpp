/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "MSCommandLineReader.h"

#include "ComparisonCommand.h"

#include <ComInitializer.h>

#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    pmi::ComInitializer comInitializer;
    MSTestCommandLineParser parser;

    pmi::Err e = pmi::kNoErr;
    if (parser.validateAndProcessCommand(app.arguments())) {
        MSCommandLineReader reader(&parser);
        e = reader.process();
        if (pmi::kNoErr != e) {
            qWarning() << "Error occured with code" << e;
            return e;
        }
        qInfo() << "Operation successful";
    }

    return 0;
}
