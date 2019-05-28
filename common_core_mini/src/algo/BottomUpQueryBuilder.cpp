/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpQueryBuilder.h"
#include "BottomUpConvolution.h"

_PMI_BEGIN

BottomUpQueryBuilder::BottomUpQueryBuilder()
{
}

BottomUpQueryBuilder::~BottomUpQueryBuilder()
{
}

Err BottomUpQueryBuilder::init(const BottomUpToIntactDataReader &bottomUpToIntactDataReader)
{
    Err e = kNoErr;

    if (!bottomUpToIntactDataReader.proteinIdsIsValid()) {
        warningCoreMini() << "bottomUpReaderData is empty";
        rrr(kError);
    }
    m_bottomUpToIntactDataReader = bottomUpToIntactDataReader;
    return e;
}

//// TODO: Replace this w/ solution Yong was talking about.
//// NOTE: yes, this is not supposed to be const.
inline double extractModValueFromString(QString &modString)
{
    double modValue = 0;
    bool ok = true;
    ////TODO method to extract double from string is not elegent or robust.  Consider regex.
    modString = modString.split('/').back().replace(")", "");
    if (modString.isEmpty()) {
        return 0.0;
    }
    modValue = modString.toDouble(&ok);
    Q_ASSERT(ok);
    return modValue;
}

Err BottomUpQueryBuilder::runConvolutionQuery()
{
    Err e = kNoErr;

    if (m_proteinQuery.proteinIDCount.isEmpty()) {
        criticalCoreMini() << "query is not set!!!!";
        rrr(kBadParameterError);
    }

    QVector<int> proteinIDs;
    for (auto it = m_proteinQuery.proteinIDCount.begin();
         it != m_proteinQuery.proteinIDCount.end(); ++it) {
        proteinIDs.push_back(it.key());
    }

    QVector<BottomUpToIntactDataReader::ProteinModPoint> queriedModificaitonRepositoryRows;
    e = m_bottomUpToIntactDataReader.selectRowsFromModificationsRepository(
        proteinIDs, &queriedModificaitonRepositoryRows); ree;

    QList<BottomUpConvolution::DistributionInput> distributionInputs;
    for (const BottomUpToIntactDataReader::ProteinModPoint &row :
         queriedModificaitonRepositoryRows) {

        int proteinRepeatCount = m_proteinQuery.proteinIDCount.value(row.proteinID);
        for (int i = 0; i < proteinRepeatCount; ++i) {

            const BottomUpConvolution::DegradationID degradationID(row.position, row.proteinID, i);
            QString modification = row.modification;
            double xValueMod = extractModValueFromString(modification);
            BottomUpConvolution::DistributionInput distributionInput(degradationID, xValueMod,
                                                                     row.modIntensityPercentage);
            distributionInputs.push_back(distributionInput);
        }
    }

    BottomUpConvolution bottomUpConvolution;
    const double pruning = 1e-5; //// Set to this value since values below this will not contribute
                           ///significantly to distribution.
    e = bottomUpConvolution.init(distributionInputs, pruning); ree;

    m_bottomUpConvolutionResult = bottomUpConvolution;

    return e;
}

Err BottomUpQueryBuilder::setProteinQuery(const ProteinQuery &proteinQuery)
{
    Err e = kNoErr;

    //// CHecks if both values are set
    if (proteinQuery.proteinIDCount.isEmpty()) {
        warningCoreMini() << "sampleProteinQuery values are not all set.";
        rrr(kError);
    }

    ///// If both values are set, makes sure that proteinID is in range.
    QHash<BottomUpToIntactDataReader::ProteinId, int> proteinIDCount = proteinQuery.proteinIDCount;
    for (auto it = proteinIDCount.begin(); it != proteinIDCount.end(); ++it) {
        if (!m_bottomUpToIntactDataReader.proteinIdsContainsProteinId(it.key())) {
            criticalCoreMini() << "proteinID value is out of range.";
            rrr(kError);
        }
    }

    m_proteinQuery = proteinQuery;
    return e;
}

BottomUpQueryBuilder::ProteinQuery BottomUpQueryBuilder::proteinQuery() const
{
    return m_proteinQuery;
}

QVector<BottomUpConvolution::DeltaMassStick> BottomUpQueryBuilder::getDeltaMassSticks() const
{
    return m_bottomUpConvolutionResult.deltaMassSticks();
}

void BottomUpQueryBuilder::degradationConstituentsReader(
    const BottomUpConvolution::DegradationConstituents &degradationConstituent)
{
    ////// iterates overall composition of the mod mass
    debugCoreMini() << "Constituent Percent Contribution"
        << degradationConstituent.percentContribution;
    for (const BottomUpConvolution::DistributionInput &dist :
        degradationConstituent.distributionInputs) {

        
        if (!m_bottomUpToIntactDataReader.proteinIdsContainsProteinId(dist.degradationID.proteinID)) {
            warningCoreMini() << "key not found in proteinIDs inverted";
            continue; //TODO: consider whether this should throw error instead or return null value
        }

        BottomUpToIntactDataReader::ProteinName proteinName;
        m_bottomUpToIntactDataReader.proteinNameFromProteinIds(
            dist.degradationID.proteinID
            , &proteinName);

        debugCoreMini() << proteinName << "Chain"
            << dist.degradationID.chainCount << "Position"
            << dist.degradationID.position << "Mod Val" << dist.xValue;
    }
    debugCoreMini() << " ";
}

_PMI_END
