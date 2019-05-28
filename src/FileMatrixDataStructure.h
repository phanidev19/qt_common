/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
*/

#ifndef FILE_MATRIX_DATA_STRUCTURE_H
#define FILE_MATRIX_DATA_STRUCTURE_H

#include "pmi_common_ms_export.h"
#include "CommonFunctions.h"
#include "FauxScanCreator.h"
#include "FeatureFinderParameters.h"

#include <common_errors.h>
#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <QString>

_PMI_BEGIN


struct RetrieveSegement {
    Eigen::RowVectorXd xic;
    Eigen::RowVectorXd rtEigen;
    int startIndex;
    int endIndex;
};


/*!
* \brief Load an MS data file into an Eigen::SparseMatrix<double>, i.e. Uniform Grid for fast retreival.
*
*/
class PMI_COMMON_MS_EXPORT FileMatrixDataStructure{

public:
    /*!
    * \brief Used when loading data from an ms datafile.
    *
    *This is the general use constructor.
    */
    explicit FileMatrixDataStructure(const QString & msFilePath);

    /*!
    * \brief Used for testing w/ FauxSpectraGenerator Class
    *
    *This is used primarily for testing w/ FauxSpectraGenerator Class and
    * directly inputting the scans into Eigen::SparseMatrix<double>
    */
    explicit FileMatrixDataStructure(const QVector<FauxScanCreator::Scan> & scansVec);
    
    ~FileMatrixDataStructure();
    Err init();

    /*!
    * \brief Used to extract XICs from FileMatrixDataStructure Eigen::SparseMatrix<double>
    *
    *mz in Thompsons
    *errorRange is defined in Thompsons and is used to extract a range around mw;  i.e., extract mz = 1000 +/- 0.01 where errorRange = 0.01 
    * rtStart and rtEnd are in minutes.
    *
    *Since MatrixFileDataStructure columns are equal to scan number, retreiveIndexFromRTTime(double rt) is used to retrieve the scan
    * number from m_timeIndex where the rt of each scan is stored.
    */
    RetrieveSegement retrieveSegementFromDataFileMatrixErrorRange(double mz, double errorRange, double rtStart, double rtEnd);
    /*!
    * \brief extracts rt of scan number at input index
    */
    double retreiveRTFromTimeIndexByIndex(int index);
    /*!
    * \brief extracts scan number w/ closest rt to input rt.
    */
    int retreiveIndexFromRTTime(double rt);
    /*!
    * \retrieves scan index of data structure using vendor scan number
    */
    int retreiveScanIndexUsingVendorScanNumber(int vendorScanNumber);
    /*!
    * \retrieves scans using index and number of scans to retrieve.
    */
    Eigen::SparseVector<double> retreiveFullScansFromIndex(int indexStart, int numberOfScans);


    //// Getters
    Eigen::SparseMatrix<double> msDataFileInMatrix() const;
    point2dList TIC() const;
    point2dList basePeakChrom() const;
    QVector<double> timeIndex() const;


private:
    Err loadMSFileIntoFileMatrixDataStructure(Eigen::SparseMatrix<double, Eigen::ColMajor> *output);
    Err loadMSFileIntoFileMatrixDataStructureFromScanVec(
        const QVector<FauxScanCreator::Scan> &scansVec, Eigen::SparseMatrix<double, Eigen::ColMajor> *output);

private:
    QString m_msFilePath;
    point2dList m_TIC;
    point2dList m_basePeakChrom;
    QVector<double> m_timeIndex;
    QVector<int> m_vendorScanNumber;
    Eigen::VectorXd m_timeIndexEigen;
    Eigen::SparseMatrix<double, Eigen::ColMajor> m_msDataFileInMatrix;
    ImmutableFeatureFinderParameters m_ffParameters;
    int m_vectorGranularity = 10000;
};
_PMI_END

#endif //FILE_MATRIX_DATA_STRUCTURE_H