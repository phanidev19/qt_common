/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#include "PMCommandLineParser.h"
#include <QDebug>

const char *PMCommandLineParser::SOURCE_STRING = "source";
const char *PMCommandLineParser::OUTPUT_STRING = "output";
const char *PMCommandLineParser::START_STRING = "start";
const char *PMCommandLineParser::END_STRING = "end";
const char *PMCommandLineParser::CENTROID_STRING = "centroid";
const char *PMCommandLineParser::COMMAND_STRING = "command";
const char *PMCommandLineParser::LOCK_MASS_LIST_STRING = "lockmasslist";
const char *PMCommandLineParser::TOLERANCE_STRING = "tolerance";
const char *PMCommandLineParser::MS_CONVERT_OPTIONS = "msconvertoptions";
const char *PMCommandLineParser::FORMAT_STRING = "format";
const char *PMCommandLineParser::PRESET_STRING = "preset";

const char *PMCommandLineParser::IMO_SUMMING_TOLERANCE_STRING = "imo.summingTolerance";
const char *PMCommandLineParser::IMO_MERGING_TOLERANCE_STRING = "imo.mergingTolerance";
const char *PMCommandLineParser::IMO_SUMMING_UNIT_STRING = "imo.summingUnit";
const char *PMCommandLineParser::IMO_MERGING_UNIT_STRING = "imo.mergingUnit";
const char *PMCommandLineParser::IMO_RETAIN_NUMBER_OF_PEAKS_STRING = "imo.retainNumberOfPeaks";

const char *PMCommandLineParser::WRITE_FULL_XY_COMPARISON_STRING = "writeFull";
const char *PMCommandLineParser::COMPARE_BY_INDEX_STRING = "compareByIndex";
const char PMCommandLineParser::COMPARE_BY_INDEX_CHAR = 'i';
const char PMCommandLineParser::WRITE_FULL_XY_COMPARISON_CHAR = 'w';

const char PMCommandLineParser::OUTPUT_CHAR = 'o';
const char PMCommandLineParser::START_CHAR = 's';
const char PMCommandLineParser::END_CHAR = 'e';
const char PMCommandLineParser::CENTROID_CHAR = 'c';
const char PMCommandLineParser::LOCK_MASS_LIST_CHAR = 'l';
const char PMCommandLineParser::TOLERANCE_CHAR = 't';
const char PMCommandLineParser::COMMA_CHAR = ',';
const char PMCommandLineParser::MS_CONVERT_OPTIONS_CHAR = 'm';
const char PMCommandLineParser::FORMAT_CHAR = 'f';
const char PMCommandLineParser::PRESET_CHAR = 'p';

PMCommandLineParser::PMCommandLineParser()
    : m_sourceFileName()
    , m_outputFolderPath()
    , m_format("")
    , m_presetFilePath("")
    , m_msConvertOption(pmi::msreader::ConvertFull)
    , m_startScan(0)
    , m_endScan(0)
    , m_doCentroid(false)
    , m_compareByIndex(false)
    , m_writeFullXYDiff(false)
{
}

QList<double> PMCommandLineParser::lockMassList() const
{
    return m_lockMassList;
}

bool PMCommandLineParser::parseSourceFile()
{
    QStringList stringList(positionalArguments());

    if (stringList.size() != 1) {
        qWarning() << "Source file is not specified or invalid number of source files";
        return false;
    }
    m_sourceFileName = stringList[0];

    return true;
}

bool PMCommandLineParser::parseOutputPath()
{
    m_outputFolderPath = value(QString(OUTPUT_CHAR));

    if (m_outputFolderPath.isEmpty()) {
        qWarning() << "Output folder path not specified";
        return false;
    }

    return true;
}

bool PMCommandLineParser::validateAndParseForCalibrationPointsCommand()
{
    if (!parseSourceFile()) {
        return false;
    }

    if (!parseOutputPath()) {
        return false;
    }

    if (!parseLockMassList()) {
        return false;
    }

    QString data(value(TOLERANCE_STRING));

    if (data.isEmpty()) {
        qWarning() << "Tolerance value not specified";
        return false;
    }

    bool result = true;
    m_tolerance = data.toDouble(&result);

    if (!result) {
        qWarning() << "Invalid data entered for tolerance";
        return false;
    }
    return true;
}

bool PMCommandLineParser::validateAndParseForMakeByspec2Command()
{
    if (!parseSourceFile()) {
        return false;
    }

    if (!parseOutputPath()) {
        return false;
    }

    if (!parseIonMobilityOptions()) {
        return false;
    }

    m_doCentroid = isSet(QString(CENTROID_CHAR));

    if (isSet(MS_CONVERT_OPTIONS)) {
        QString data(value(MS_CONVERT_OPTIONS));
        if (data.isEmpty()) {
            qWarning() << "MSConvertOptions not specified";
            return false;
        }

        bool result = true;
        m_msConvertOption = static_cast<pmi::msreader::MSConvertOption>(data.toInt(&result));

        if (!result || (pmi::msreader::ConvertFull < m_msConvertOption
                        || m_msConvertOption < pmi::msreader::ConvertChronosOnly)) {
            qWarning() << "Invalid data entered for MSConvertOptions";
            return false;
        }
    }

    return true;
}

bool PMCommandLineParser::validateAndParseForNonUniformCacheCommand()
{
    if (!parseSourceFile()) {
        return false;
    }

    m_doCentroid = isSet(QString(CENTROID_CHAR));
    return true;
}

bool PMCommandLineParser::validateAndParseForComparisonCommand()
{
    m_sourceFileNameList = positionalArguments();

    if (m_sourceFileNameList.size() != 2) {
        qWarning() << "Source file is not specified or invalid number of source files (two expected)";
        return false;
    }

    if (!parseOutputPath()) {
        return false;
    }

    m_compareByIndex = isSet(QString(COMPARE_BY_INDEX_CHAR));
    m_doCentroid = isSet(QString(CENTROID_CHAR));

    return true;
}

bool PMCommandLineParser::parseIonMobilityOptions()
{
    m_ionMobilityOptions = pmi::msreader::IonMobilityOptions();
    if (isSet(IMO_SUMMING_TOLERANCE_STRING)) {
        QString data(value(IMO_SUMMING_TOLERANCE_STRING));
        if (data.isEmpty()) {
            qWarning() << "IonMobility Summing Tolerance Option not specified";
            return false;
        }

        bool result = true;
        m_ionMobilityOptions.summingTolerance = data.toDouble(&result);
        if (!result || m_ionMobilityOptions.summingTolerance < 0) {
            qWarning() << "Invalid data entered for IonMobility Summing Tolerance Option";
            return false;
        }
    }
    if (isSet(IMO_MERGING_TOLERANCE_STRING)) {
        QString data(value(IMO_MERGING_TOLERANCE_STRING));
        if (data.isEmpty()) {
            qWarning() << "IonMobility Merging Tolerance Option not specified";
            return false;
        }

        bool result = true;
        m_ionMobilityOptions.mergingTolerance = data.toDouble(&result);
        if (!result || m_ionMobilityOptions.mergingTolerance < 0) {
            qWarning() << "Invalid data entered for IonMobility Merging Tolerance Option";
            return false;
        }
    }
    if (isSet(IMO_SUMMING_UNIT_STRING)) {
        QString data(value(IMO_SUMMING_UNIT_STRING));
        if (data.isEmpty()) {
            qWarning() << "IonMobility Summing Unit Option not specified";
            return false;
        }

        bool result = true;
        m_ionMobilityOptions.summingUnit
            = static_cast<pmi::msreader::MassUnit>(data.toInt(&result));
        if (!result
            || (pmi::msreader::MassUnit::MassUnit_Dalton < m_ionMobilityOptions.summingUnit
                || m_ionMobilityOptions.summingUnit < pmi::msreader::MassUnit::MassUnit_PPM)) {
            qWarning() << "Invalid data entered for IonMobility Summing Unit Option";
            return false;
        }
    }
    if (isSet(IMO_MERGING_UNIT_STRING)) {
        if (isSet(IMO_MERGING_UNIT_STRING)) {
            QString data(value(IMO_MERGING_UNIT_STRING));
            if (data.isEmpty()) {
                qWarning() << "IonMobility Merging Unit Option not specified";
                return false;
            }

            bool result = true;
            m_ionMobilityOptions.mergingUnit
                = static_cast<pmi::msreader::MassUnit>(data.toInt(&result));
            if (!result
                || (pmi::msreader::MassUnit::MassUnit_Dalton < m_ionMobilityOptions.mergingUnit
                    || m_ionMobilityOptions.mergingUnit < pmi::msreader::MassUnit::MassUnit_PPM)) {
                qWarning() << "Invalid data entered for IonMobility Merging Unit Option";
                return false;
            }
        }
    }
    if (isSet(IMO_RETAIN_NUMBER_OF_PEAKS_STRING)) {
        QString data(value(IMO_RETAIN_NUMBER_OF_PEAKS_STRING));
        if (data.isEmpty()) {
            qWarning() << "IonMobility Retain Number Of Peaks Option not specified";
            return false;
        }

        bool result = true;
        m_ionMobilityOptions.retainNumberOfPeaks = data.toInt(&result);
        if (!result) {
            qWarning() << "Invalid data entered for IonMobility Retain Number Of Peaks Option";
            return false;
        }
    }

    return true;
}

bool PMCommandLineParser::validateAndProcessCommand(const QStringList &arguments)
{
    QStringList argumentsLocal(arguments);
    if (argumentsLocal.size() == 1) {
        argumentsLocal.append("-h");
    }

    process(argumentsLocal);

    bool result = false;
    m_commandId = static_cast<CommandId>(value(COMMAND_STRING).toInt(&result));

    return result;
}

bool PMCommandLineParser::validateAndParseForMassIntensityCommand()
{
    if (!parseSourceFile()) {
        return false;
    }

    if (!parseOutputPath()) {
        return false;
    }

    QString data;
    data = value(QString(START_CHAR));

    if (data.isEmpty()) {
        qWarning() << "Start scan not specified";
        return false;
    }
    bool result = true;
    m_startScan = data.toInt(&result);

    if (!result) {
        qWarning() << "Invalid start scan value";
        return false;
    }

    data = value(QString(END_CHAR));

    if (data.isEmpty()) {
        qWarning() << "End scan not specified";
        return false;
    }

    result = true;
    m_endScan = data.toInt(&result);

    if (!result) {
        qWarning() << "Invalid end scan value";
        return false;
    }

    m_doCentroid = isSet(QString(CENTROID_CHAR));
    return true;
}

QString PMCommandLineParser::sourceFile() const
{
    return m_sourceFileName;
}

QStringList PMCommandLineParser::sourceFiles() const
{
    return m_sourceFileNameList;
}

QString PMCommandLineParser::outputPath() const
{
    return m_outputFolderPath;
}

PMCommandLineParser::CommandId PMCommandLineParser::commandId() const
{
    return m_commandId;
}

pmi::msreader::MSConvertOption PMCommandLineParser::msConvertOptions() const
{
    return m_msConvertOption;
}

pmi::msreader::IonMobilityOptions PMCommandLineParser::ionMobilityOptions() const
{
    return m_ionMobilityOptions;
}

double PMCommandLineParser::tolerance() const
{
    return m_tolerance;
}

long PMCommandLineParser::startScan() const
{
    return m_startScan;
}

long PMCommandLineParser::endScan() const
{
    return m_endScan;
}

bool PMCommandLineParser::doCentroiding() const
{
    return m_doCentroid;
}

bool PMCommandLineParser::compareByIndex() const
{
    return m_compareByIndex;
}

bool PMCommandLineParser::writeFullXYDiff() const
{
    return m_writeFullXYDiff;
}

void PMCommandLineParser::addConvertOption()
{
    addOption(
        QCommandLineOption(QStringList() << QString(MS_CONVERT_OPTIONS_CHAR) << MS_CONVERT_OPTIONS,
                           QObject::tr("0. ConvertChronosOnly\n"
                                       "1. ConvertWithCentroidButNoPeaksBlobs\n"
                                       "2. ConvertFull"),
                           "MSConvertOptions"));
}

void PMCommandLineParser::addOutputOption()
{
    addOption(QCommandLineOption(
        QStringList() << QString(OUTPUT_CHAR) << OUTPUT_STRING,
        QObject::tr("Output folder path or full file path (for byspec2 writer)"), "output_path"));
}

void PMCommandLineParser::addIonMobilityOptions()
{
    const pmi::msreader::IonMobilityOptions defaultValue
        = pmi::msreader::IonMobilityOptions::defaultValues();

    addOption(QCommandLineOption(QStringList() << QString(IMO_SUMMING_TOLERANCE_STRING),
                                 QObject::tr("IonMobility Summing Tolerance"), "tolerance",
                                 QString::number(defaultValue.summingTolerance)));

    addOption(QCommandLineOption(QStringList() << QString(IMO_MERGING_TOLERANCE_STRING),
                                 QObject::tr("IonMobility Merging Tolerance"), "tolerance",
                                 QString::number(defaultValue.mergingTolerance)));

    addOption(QCommandLineOption(QStringList() << QString(IMO_SUMMING_UNIT_STRING),
                                 QObject::tr("IonMobility Summing Unit\n"
                                             "0. PPM\n"
                                             "1. Dalton"),
                                 "unit", QString::number(defaultValue.summingUnit)));

    addOption(QCommandLineOption(QStringList() << QString(IMO_MERGING_UNIT_STRING),
                                 QObject::tr("IonMobility Merging Unit\n"
                                             "0. PPM\n"
                                             "1. Dalton"),
                                 "unit", QString::number(defaultValue.mergingUnit)));

    addOption(QCommandLineOption(QStringList() << QString(IMO_RETAIN_NUMBER_OF_PEAKS_STRING),
                                 QObject::tr("IonMobility Retain Number Of Peaks"), "number",
                                 QString::number(defaultValue.retainNumberOfPeaks)));
}

bool PMCommandLineParser::parseLockMassList()
{
    m_lockMassList.clear();
    QString lockMassList(value(LOCK_MASS_LIST_STRING));

    if (lockMassList.isEmpty()) {
        qWarning() << "Lock mass list not specified";
        return false;
    }

    QStringList stringList(lockMassList.split(COMMA_CHAR));

    for (const QString &string : stringList) {
        bool result = true;
        double mass = string.toDouble(&result);

        if (!result) {
            m_lockMassList.clear();
            qWarning() << "Invalid data entered for lock mass list";
            return false;
        }
        m_lockMassList.append(mass);
    }
    return true;
}
