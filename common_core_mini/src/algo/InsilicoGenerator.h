/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#ifndef INSILICO_GENERATOR_H
#define INSILICO_GENERATOR_H

#include "pmi_common_core_mini_export.h"

#include <pmi_core_defs.h>
#include <common_errors.h>

_PMI_BEGIN

/*!
* @brief Transforms the features found by CrossSampleFeatureCollator to insilico peptide CSV file
*
* todo: Move this class out of common_core_mini to feature finder, it's too specific to be core mini
*/
class PMI_COMMON_CORE_MINI_EXPORT InsilicoGenerator
{
public:
    explicit InsilicoGenerator(const QString &masterFileDBPath);
    
    ~InsilicoGenerator();
    
    Err generateInsilicoFile(QString *outputFilePath);

    /*!
    * @brief Enables/disables matching to MS/MS byonic information
    * If enabled, sequence can be included in the final insilico CSV output.
    * Else the sequence is UNKNOWN_N where N is the autoincremented number
    */
    void setMS2MatchingEnabled(bool on);

private:
    void closeConnections();

private:
    QString m_masterFeaturesDBPath;
    bool m_enableMS2Matching;
    QStringList m_connectionNames;

};

_PMI_END

#endif //INSILICO_GENERATOR_H


