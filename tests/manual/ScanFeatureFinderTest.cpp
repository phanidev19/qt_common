/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "CrossSampleFeatureCollatorTurbo.h"
#include "InsilicoGenerator.h"
#include "MultiSampleScanFeatureFinder.h"
#include "PQMFeatureMatcher.h"
#include "ScanIterator.h"

#include <CsvReader.h>
#include <PmiMemoryInfo.h>

#include <ComInitializer.h>

#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QString>

using namespace pmi;

const int TEST_RESULT_SUCCESS = 0;
const int TEST_RESULT_FAIL = 1;

int testMultiSampleScanFeatureFinder(const QVector<SampleSearch> &msFiles)
{
    SettableFeatureFinderParameters ffUserParams;

    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");

    MultiSampleScanFeatureFinder finder(msFiles, neuralNetworkDbFilePath, ffUserParams);

    FeatureMatcherSettings settings;
    // modify the score for MS2 search
    settings.score = 140;
    finder.setMatchingSettings(settings);
    finder.setWorkingDirectory(QDir());

    Err e = finder.init();
    if (e != kNoErr) {
        qDebug() << finder.errorMessage();
        return TEST_RESULT_FAIL;
    }

    e = finder.findFeatures();
    if (e != kNoErr) {
        qDebug() << finder.errorMessage();
    }

    for (const QString &connectionName : QSqlDatabase::connectionNames()) {
        if (QSqlDatabase::database(connectionName, false).isOpen()) {
            qDebug() << connectionName << "connectionName stayed open!";
        }
    }

    qDebug() << "Process peak memory" << pmi::MemoryInfo::peakProcessMemory();

    return TEST_RESULT_SUCCESS;
}

QVector<SampleSearch> loadFromCsvFile(const QString &csvFilePath)
{
    QVector<SampleSearch> result;

    CsvReader csvReader(csvFilePath);
    if (!csvReader.open()) {
        qWarning() << "Cannot open" << csvFilePath;
        return result;
    }

    QList<QStringList> rows;
    csvReader.readAllRows(&rows);
    if (rows.size() < 2) {
        qWarning() << "Invalid CSV format. Header with 'Filename' column and at least one path is "
                      "required!";
        return result;
    }

    // skip first row, thus 1
    for (int i = 1; i < rows.size(); ++i) {
        QStringList row = rows.at(i);

        if (row.isEmpty()) {
            continue;
        } else {
            SampleSearch item;

            QString vendorPath = row.at(0);
            if (!QFileInfo::exists(vendorPath)) {
                qWarning() << "Cannot read" << vendorPath << " -- SKIPPING";
                continue;
            }
            item.sampleFilePath = row.at(0);

            if (row.size() > 1) {
                item.byonicFilePath = row.at(1);
            }

            result.push_back(item);
        }
    }

    return result;
}

int main(int argc, char *argv[]) {
    Err e = kNoErr;
    SettableFeatureFinderParameters ffUserParams;

    //// Parser Stuff /////////////////////////////////////////////////////////////////////////////////
    QCoreApplication app(argc, argv);
    ComInitializer comInitializer;
    QCommandLineParser parser;
    parser.addHelpOption();
    
    parser.addPositionalArgument(QLatin1String("inputFile"),
        QString("Input file containing MS data, supported by MSReader (.raw, .d etc.)"));
    
    parser.addPositionalArgument(QLatin1String("byrsltFile"),
        QString("Input Byonic file containing MSMS IDs (.byrslt)"));

    QCommandLineOption csvVendorFiles(QStringList() << "c" << "csv",
                                    "Turns on MultiSampleScanFeatureFinder and loads vendor files "
                                    "from CSV file\nThe csv file has one column 'Filepath'.");

    parser.addOption(csvVendorFiles);

    parser.process(app);
    
    const QStringList args = parser.positionalArguments();

    QString csvFilePath;
    if (parser.isSet(csvVendorFiles)) {
        csvFilePath = args.at(0);
    }

    if (!parser.isSet(csvVendorFiles) && args.count() != 2) {
        parser.showHelp(1);
    }
    
    QString filePath = args.at(0);
    QString byrsltFilePath = csvFilePath.isEmpty() ? args.at(1) : QString();

    if (!QFileInfo::exists(filePath)) {
        parser.showHelp(1);
    }

    // TODO: think if we want to keep the test with separate classes
    // or we want to test only through the wrapper MultiSampleScanFeatureFinder
    // I think Drew can find the old way still useful, so I'm not removing it yet
    bool testWrapper = parser.isSet(csvVendorFiles);
    if (testWrapper) {
        {
            QVector<SampleSearch> msFiles = loadFromCsvFile(csvFilePath);

// override the command line parameters with this project
#if 0
            QString vendor1 = R"(C:\PMI-Test-Data\Data\MS-Avas\MS\011315_DM_AvastinEu_IA_LysN.raw)";
            QString vendor2 = R"(C:\PMI-Test-Data\Data\MS-Avas\MS\011315_DM_AvastinUS_IA_LysN.raw)";
            QString vendor3 = R"(C:\PMI-Test-Data\Data\MS-Avas\MS\011315_DM_BioS1_IA_LysN.raw)";
            QString vendor4 = R"(C:\PMI-Test-Data\Data\MS-Avas\MS\011315_DM_BioS2_IA_LysN.raw)";

            QString byonic1 = R"(C:\PMI-Test-Data\Data\MS-Avas\Byonic\011315_DM_Av..._Byonic_150326_120916\011315_DM_Av..._Byonic_150326_120916.byrslt)";
            QString byonic2 = R"(C:\PMI-Test-Data\Data\MS-Avas\Byonic\011315_DM_Av..._Byonic_150326_121439\011315_DM_Av..._Byonic_150326_121439.byrslt)";
            QString byonic3 = R"(C:\PMI-Test-Data\Data\MS-Avas\Byonic\011315_DM_Bi..._Byonic_150326_121758\011315_DM_Bi..._Byonic_150326_121758.byrslt)";
            QString byonic4 = R"(C:\PMI-Test-Data\Data\MS-Avas\Byonic\011315_DM_Bi..._Byonic_150326_122051\011315_DM_Bi..._Byonic_150326_122051.byrslt)";

            msFiles = { {vendor1, byonic1 }, { vendor2, byonic2 }, { vendor3, byonic3 }, {vendor4, byonic4} };
#endif

            if (msFiles.isEmpty()) {
                qWarning() << "No samples provided! Finishing...";
                return TEST_RESULT_FAIL;
            }

            qDebug() << "Testing with" << msFiles.size() << "samples:";
            for (const auto &item : msFiles) {
                qDebug() << item.sampleFilePath;
            }

            QElapsedTimer et;
            et.start();
            int result = testMultiSampleScanFeatureFinder(msFiles);
            qDebug() << "testMultiSampleScanFeatureFinder done in" << et.elapsed() << "ms";

            return result;
        }
    }
	qDebug() << "Process peak memory" << pmi::MemoryInfo::peakProcessMemory();
    QElapsedTimer et;
    et.start();

    //Perhaps include these paths as arguments to be parsed from CLI?
    QString neuralNetworkDbFilePath = QDir(qApp->applicationDirPath()).filePath("nn_weights.db");


    //////////Begin Iterating MS1 Scans
    
    ScanIterator scanFlipper(neuralNetworkDbFilePath, ffUserParams);
    scanFlipper.init();
    QString output;
    //filePath = "Z:/Data/1448/reserves/UNK_MSMS.raw";
    //filePath = "Z:/ultramegatest.msfaux.byspec2";
    e = scanFlipper.iterateMSFileLinearDBSCANSelect(filePath, &output, false);
    if (e != kNoErr) {
        qDebug() << "iterateMSFileLinearDBSCANSelect failed with " << QString::fromStdString(pmi::convertErrToString(e));
        return TEST_RESULT_FAIL;
    }

    //////////Begin Matching Features to MSMS if byrslt filepath is provided.
    //PQMFeatureMatcher msmsToFeatureHookerUpper(filePath, byrsltFilePath);
    //e = msmsToFeatureHookerUpper.init();

    //// This must be run after all files have been processed by the feature finder so this will probs be moved elsewhere
    ////////////Begin Cross Sample Feature Collation
    //CrossSampleFeatureCollator crossSampleCollator(filePath, PPM, MAX_TIME_TOLERANCE_COARSE);
    //e = crossSampleCollator.init();
    //e = crossSampleCollator.run();
    //
    //// This must be run after all files have been processed by the feature finder so this will probs be moved elsewhere
    /////////////Generate Insilico List
    //InsilicoGenerator insilicoGenerator("C:/Users/drew/Desktop/chris/Comparitron.nslco"); //TODO replace the argument w/ crossSampleCollator.masterFeaturesDatabasePath()
    //e = insilicoGenerator.generateInsilicoFile();
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    
    qDebug() << "Seconds: " << et.elapsed();
    //////////////////////////////////////////////////////////////////////////////////////

    qDebug() << "Finish";
    //End Iterating MS1 Scans

    if (e != kNoErr) {
        qDebug() << "Finder failed" << QString::fromStdString(pmi::convertErrToString(e));
        return TEST_RESULT_FAIL;
    }

    return TEST_RESULT_SUCCESS;
}

////// Command Line Inputs
//  C:\db\Harmonized_Data\SPK_MSMS.raw C:\db\Harmonized_Data\SPK_MSMS.raw_20180717_Byonic\SPK_MSMS.raw_20180717_Byonic.byrslt
//  C:\db\Harmonized_Data\REF_MSMS.raw C:\db\Harmonized_Data\REF_MSMS.raw_20180717_Byonic\REF_MSMS.raw_20180717_Byonic.byrslt
//  C:\db\Harmonized_Data\UNK_MSMS.raw C:\db\Harmonized_Data\UNK_MSMS.raw_20180717_Byonic\UNK_MSMS.raw_20180717_Byonic.byrslt

//// Time warp challenged
//  C:\db\1448\REF_MSMS.raw C:\db\1448\ms2_search_1_col_0_byonic_ref_msms.byrslt
//  C:\db\1448\SPK_MSMS.raw C:\db\1448\ms2_search_3_col_0_byonic_spk_msms.byrslt
//  C:\db\1448\PH_MSMS.raw C:\db\1448\ms2_search_2_col_0_byonic_ph_msms.byrslt

//"D:\Dropbox (PMI)\PMI_Share_Data\Data2018\Ankur_Peptidoglycan\OT_180809_APatel_0p10ug_ETD100-HCD25_1.raw" C:\db\1448\ms2_search_1_col_0_byonic_ref_msms.byrslt


////NIST
//// "Z:\Data\Chris_MAM\despot\AD632_NIST-ref_RP12.raw" "Z:\Data\Chris_MAM\AD632_NIST-ref_RP12.raw_20180724_Byonic\AD632_NIST-ref_RP12.raw_20180724_Byonic.byrslt"
//// "C:\Users\drew\Desktop\AD632_NIST\AD632_NIST-spike_RP12.raw" "C:\Users\drew\Desktop\AD632_NIST\byos_tmp\AD632_NIST.SazRB\ms2_search_1_col_0_byonic_ad632_nist-spike_rp12_try.byrslt"
////  "Z:\Data\Chris_MAM\despot\AD632_NIST-ph-stress_RP12.raw" "Z:\Data\Chris_MAM\AD632_NIST-ph-stress_RP12.raw_20180724_Byonic\AD632_NIST-ph-stress_RP12.raw_20180724_Byonic.byrslt"
//// "C:\Users\drew\Desktop\chris\AD632_NIST-ph-stress_RP12.raw" "C:\Users\drew\Desktop\chris\AD632_NIST-ph-stress_RP12.raw_20180724_Byonic.byrslt"
