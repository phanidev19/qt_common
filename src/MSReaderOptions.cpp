/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Dmitry Pischenko dmitry.pischenko@toplab.io
 */

#include "MSReaderOptions.h"

_PMI_BEGIN

void MSReaderOptions::setIonMobilityOptions(const msreader::IonMobilityOptions &ionMobilityOptions)
{
    m_ionMobilityOptions = ionMobilityOptions;
}

const msreader::IonMobilityOptions &MSReaderOptions::ionMobilityOptions() const
{
    return m_ionMobilityOptions;
}

_PMI_END
