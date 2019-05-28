/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpQueryBuilder.h"
#include "BottomUpToIntactDataReader.h"

#include <QCommandLineParser>
#include <QElapsedTimer>

using namespace pmi;


int main(int argc, char *argv[])
{
    Err e = kNoErr;
    QCoreApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();

    parser.addPositionalArgument(QLatin1String("inputBottomUpCSVFilePath"),
        QString("Input file containing Degradations in csv format"));

    parser.process(app);
    const QStringList args = parser.positionalArguments();

    QElapsedTimer et;
    et.start();

    BottomUpToIntactDataReader bottomUpData;

    QHash<BottomUpToIntactDataReader::ProteinId, BottomUpToIntactDataReader::ProteinName> proteinIds;
    proteinIds.insert(0, "HC");
    proteinIds.insert(1, "LC");

    bottomUpData.setProteinIds(proteinIds);
    e = bottomUpData.readCSVIntoModificationsRepository(args.at(0)); ree;

   BottomUpQueryBuilder bottomUpQueryBuilder;
    e = bottomUpQueryBuilder.init(bottomUpData); ree;
   
    //// Build the query here.
    BottomUpToIntactDataReader::ProteinId proteinID = 1; 
    int proteinCount = 1;
    QHash<BottomUpToIntactDataReader::ProteinId, int> proteinIDCount;
    proteinIDCount.insert(proteinID, proteinCount);

    BottomUpQueryBuilder::ProteinQuery query(proteinIDCount);
    
    //// Set the query and run.
    e = bottomUpQueryBuilder.setProteinQuery(query); ree;
    e = bottomUpQueryBuilder.runConvolutionQuery(); ree;

    //// Retrieve the Sticks.
    const QVector<BottomUpConvolution::DeltaMassStick> & deltaMassSticks
        = bottomUpQueryBuilder.getDeltaMassSticks();

    //// DEMO
    for (const BottomUpConvolution::DeltaMassStick & deltaMassStick : deltaMassSticks) {

        if (deltaMassStick.intensityFraction < 0.01) {
            continue;
        }

        qDebug() << "Delta Mass" << deltaMassStick.deltaMass 
                         << "Intensity" << deltaMassStick.intensityFraction;
        qDebug() << "";

        for (const BottomUpConvolution::DegradationConstituents & constituent : deltaMassStick.degradationConstituents) {
            bottomUpQueryBuilder.degradationConstituentsReader(constituent);
        }
        qDebug() << "*****************************";
    }

    Q_ASSERT(e == kNoErr);

    qDebug() << "Seconds: " << et.elapsed();
    qDebug() << "Finished";
}