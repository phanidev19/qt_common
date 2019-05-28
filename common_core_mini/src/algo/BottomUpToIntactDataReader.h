/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */ 

#ifndef BOTTOM_UP_TO_INTACT_DATA_READER
#define BOTTOM_UP_TO_INTACT_DATA_READER

// common_stable
#include "BottomUpConvolution.h"
#include "pmi_common_core_mini_debug.h"
#include "pmi_common_core_mini_export.h"
#include <common_errors.h>
#include <pmi_core_defs.h>

_PMI_BEGIN

/*!
 * @brief Reads csv of bottom up modifications in a particular format for use in BottomUpConvolution
 * Class
 *
 * Header of csv file must have format:  Protein, Position, Mod, SampleName_1, ..... , SampleName_N
 * " Samples must be in Percentages and not decimal representations.
 */
class PMI_COMMON_CORE_MINI_EXPORT BottomUpToIntactDataReader
{

public:
    typedef int ProteinId;
    typedef QString ProteinName;

    /*!
     * @brief table structure for input data to go into m_modificationsRepository
     */
    struct ProteinModPoint {
        ProteinId proteinID;
        QString position;
        QString modification;
        double modIntensityPercentage = 0;

        ProteinModPoint() {}

        ProteinModPoint(
            ProteinId proteinID
            , const QString &position
            , const QString &modification
            , double modIntensityPercentage
        )
            : proteinID(proteinID)
            , position(position)
            , modification(modification)
            , modIntensityPercentage(modIntensityPercentage)
        {
        }
    };

    BottomUpToIntactDataReader();

    ~BottomUpToIntactDataReader();

    /*!
     * @brief Reads the csv into the data m_modificationsRepository.
     */
    Err readCSVIntoModificationsRepository(const QString &csvFilePath);

    /*!
     * @brief Returns queried rows from m_modificationsRepository
     */
    Err selectRowsFromModificationsRepository(
        QVector<ProteinId> proteinIDs
        , QVector<ProteinModPoint> *queriedModificaitonRepositoryRows);

    /*!
    * @brief returns proteinName given a proteinId
    */
    Err proteinNameFromProteinIds(ProteinId proteinId, ProteinName *proteinName) const;

    /*!
    * @brief Retrieves proteinId given a proteinName
    */
    Err proteinIdFromProteinIds(ProteinName proteinName, ProteinId *proteinId) const;

    /*!
    * @brief Checks if a proteinId exists in m_proteinIds keys
    */
    bool proteinIdsContainsProteinId(ProteinId proteinId) const;

    /*!
    * @brief Checks to see if a proteinName exists in m_proteinIds values
    */
    bool proteinIdsContainsProteinName(ProteinName proteinName) const;

    /*!
    * @brief Checks to see if m_proteinIds is populated
    */
    bool proteinIdsIsValid() const;

    /*!
    * @brief Returns the size of modRepository for use in loops.
    */
    int modificationsRepositorySize() const;

    Err setModificationsRepository(const QVector<ProteinModPoint> &modificationsRepository);
    Err setProteinIds(const QHash<ProteinId, ProteinName> &proteinIds);
    Err getProteinModPointByIndex(int index, ProteinModPoint *proteinModPoint) const;

    Err checkIfProteinIdsSet() const;

private:
    QHash<ProteinId, ProteinName> m_proteinIDs;
    QVector<ProteinModPoint> m_modificationsRepository;
};

_PMI_END

#endif // BOTTOM_UP_TO_INTACT_DATA_READER
