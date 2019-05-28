/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */

#include "ComparisonStructs.h"

/*
** Author's note:
**     All of the following hash functions are used for generating QSets of the structures.
**     !!DO NOT MODIFY!!
*/

uint qHash(const ComparisonStructs::MetaInfoForScanInBothFiles &toHash)
{
    return qt_hash(toHash.nativeId);
}

uint qHash(const ComparisonStructs::DiffInfoForOneScan &toHash)
{
    return qt_hash(toHash.nativeId);
}

uint qHash(const ComparisonStructs::MetaInfoForScanInOneFile &toHash)
{
    return qt_hash(toHash.scanInfo.nativeId);
}