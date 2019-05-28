/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#ifndef __MS_READER_OPTIONS_H__
#define __MS_READER_OPTIONS_H__

#include "MSReaderTypes.h"


_PMI_BEGIN

/*!
 * \brief The MSReaderOptions is basic holder of MSReader options.
 */
class PMI_COMMON_MS_EXPORT MSReaderOptions
{
public:
    /*!
    * \brief set IonMobility options (now used only in BrukerTIMS reader)
    */
    virtual void setIonMobilityOptions(const msreader::IonMobilityOptions &ionMobilityOptions);
    const msreader::IonMobilityOptions &ionMobilityOptions() const;

private:
    msreader::IonMobilityOptions m_ionMobilityOptions;
};

_PMI_END

#endif
