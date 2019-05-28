/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "BottomUpToIntactDataReader.h"

// common_stable
#include "CsvReader.h"

_PMI_BEGIN

BottomUpToIntactDataReader::BottomUpToIntactDataReader()
{
}

BottomUpToIntactDataReader::~BottomUpToIntactDataReader()
{
}

//TODO (Drew Nichols 2019-04-11)  These are duplicated everywhere.  Consolidate
inline double extractModValueFromString(QString & modString) {
    double modValue = 0;
    bool ok = true;
    ////TODO method to extract double from string is not elegent or robust.  Consider regex.
    modString = modString.split('/').back().replace(")", "");
    modValue = modString.toDouble(&ok);
    Q_ASSERT(ok);
    return modValue;
}

Err BottomUpToIntactDataReader::readCSVIntoModificationsRepository(const QString &csvFilePath)

{
    Err e = kNoErr;

    e = checkIfProteinIdsSet(); ree;

    CsvReader reader(csvFilePath);
    reader.setQuotationCharacter('"');
    reader.setFieldSeparatorChar(',');

    if (!reader.open()) {
        criticalCoreMini() << QString("Failed to Open Report CSV File %1").arg(csvFilePath);
        rrr(kError);
    }

    int rowSize = -1;

    const QString kProtein = "Protein";
    const QString kPosition = "Position";
    const QString kMod = "Mod";

    enum CriticalColumnsOfCSVInput { 
        Protein = 0
        , Position
        , Mod
        , FirstSampleColumn
        , TotalLength 
    };

    if (reader.hasMoreRows()) {
        QStringList row = reader.readRow();

        if (row.size() < TotalLength) {
            warningCoreMini() << "CSV must have at least one sample";
            rrr(kError);
        }

        if (row[Protein] != kProtein 
            || row[Position] != kPosition 
            || row[Mod] != kMod)
        {
            criticalCoreMini() << "Header must have format:  Protein, Position, Mod, "
                                  "SampleName_1,....., SampleName_N ";
            warningCoreMini() << "Samples must be in Percentages";
            rrr(kError);
        } else {
            rowSize = static_cast<int>(row.size());
        }
    } else {
        criticalCoreMini() << "CSV file is empty";
        rrr(kError);
    }

    //TODO: for now, we only accept one column as sample. Later, we can extend this (with proper refactoring)
    // to handle multiple samples.
    // The input csv and the code structure used to couple sample and protein into a single row. When we refactor,
    // we want to decouple the two.
    /*
     *  Dummy example (names are not specific to this file/class/methods).
        Before:
        struct SampleProtein {
        sample id;
        protein stuff;
        }

        QVector<SampleProtein> coupledList;

        Current:
        struct ProteinOnly {
         protein stuff;
        };


        Desired (for later):
        struct SampleWithProteins {
            QVector<ProteinOnly> decoupledList;
            QVector<int> samplesId; //Or something better to link the two; maybe QHash<int, ProteinOnly>
        }
    */

    if (rowSize != TotalLength ) {
        warningCoreMini() << "CSV must have exactly one sample column defined";
        rrr(kError);
    }

    while (reader.hasMoreRows()) {

        QStringList row = reader.readRow();
        //// rowSize is guaranteed to be greater than 3 as defined by enum and range check above.
        if (row.size() < rowSize) {
            continue;
        }

        bool ok = true;
        QString proteinName = row.at(Protein);
        QString position = row.at(Position);
        QString modification = row.at(Mod);

        QStringList modificationSplit = modification.split(";");
        QStringList positionSplit = position.split(",");
        if (modificationSplit.size() != positionSplit.size()) {
            rrr(kError);
        }
        
        for (int i = 0; i < positionSplit.size(); ++i) {
            position = positionSplit[i];
            modification = modificationSplit[i];

            for (int j = FirstSampleColumn; j < rowSize; ++j) {

                QString valString = row.at(j).isEmpty() ? "0" : row.at(j);
                double relIntensity = valString.toDouble(&ok);
                Q_ASSERT(ok);
                relIntensity /= 100.0; ////Divide by 100 to turn into decimal from Percent

                ProteinModPoint proteinModPoint;
                ProteinId proteinId;
                e = proteinIdFromProteinIds(proteinName, &proteinId); ree;

                proteinModPoint.proteinID = proteinId;
                proteinModPoint.position = position;
                proteinModPoint.modification = modification;
                proteinModPoint.modIntensityPercentage = relIntensity;

                ////This is not supposed to be const because it is being modfied in the loop.
                bool proteinModPointFound = false;
                for (ProteinModPoint &currentProteinModPoint : m_modificationsRepository) {
                    if (proteinModPoint.proteinID == currentProteinModPoint.proteinID
                        && proteinModPoint.position == currentProteinModPoint.position
                        && proteinModPoint.modification == currentProteinModPoint.modification) {
                        currentProteinModPoint.modIntensityPercentage += proteinModPoint.modIntensityPercentage;
                        proteinModPointFound = true;
                        break;
                    }
                }
                if (!proteinModPointFound) {
                    m_modificationsRepository.push_back(proteinModPoint);
                }          
            }
        }
    }

    return e;
}

Err BottomUpToIntactDataReader::selectRowsFromModificationsRepository(QVector<int> proteinIDs,
    QVector<ProteinModPoint> *queriedModificaitonRepositoryRows)
{
    Err e = kNoErr;

    e = checkIfProteinIdsSet(); ree;

    QVector<ProteinModPoint> modificationRowsQuerySet;

    for (const ProteinModPoint &proteinModPoint : m_modificationsRepository) {
        if (proteinIDs.contains(proteinModPoint.proteinID)) {
            modificationRowsQuerySet.push_back(proteinModPoint);
        }
    }

    //// TODO:  Think about whether you want to return an error if query returns null.
    //// this might not be desired behavior.
    if (modificationRowsQuerySet.isEmpty()) {
        rrr(kError);
    }

    *queriedModificaitonRepositoryRows = modificationRowsQuerySet;
    return e;
}



Err BottomUpToIntactDataReader::proteinNameFromProteinIds(
    ProteinId proteinId
    , ProteinName *proteinName) const
{
    Err e = kNoErr;

    e = checkIfProteinIdsSet(); ree;

    if (!proteinIdsContainsProteinId(proteinId)) {
        warningCoreMini() << "ProteinId not found in proteinIds";
        rrr(kError);
    }

    *proteinName = m_proteinIDs.value(proteinId);

    return e;
}

Err BottomUpToIntactDataReader::proteinIdFromProteinIds(
    ProteinName proteinName
    , ProteinId *proteinId) const
{
    Err e = kNoErr;

    e = checkIfProteinIdsSet(); ree;

    if (!proteinIdsContainsProteinName(proteinName)) {
        warningCoreMini() << QString("ProteinName %1 not found in proteinIds").arg(proteinName);
        rrr(kError);
    }

    *proteinId = m_proteinIDs.key(proteinName);

    return e;
}

bool BottomUpToIntactDataReader::proteinIdsContainsProteinId(ProteinId proteinId) const
{
    return m_proteinIDs.keys().contains(proteinId);
}

bool BottomUpToIntactDataReader::proteinIdsContainsProteinName(ProteinName proteinName) const
{
    return m_proteinIDs.values().contains(proteinName);
}

bool BottomUpToIntactDataReader::proteinIdsIsValid() const
{
    return m_proteinIDs.empty() ? false : true;
}

int BottomUpToIntactDataReader::modificationsRepositorySize() const
{
    return m_modificationsRepository.size();
}

Err BottomUpToIntactDataReader::setModificationsRepository(
    const QVector<ProteinModPoint> &modificationsRepository)
{
    Err e = kNoErr;

    if (modificationsRepository.isEmpty()) {
        warningCoreMini() << "Input is empty!!!!";
    }

    m_modificationsRepository = modificationsRepository;

    return e;
}

Err BottomUpToIntactDataReader::setProteinIds(const QHash<ProteinId, QString>& proteinIds)
{
    Err e = kNoErr;

    if (proteinIds.isEmpty()) {
        warningCoreMini() << "Input is empty!!!!";
    }

    m_proteinIDs = proteinIds;

    return e;
}

Err BottomUpToIntactDataReader::getProteinModPointByIndex(
    int index
    , ProteinModPoint *proteinModPoint) const
{
    Err e = kNoErr;

    e = checkIfProteinIdsSet(); ree;

    if (index < 0 || index > m_modificationsRepository.size() - 1 ) {
        criticalCoreMini() << "Failed to retrieve ProteinModPoint";
        rrr(kError);
    }

    *proteinModPoint = m_modificationsRepository[index];
    return e;
}

Err BottomUpToIntactDataReader::checkIfProteinIdsSet() const
{
    Err e = kNoErr;

    if (!proteinIdsIsValid()) {
        criticalCoreMini() << "ProteinIds is empty";
        rrr(kError);
    }

    return e;
}


_PMI_END
