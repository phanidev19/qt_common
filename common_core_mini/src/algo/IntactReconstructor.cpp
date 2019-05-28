/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#include "IntactReconstructor.h"

//QT
#include <QFileInfo>

_PMI_BEGIN

const int ROUNDING_FACTOR = 1000;

IntactReconstructor::IntactReconstructor(
    const const std::vector<SampleProteinModPoint> &sampleProteinModPoints, int experiments)
    : m_sampleProteinModPoints(sampleProteinModPoints)
    , m_experiments(experiments)
{

}

IntactReconstructor::~IntactReconstructor() 
{
}

Err IntactReconstructor::run(const bool reconstructIntact)
{
    Err e = kNoErr;
    if (m_sampleProteinModPoints.empty()) {
        rrr(kError);
    }

#define  TROUBLESHOOTING_ENABLED
#ifdef TROUBLESHOOTING_ENABLED

    QList<DistributionInput> distributionInputs;
    DistributionInput d1( "A", 15.999, 0.5 );
    distributionInputs.push_back(d1);
    DistributionInput d2("B", 15.999, 0.5);
    distributionInputs.push_back(d2);


    if (reconstructIntact) {
        d1.id = "C";
        distributionInputs.push_back(d1);
        d2.id = "D";
        distributionInputs.push_back(d2);
    }

    BottomUpConvolution bottomUpConvolution;
    e = bottomUpConvolution.init(distributionInputs, m_experiments, ROUNDING_FACTOR);

    for (const double &k : bottomUpConvolution.computedDistributionsKeys()) {
        qDebug() << k << " " << bottomUpConvolution.returnFractionOfModificationMass(k);
        QMap<QStringList, double> x = bottomUpConvolution.returnFractionOfOriginsByKey(k);
        for (const QStringList &sub : x.keys()) {
            qDebug() << sub << " " << x[sub];
        }
        qDebug() << "*****";
    }
    

#else
    e = reconstructDistributionsPerSamplePerProtein(reconstructIntact); ree;
#endif

    return e;
}

Err IntactReconstructor::reconstructDistributionsPerSamplePerProtein(bool isIntact)
{
    Err e = kNoErr;
    if (m_sampleProteinModPoints.empty()) {
        qDebug() << "No sample protein modification points found";
        rrr(kError);
    }
    
    QVector<QString> sampleIDs;
    QVector<QString> proteinNames;
    for (const SampleProteinModPoint & pnt : m_sampleProteinModPoints) {
        if (!std::count(sampleIDs.begin(), sampleIDs.end(), pnt.samplesID)) {
            sampleIDs.push_back(pnt.samplesID);
        }
        if (!std::count(proteinNames.begin(), proteinNames.end(), pnt.proteinName)) {
            proteinNames.push_back(pnt.proteinName);
        }
    }
   
    for (const QString &samplesID : sampleIDs) {
        for (const QString &proteinName : proteinNames) {
            QList<DistributionInput> distributionInputs;
            for (const SampleProteinModPoint & pnt : m_sampleProteinModPoints) {
                if (!isIntact) {
                    if (pnt.proteinName == proteinName && pnt.samplesID == samplesID) {
                        distributionInputs.append(pnt.distributionInput);
                    }
                }
                else {
                    if (pnt.samplesID == samplesID) {
                        distributionInputs.append(DistributionInput(
                            pnt.proteinName + " " + pnt.distributionInput.id,
                            pnt.distributionInput.xValue, pnt.distributionInput.yFraction));

                        distributionInputs.append(DistributionInput(
                            "alt_" + pnt.proteinName + " " + pnt.distributionInput.id,
                            pnt.distributionInput.xValue, pnt.distributionInput.yFraction));
                    }
                }
            }
           
            
            ////This is a results viewer.  
            qDebug() << samplesID << " " << proteinName;
            BottomUpConvolution bottomUpConvolution;
            e = bottomUpConvolution.init(distributionInputs, m_experiments, ROUNDING_FACTOR);
            //QList<int> test = bottomUpConvolution.computedDistributionsKeys();
            for (const int &k : bottomUpConvolution.computedDistributionsKeys()) {
                qDebug() << k << " " << bottomUpConvolution.returnFractionOfModificationMass(k);
                QMap<QStringList, double> x = bottomUpConvolution.returnFractionOfOriginsByKey(k);
                for (const QStringList &sub : x.keys()) {
                    qDebug() << sub << " " << x[sub];
                }
            }
            qDebug() << "";

            //// This prevents the loop from running duplicates in the case of Intact Reconstruction.
            if (isIntact) {
                break;
            }
        }
    }

    return e;
}

_PMI_END


