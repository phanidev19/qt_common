/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Dmitry Pischenko (dmitry.pischenko@toplab.io)
 */


#include "MakeByspec2PmiCommand.h"
#include "MSConvertCommandLineParser.h"
#include "MSReader.h"
#include "MSReaderCommand.h"

#include <ComInitializer.h>

#include <QCoreApplication>
#include <QDebug>

pmi::Err process(MSConvertCommandLineParser *parser)
{
    pmi::Err e = pmi::kNoErr;

    if (parser->commandId() != 4) {
        qWarning() << "Invalid command ID.";
        return pmi::kBadParameterError;
    }

    QScopedPointer<MSReaderCommand> command(new MakeByspec2PmiCommand(parser));
    return command->execute();
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    pmi::ComInitializer comInitializer;
    MSConvertCommandLineParser parser;

    pmi::Err e = pmi::kNoErr;
    if (parser.validateAndProcessCommand(app.arguments())) {
        e = process(&parser);
        if (pmi::kNoErr != e) {
            qWarning() << "Error occured with code" << e;
            return e;
        }
        qInfo() << "Operation successful";
    }

    return 0;
}
