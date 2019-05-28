/*
 *  Copyright (C) 2018 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Dmitry Pischenko (dmitry.pischenko@toplab.io)
 */

#include "MSTestCommandLineParser.h"

static const QString APPLICATION_DESCRIPTION = QStringLiteral("PMi-MSTest - testing tool");

MSTestCommandLineParser::MSTestCommandLineParser()
{
    setApplicationDescription(APPLICATION_DESCRIPTION);
    addHelpOption();
    addPositionalArgument(SOURCE_STRING, QObject::tr("Source file"));

    addOption(QCommandLineOption(
        QStringList() << QString(COMMAND_STRING),
        QObject::tr("1. Extract Mass & Intensity \n "
                    "2. Extract Calibration points \n"
                    "3. Create byspec2 files Native \n"
                    "4. Create byspec2 files PMI\n"
                    "5. Non-uniform cache\n"
                    "6. Compare MS Files"), "command"));

    addOutputOption();

    addOption(QCommandLineOption(QStringList() << QString(CENTROID_CHAR) << CENTROID_STRING,
                                 QObject::tr("Do centroiding, or, in the context of command 6 (comparison), tells the program that the files are centroided")));

    addOption(QCommandLineOption(QStringList() << QString(START_CHAR) << START_STRING,
                                 QObject::tr("Start scan"), "start_scan"));

    addOption(QCommandLineOption(QStringList() << QString(END_CHAR) << END_STRING,
                                 QObject::tr("End scan"), "end_scan"));

    addOption(QCommandLineOption(
        QStringList() << QString(LOCK_MASS_LIST_CHAR) << LOCK_MASS_LIST_STRING,
        QObject::tr("List of LockMass, CSV style data e.g. (121.050873,322.048121,622.028960)"),
        "lock_mass_list"));

    addOption(QCommandLineOption(QStringList() << QString(TOLERANCE_CHAR) << TOLERANCE_STRING,
                                 QObject::tr("Tolerance window for lock mass list (ppm)"),
                                 "tolerance"));

    addConvertOption();

    addOption(QCommandLineOption(QStringList() << QString(FORMAT_CHAR) << FORMAT_STRING,
                                 QObject::tr("non-uniform"
                                             "uniform"),
                                 "Format"));

    addOption(QCommandLineOption(QStringList() << QString(PRESET_CHAR) << PRESET_STRING,
                                 QObject::tr("preset file path (.json)"),
                                 "Preset"));
    addOption(QCommandLineOption(QStringList() << QString(COMPARE_BY_INDEX_CHAR) << COMPARE_BY_INDEX_STRING,
        QObject::tr("Compare scan x-y data by index (when using --command 6); Default is comparing by co - ordinate")));
    
    addOption(QCommandLineOption(QStringList() << QString(WRITE_FULL_XY_COMPARISON_CHAR) << WRITE_FULL_XY_COMPARISON_STRING,
        QObject::tr("Print information about *every* differing x-y coordinate between the files (when using --command 6)")));

    addIonMobilityOptions();
}

