/*
 * Copyright (C) 2017-2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author  : Sam Bogar (sbogar@proteinmetrics.com)
 */
#ifndef MSCOMPARISONMANAGER_H
#define MSCOMPARISONMANAGER_H

#include "CsvWriter.h"
#include "MSReader.h"
#include "ComparisonStructs.h"

/*!
 * \brief Have meta information about both of the files and provide the right mapping between
 * nativeId and scanNumber Additionally provide tools for comparing the files identified by the
 * meta-information
 */

class MSComparisonManager
{
public:
    MSComparisonManager() {};

    ~MSComparisonManager() {};

    static bool compareMetaInfo(const ComparisonStructs::MetaInfoForScanInOneFile &a,
                                const ComparisonStructs::MetaInfoForScanInOneFile &b, ComparisonStructs::MetaInfoCompare *result);
    ComparisonStructs::DiffInfo compare2dPointData(const QSet<ComparisonStructs::MetaInfoForScanInBothFiles> &overlap,
                                const ComparisonStructs::UserInput &userInput);
    pmi::Err readFromFiles(const QString &firstFile, const QString &secondFile);
    pmi::Err findSharedScans(QSet<ComparisonStructs::MetaInfoForScanInBothFiles> *scansPresentInBothFiles,
                        QSet<ComparisonStructs::MetaInfoForScanInOneFile> *scansPresentOnlyInFirstFile,
                        QSet<ComparisonStructs::MetaInfoForScanInOneFile> *scansPresentOnlyInSecondFile);


private:
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> m_firstFileScans;
    QSet<ComparisonStructs::MetaInfoForScanInOneFile> m_secondFileScans;

    static long compareOneScan2dPointDataProfile(const pmi::point2dList &firstPoints,
                                                 const pmi::point2dList &secondPoints,
                                                 const ComparisonStructs::UserInput &userInput,
                                                 const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo,
                                                 pmi::CsvWriter *csvWriter, ComparisonStructs::DiffInfo *diffInfo);
    static long compareOneScan2dPointDataCentroid(const pmi::point2dList &firstPoints,
                                                  const pmi::point2dList &secondPoints,
                                                  const ComparisonStructs::UserInput &userInput,
                                                  const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo,
                                                  pmi::CsvWriter *csvWriter, ComparisonStructs::DiffInfo *diffInfo);
    static pmi::point2dList compareCentroided2dPointsByIndex(
        const pmi::point2dList &firstPoints, const pmi::point2dList &secondPoints, const ComparisonStructs::UserInput &userInput,
        pmi::CsvWriter *csvWriter, const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo, ComparisonStructs::DiffInfo *diffInfo);
    static pmi::point2dList compareCentroided2dPointsByCoordinate(
        const pmi::point2dList &firstPoints, const pmi::point2dList &secondPoints, const ComparisonStructs::UserInput &userInput,
        pmi::CsvWriter *csvWriter, const ComparisonStructs::MetaInfoForScanInBothFiles &metaInfo);

    pmi::Err readFromOneMSReader(pmi::MSReader *msReader, bool isA);
};

#endif // MSCOMPARISONOMANAGER_H
