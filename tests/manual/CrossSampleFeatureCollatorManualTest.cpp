/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "CrossSampleFeatureCollatorTurbo.h"
//#include "pmi_common_ms_debug.h"

#include <ComInitializer.h>
#include <PmiMemoryInfo.h>

#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QString>

using namespace pmi;

const int TEST_RESULT_SUCCESS = 0;
const int TEST_RESULT_FAIL = 1;

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
    
    parser.process(app);   
    const QStringList args = parser.positionalArguments();
	
    QElapsedTimer et;
    et.start();

    QVector<SampleFeaturesTurbo> samples;

    SampleFeaturesTurbo sample1;
    sample1.sampleFilePath =
        "P:/PMI-Dev/Share/CrossSampleFeatureCollatorTest/011315_DM_AvastinEu_IA_LysN.raw";
    sample1.featuresFilePath =
        "P:/PMI-Dev/Share/CrossSampleFeatureCollatorTest/011315_DM_AvastinEu_IA_LysN.ftrs";
    samples.push_back(sample1);

    SampleFeaturesTurbo sample2;
    sample2.sampleFilePath =
        "P:/PMI-Dev/Share/CrossSampleFeatureCollatorTest/020215_DM_AvastinEU_trp_HCDETD.raw";
    sample2.featuresFilePath =
        "P:/PMI-Dev/Share/CrossSampleFeatureCollatorTest/020215_DM_AvastinEU_trp_HCDETD.ftrs";
    samples.push_back(sample2);

    SampleFeaturesTurbo sample3;
    sample3.sampleFilePath =
        "P:/PMI-Dev/Share/CrossSampleFeatureCollatorTest/020215_DM_BioS2_trp_HCDETD.raw";
    sample3.featuresFilePath =
        "P:/PMI-Dev/Share/CrossSampleFeatureCollatorTest/020215_DM_BioS2_trp_HCDETD.ftrs";
    samples.push_back(sample3);

    
    SettableFeatureFinderParameters params;
    CrossSampleFeatureCollatorTurbo crossSampleFeatureCollatorTurbo(samples, params);
    e = crossSampleFeatureCollatorTurbo.init(); ree;
    e = crossSampleFeatureCollatorTurbo.run();  ree;
    
    qDebug() << "Process peak memory" << pmi::MemoryInfo::peakProcessMemory() / 1000000.0 << "mB";
    qDebug() << "Seconds: " << et.elapsed() / 1000.0;
    qDebug() << "Finish";

    if (e != kNoErr) {
        qDebug() << "Finder failed" << QString::fromStdString(pmi::convertErrToString(e));
        return TEST_RESULT_FAIL;
    }

    return TEST_RESULT_SUCCESS;
}

