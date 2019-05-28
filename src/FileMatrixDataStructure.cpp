/*
 * Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#include "FileMatrixDataStructure.h"

#include <MSReader.h>
#include <PmiMemoryInfo.h>

_PMI_BEGIN

FileMatrixDataStructure::FileMatrixDataStructure(const QString &msFilePath)
    : m_msFilePath(msFilePath)
{
}

FileMatrixDataStructure::FileMatrixDataStructure(const QVector<FauxScanCreator::Scan>& scansVec)
{
    Err e = kNoErr;
    e = loadMSFileIntoFileMatrixDataStructureFromScanVec(scansVec, &m_msDataFileInMatrix);
    if (e != kNoErr) {
        qDebug() << "File did not load into FileMatrixDataStructure";
    }
}

FileMatrixDataStructure::~FileMatrixDataStructure()
{
}

inline double calculatePPMTolerance(const double &mw, const int &ppm)
{
    return (mw * ppm) / 1000000;
}

Err FileMatrixDataStructure::init()
{
    Err e = kNoErr;
    e = loadMSFileIntoFileMatrixDataStructure(&m_msDataFileInMatrix);
    return e;
}

RetrieveSegement FileMatrixDataStructure::retrieveSegementFromDataFileMatrixErrorRange(double mz, double errorRange,
    double rtStart,
    double rtEnd)
{
    int mzHashed = FeatureFinderUtils::hashMz(mz, m_vectorGranularity);
    int errorRangeToleranceHashed = FeatureFinderUtils::hashMz(errorRange, m_vectorGranularity);
    int rtStartHashed = retreiveIndexFromRTTime(rtStart);
    int rtEndHashed = retreiveIndexFromRTTime(rtEnd) + 1; //1 is added to include last point.

    Eigen::SparseMatrix<double, Eigen::ColMajor> sliceDataFileMatrix
        = m_msDataFileInMatrix.middleRows(mzHashed - errorRangeToleranceHashed, (errorRangeToleranceHashed * 2) + 1)
        .middleCols(rtStartHashed, (rtEndHashed - rtStartHashed) );

    Eigen::VectorXd XIC = sliceDataFileMatrix.transpose() * Eigen::VectorXd::Ones(sliceDataFileMatrix.rows());

    RetrieveSegement retrieveSegment;
    retrieveSegment.xic = XIC;
    retrieveSegment.startIndex = rtStartHashed;
    retrieveSegment.endIndex = rtEndHashed;
    retrieveSegment.rtEigen = m_timeIndexEigen.middleRows(rtStartHashed, (rtEndHashed - rtStartHashed));
    return retrieveSegment;
}

int FileMatrixDataStructure::retreiveScanIndexUsingVendorScanNumber(int vendorScanNumber)
{
    ptrdiff_t pos = std::find(m_vendorScanNumber.begin(), m_vendorScanNumber.end(), vendorScanNumber) - m_vendorScanNumber.begin();
    return pos >= m_vendorScanNumber.size() ? -1 : pos;
}

Eigen::SparseVector<double> FileMatrixDataStructure::retreiveFullScansFromIndex(int indexStart, int numberOfScans)
{
    indexStart = indexStart < 0 ? 0 : indexStart;
    numberOfScans = numberOfScans <= 0 ? 1 : numberOfScans;

    Eigen::SparseVector<double> sliceDataFileMatrix(m_msDataFileInMatrix.rows());
    sliceDataFileMatrix.setZero();
    for (int i = 0; i < numberOfScans; ++i) {
        if (indexStart + i < m_msDataFileInMatrix.cols()) {
            sliceDataFileMatrix += m_msDataFileInMatrix.col(indexStart + i);
        }
    }

    sliceDataFileMatrix.prune(0.0);
    return sliceDataFileMatrix / numberOfScans;
}

Eigen::SparseMatrix<double> FileMatrixDataStructure::msDataFileInMatrix() const
{
    return m_msDataFileInMatrix;
}

point2dList FileMatrixDataStructure::TIC() const
{
    return m_TIC;
}

point2dList FileMatrixDataStructure::basePeakChrom() const
{
    return m_basePeakChrom;
}

QVector<double> FileMatrixDataStructure::timeIndex() const
{
    return m_timeIndex;
}

double FileMatrixDataStructure::retreiveRTFromTimeIndexByIndex(int index)
{
    return m_timeIndex[index];
}

int FileMatrixDataStructure::retreiveIndexFromRTTime(double rt)
{
    Eigen::RowVectorXd diff = m_timeIndexEigen.array() - rt;
    diff = diff.cwiseAbs();
    double minimumDiff = diff.minCoeff();
    std::vector<double> diffVec(diff.data(), diff.data() + diff.size());
    ptrdiff_t pos = std::find(diffVec.begin(), diffVec.end(), minimumDiff) - diffVec.begin();
    return pos >= static_cast<int>(diffVec.size()) ? -1 : pos;
}

Err FileMatrixDataStructure::loadMSFileIntoFileMatrixDataStructure(Eigen::SparseMatrix<double, Eigen::ColMajor>*output) 
{
	Err e = kNoErr;

	const bool doCentroid = true;

	MSReader *ms = MSReader::Instance();
	e = ms->openFile(m_msFilePath); ree;
	QList<msreader::ScanInfoWrapper> scanInfoList;
	e = ms->getScanInfoListAtLevel(1, &scanInfoList); ree;

	int buffer = 10;
	Eigen::SparseMatrix<double, Eigen::ColMajor> buildingDataFileMatrix(static_cast<int>((m_ffParameters.mzMax + buffer) * m_vectorGranularity), scanInfoList.size());
	buildingDataFileMatrix.reserve(Eigen::VectorXi::Constant(scanInfoList.size(), m_ffParameters.maxIonCount ));

	//// Creates both point2DList TIC and Eigen::SparseMatrix<int> m_msDataFileInMatrix
	for (int i = 0; i < scanInfoList.size(); ++i) {

		//const MSReaderBase::ScanInfoLockmass &scanInfo = scanInfoList[i];
		const msreader::ScanInfoWrapper &scanInfo = scanInfoList[i];
		m_timeIndex.push_back(scanInfo.scanInfo.retTimeMinutes);
        m_vendorScanNumber.push_back(scanInfo.scanNumber);
        //qDebug() << scanInfo.scanNumber;

		point2dList points;
		double scanTIC = 0;
        double scanBasePeak = 0;
		e = ms->getScanData(scanInfo.scanNumber, &points, doCentroid); ree;

		int pointsSize = static_cast<int>(points.size());
		if (pointsSize > m_ffParameters.maxIonCount) {
			std::sort(points.begin(), points.end(), 
				[](const point2d &left, const point2d &right) {
				return (left.y() > right.y());
			});
			pointsSize = m_ffParameters.maxIonCount;
		}

        /////TODO implement constructing BPI here as well.
		for (size_t j = 0; j < pointsSize; ++j) {
			int mzInsertionPoint = FeatureFinderUtils::hashMz(points[j].x(), m_vectorGranularity);
			if (mzInsertionPoint < static_cast<int>(m_ffParameters.mzMax * m_vectorGranularity)) {
				buildingDataFileMatrix.coeffRef(mzInsertionPoint, i) = static_cast<double>(points[j].y());
                scanBasePeak = points[j].y() > scanBasePeak ? points[j].y() : scanBasePeak;
				scanTIC += points[j].y();
			}
		}

		point2d tempPoint;
		tempPoint.setX(scanInfo.scanInfo.retTimeMinutes);
		tempPoint.setY(scanTIC);
		m_TIC.push_back(tempPoint);

        point2d tempPointBP;
        tempPointBP.setX(scanInfo.scanInfo.retTimeMinutes);
        tempPointBP.setY(scanBasePeak);
        m_basePeakChrom.push_back(tempPointBP);
	}

	Eigen::VectorXd timeIndexEigen(m_timeIndex.size());
	for (int i = 0; i < m_timeIndex.size(); ++i) {
        timeIndexEigen(i) = m_timeIndex[i];
	}

	m_timeIndexEigen = timeIndexEigen;
	buildingDataFileMatrix.makeCompressed();
	*output = buildingDataFileMatrix;

    if (buildingDataFileMatrix.nonZeros() == 0) {
        rrr(kError);
    }

    ms->closeFile();
	return e;
}

Err FileMatrixDataStructure::loadMSFileIntoFileMatrixDataStructureFromScanVec(
    const QVector<FauxScanCreator::Scan> &scansVec,
    Eigen::SparseMatrix<double, Eigen::ColMajor> *output)
{
    Err e = kNoErr;
    int buffer = 10;
    Eigen::SparseMatrix<double, Eigen::ColMajor> buildingDataFileMatrix(static_cast<int>((m_ffParameters.mzMax + buffer) * m_vectorGranularity), scansVec.size());
    buildingDataFileMatrix.reserve(Eigen::VectorXi::Constant(scansVec.size(), m_ffParameters.maxIonCount));

    for (int i = 0; i < scansVec.size(); ++i) {

        //const MSReaderBase::ScanInfoLockmass &scanInfo = scanInfoList[i];
        const FauxScanCreator::Scan &scanInfo = scansVec[i];
        m_timeIndex.push_back(scanInfo.retTimeMinutes);

        point2dList points = scanInfo.scanIons;
        double scanTIC = 0;
        double scanBasePeak = 0;
        
        int pointsSize = static_cast<int>(points.size());
        if (pointsSize > m_ffParameters.maxIonCount) {
            std::sort(points.begin(), points.end(),
                [](const point2d &left, const point2d &right) {
                return (left.y() > right.y());
            });
            pointsSize = m_ffParameters.maxIonCount;
        }

        for (size_t j = 0; j < pointsSize; ++j) {
            int mzInsertionPoint = FeatureFinderUtils::hashMz(points[j].x(), m_vectorGranularity);
            if (mzInsertionPoint < static_cast<int>(m_ffParameters.mzMax * m_vectorGranularity)) {
                buildingDataFileMatrix.coeffRef(mzInsertionPoint, i) = static_cast<double>(points[j].y());
                scanBasePeak = points[j].y() > scanBasePeak ? points[j].y() : scanBasePeak;
                scanTIC += points[j].y();
            }
        }

        point2d tempPoint;
        tempPoint.setX(scanInfo.retTimeMinutes);
        tempPoint.setY(scanTIC);
        m_TIC.push_back(tempPoint);

        point2d tempPointBP;
        tempPointBP.setX(scanInfo.retTimeMinutes);
        tempPointBP.setY(scanBasePeak);
        m_basePeakChrom.push_back(tempPointBP);
    }

    Eigen::VectorXd timeIndexEigen(m_timeIndex.size());
    for (int i = 0; i < m_timeIndex.size(); ++i) {
        timeIndexEigen(i) = m_timeIndex[i];
    }

    m_timeIndexEigen = timeIndexEigen;
    buildingDataFileMatrix.makeCompressed();
    *output = buildingDataFileMatrix;

    if (buildingDataFileMatrix.nonZeros() == 0) {
        rrr(kError);
    }

    return e;
}

_PMI_END
