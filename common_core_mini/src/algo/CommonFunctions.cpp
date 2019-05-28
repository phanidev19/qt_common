/*
* Copyright (C) 2018 Protein Metrics Inc. - All Rights Reserved.
* Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
* Confidential.
*
* Author: Andrew Nichols anichols@proteinmetrics.com
* Author: Lukas Tvrdy ltvrdy@milosolutions.com
*/

#include "CommonFunctions.h"
#include "FeatureFinderParameters.h"
#include "Point2dListUtils.h"

#include <QtSqlUtils.h>
#include <sqlite3.h>

#include <QSqlDatabase>

#include "pmi_common_core_mini_debug.h"

using namespace pmi;

ImmutableFeatureFinderParameters ffParams;

Q_DECL_CONSTEXPR int FeatureFinderUtils::hashMz(double mz, double granularity) {
    return static_cast<int>(std::round(mz * granularity) );
}

PMI_COMMON_CORE_MINI_EXPORT std::vector<double> FeatureFinderUtils::extractUniformIntensity(const point2dList &scanPart, double mz, double mzRadius)
{


    double centerPoint = mz;
    int clusterDistance = hashMz(mzRadius, ffParams.vectorGranularity);
    int sliceStart = hashMz(centerPoint, ffParams.vectorGranularity) - clusterDistance;
    int sliceEnd = hashMz(centerPoint, ffParams.vectorGranularity) + clusterDistance;

    const size_t size = sliceEnd - sliceStart + 1;
    std::vector<double> uniformIntensitySegment(size, double(0.0));

    double mzStart = mz - mzRadius;
    double mzEnd = mz + mzRadius;

    // extract the mzStart, mzEnd from scanPart

    const point2dList data = Point2dListUtils::extractPointsIncluding(scanPart, mzStart, mzEnd);

    for (const point2d &mzIntensity : data) {
        int index = hashMz(mzIntensity.x(), ffParams.vectorGranularity) - sliceStart;
        uniformIntensitySegment[index] += mzIntensity.y();
    }

    return uniformIntensitySegment;
}


_PMI_BEGIN

Eigen::RowVectorXd relu(Eigen::RowVectorXd inputVector) {
    return (inputVector.array() < 0).select(0, inputVector);
}

Eigen::RowVectorXd sigmoid(Eigen::RowVectorXd inputVector) {
    return 1 / (Eigen::exp(-inputVector.array()) + 1);
}

Eigen::RowVectorXd convertVectorToEigenRowVector(const std::vector<double> & inputVector) {
    Eigen::RowVectorXd vecEigenVec(inputVector.size());
    for (size_t i = 0; i < inputVector.size(); ++i) {
        vecEigenVec(i) = inputVector[i];
    }
    return vecEigenVec;
}

Eigen::MatrixXd appendEigenMatrixWithVector(Eigen::MatrixXd inputMatrix, Eigen::VectorXd inputVector) {

    inputMatrix.conservativeResize(inputMatrix.rows() + 1, inputMatrix.cols());
    inputMatrix.row(inputMatrix.rows() - 1) = inputVector;

    return inputMatrix;
}


 Err readNeuralNetworkWeightsSqlQT(const std::string &nnPath, int modelID, std::vector<Eigen::MatrixXd> *matrices)
{
    Err e = kNoErr;
    QSqlDatabase db;
    static int counter = 0;
    counter++;
    e = addDatabaseAndOpen(QString("db_%1_connectCount_%2").arg(modelID).arg(counter),
                           QString::fromStdString(nnPath), db);

    if (e != kNoErr) {
        debugCoreMini() << "Failed to load neural network db"
                        << QString::fromStdString(pmi::convertErrToString(e));
    }

    QString sql = "SELECT layers FROM NN_File WHERE ID = :modelID";
    QSqlQuery q = makeQuery(db, true);
    e = QPREPARE(q, sql); ree;
    q.bindValue(":modelID", modelID);
    if (!q.exec()) {
        debugCoreMini() << "Error getting layer count for model number" << q.lastError();
        return e;
    }

    bool ok = true;
    int numberOfLayersInModel = -1;
    if (q.next()) {
        numberOfLayersInModel = q.value(0).toInt(&ok);
        Q_ASSERT(ok);
    }

    std::vector<Eigen::MatrixXd> matrixBuilderVector;
    for (int i = 0; i < numberOfLayersInModel; ++i) {
        sql = "SELECT MAX(row) as maxRow, MAX(col) as maxCol "
            "FROM Weights "
            "WHERE nn_file_id = :nn_file_id "
            "AND layer = :layer ";
        e = QPREPARE(q, sql); ree;
        q.bindValue(":nn_file_id", modelID);
        q.bindValue(":layer", i);
        if (!q.exec()) {
            debugCoreMini() << "Error getting layer count for model number" << q.lastError();
            return e;
        }

        int matrixRowsMax = -1;
        int matrixColsMax = -1;
        if (q.next()) {
            matrixRowsMax = q.value(0).toInt(&ok) + 1;
            Q_ASSERT(ok);
            matrixColsMax = q.value(1).toInt(&ok) + 1;
            Q_ASSERT(ok);
        }

        Eigen::MatrixXd matrixBuilder(matrixRowsMax, matrixColsMax);
        sql = "SELECT row, col, val "
            "FROM Weights "
            "WHERE nn_file_id = :nn_file_id "
            "AND layer = :layer "
            "ORDER BY row, col ASC";

        e = QPREPARE(q, sql); ree;
        q.bindValue(":nn_file_id", modelID);
        q.bindValue(":layer", i);
        if (!q.exec()) {
            debugCoreMini() << "Error getting layer count for model number" << q.lastError();
            return e;
        }

        int matrixRow = -1;
        int matrixCol = -1;
        double matrixVal = -1;
        while (q.next()) {
            matrixRow = q.value(0).toInt(&ok);
            Q_ASSERT(ok);
            matrixCol = q.value(1).toInt(&ok);
            Q_ASSERT(ok);
            matrixVal = q.value(2).toDouble(&ok);
            Q_ASSERT(ok);

            matrixBuilder(matrixRow, matrixCol) = matrixVal;
        }

        matrixBuilderVector.push_back(matrixBuilder);
    }

    *matrices = matrixBuilderVector;
    db.close();
    return e;
}

Eigen::RowVectorXd convertPoint2dIntoVector(const point2dList &scan) {

    Eigen::RowVectorXd fullScan(static_cast<int>(ffParams.mzMax * ffParams.vectorGranularity));
    fullScan.setZero();
    for (size_t i = 0; i < scan.size(); ++i) {
        double insertionPoint = FeatureFinderUtils::hashMz(scan[i].x(), ffParams.vectorGranularity);
        fullScan(insertionPoint) = scan[i].y();
    }

    return fullScan;
}

double euclidDistance(Eigen::RowVectorXd x, Eigen::RowVectorXd y) {
    return sqrt(x.dot(x) - 2 * x.dot(y) + y.dot(y));
}

_PMI_END

