/*
 *  Copyright (C) 2017 by Protein Metrics Inc. - All Rights Reserved.
 *  Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 *  Confidential.
 *
 *  Author  : Yashpalsinh Gohil (yash.gohiil@gmail.com)
 */

#ifndef PMCOMMANDLINEPARSER_H
#define PMCOMMANDLINEPARSER_H

#include <QCommandLineParser>
#include "MSReaderTypes.h"
#include "MSReaderBase.h"

class PMCommandLineParser : public QCommandLineParser
{
    Q_DECLARE_TR_FUNCTIONS(PMCommandLineParser)
public:
    enum CommandId {
        CommandNone,
        CommandMassIntensity,
        CommandCalibrationPoint,
        CommandMakeBySpec2,
        CommandMakeBySpec2Pmi,
        CommandNonUniformCache,
        CommandCompare,
        CommandCompareMSFiles,
        MaxCommand
    };
    PMCommandLineParser();

    QList<double> lockMassList() const;
    QString sourceFile() const;
    QStringList sourceFiles() const;
    QString outputPath() const;
    CommandId commandId() const;
    pmi::msreader::MSConvertOption msConvertOptions() const;
    pmi::msreader::IonMobilityOptions ionMobilityOptions() const;
    double tolerance() const;
    long startScan() const;
    long endScan() const;

    bool validateAndProcessCommand(const QStringList &arguments);
    bool validateAndParseForMassIntensityCommand();
    bool validateAndParseForCalibrationPointsCommand();
    bool validateAndParseForMakeByspec2Command();
    bool validateAndParseForNonUniformCacheCommand();
    bool validateAndParseForComparisonCommand();
    bool doCentroiding() const;
    bool compareByIndex() const;
    bool writeFullXYDiff() const;

protected:
    void addConvertOption();
    void addOutputOption();
    void addIonMobilityOptions();

protected:
    static const char *SOURCE_STRING;
    static const char *OUTPUT_STRING;
    static const char *START_STRING;
    static const char *END_STRING;
    static const char *CENTROID_STRING;
    static const char *COMMAND_STRING;
    static const char *LOCK_MASS_LIST_STRING;
    static const char *TOLERANCE_STRING;
    static const char *MS_CONVERT_OPTIONS;
    static const char *FORMAT_STRING;
    static const char *PRESET_STRING;

    static const char *IMO_SUMMING_TOLERANCE_STRING;
    static const char *IMO_MERGING_TOLERANCE_STRING;
    static const char *IMO_SUMMING_UNIT_STRING;
    static const char *IMO_MERGING_UNIT_STRING;
    static const char *IMO_RETAIN_NUMBER_OF_PEAKS_STRING;

    static const char *WRITE_FULL_XY_COMPARISON_STRING;
    static const char *COMPARE_BY_INDEX_STRING;
    static const char COMPARE_BY_INDEX_CHAR;
    static const char WRITE_FULL_XY_COMPARISON_CHAR;

    static const char OUTPUT_CHAR;
    static const char START_CHAR;
    static const char END_CHAR;
    static const char CENTROID_CHAR;
    static const char LOCK_MASS_LIST_CHAR;
    static const char TOLERANCE_CHAR;
    static const char COMMA_CHAR;
    static const char MS_CONVERT_OPTIONS_CHAR;
    static const char FORMAT_CHAR;
    static const char PRESET_CHAR;

private:
    bool parseLockMassList();
    bool parseSourceFile();
    bool parseOutputPath();
    bool parseIonMobilityOptions();

    QList<double> m_lockMassList;
    QString m_sourceFileName;
    QString m_outputFolderPath;
    QString m_format;
    QString m_presetFilePath;
    QStringList m_sourceFileNameList;
    CommandId m_commandId;
    pmi::msreader::MSConvertOption m_msConvertOption;
    pmi::msreader::IonMobilityOptions m_ionMobilityOptions;
    double m_tolerance;
    long m_startScan;
    long m_endScan;
    bool m_doCentroid;
    bool m_compareByIndex;
    bool m_writeFullXYDiff;
};

#endif // PMCOMMANDLINEPARSER_H
