/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Dmitry Pischenko (dmitry.pischenko@toplab.io)
 */

#include "MSConvertCommandLineParser.h"

static const QString APPLICATION_DESCRIPTION = QStringLiteral("PMi-MSConvert - Create byspec2 files PMI");

MSConvertCommandLineParser::MSConvertCommandLineParser()
{
    addHelpOption();
    addPositionalArgument(SOURCE_STRING, QObject::tr("Source file"));

    setApplicationDescription(APPLICATION_DESCRIPTION);
    QCommandLineOption commandOption(QStringList() << QString(COMMAND_STRING),
                                     QObject::tr("Deprecated option"), "command", "4");
    commandOption.setFlags(QCommandLineOption::HiddenFromHelp);
    addOption(commandOption);

    addOutputOption();

    addOption(QCommandLineOption(QStringList() << QString(CENTROID_CHAR) << CENTROID_STRING,
                                 QObject::tr("Convert only centroid data")));

    addConvertOption();
    addIonMobilityOptions();

}

