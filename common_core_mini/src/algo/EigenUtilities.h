/*
 * Copyright (C) 2019 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 *
 * Author: Andrew Nichols anichols@proteinmetrics.com
 */

#ifndef EIGEN_UTILITIES_H
#define EIGEN_UTILITIES_H

#include "CommonFunctions.h"
#include "pmi_common_core_mini_export.h" 

#include <common_errors.h>
#include <common_math_types.h>
#include <pmi_core_defs.h>

#include <Eigen/Core>
#include <Eigen/Dense>
#include <Eigen/Sparse>

#include <random>

_PMI_BEGIN

class PMI_COMMON_CORE_MINI_EXPORT EigenUtilities
{
public:
    /*!
     * @brief Returns max value in SparseVector
     */
    template <typename T>
    static T sparseVectorMax(const Eigen::SparseVector<T> &vec)
    {
        T returnValue = std::numeric_limits<T>::min();
        for (Eigen::SparseVector<T>::InnerIterator it(vec); it; ++it) {
            returnValue = it.value() > returnValue ? it.value() : returnValue;
        }
        return returnValue;
    }

    /*!
     * @brief Returns min value in SparseVector
     */
    template <typename T>
    static T sparseVectorMin(const Eigen::SparseVector<T> &vec)
    {
        T returnValue = std::numeric_limits<T>::max();
        for (Eigen::SparseVector<T>::InnerIterator it(vec); it; ++it) {
            returnValue = it.value() < returnValue ? it.value() : returnValue;
        }
        return returnValue;
    }

    /*!
     * @brief Translates all indexes by the roll distance.
     *
     *  CONCRETELY:  If vector contains values at indicies 100 and 105, and rollDistance = 10, the
     * values will be moved to indicies 110 and 115 respectively.
     *
     *  NOTE:  That this function does not wrap, i.e. if a translated value is out of range, it will
     * just vanish.
     */
    template <typename T>
    static void sparseVectorRoll(Eigen::SparseVector<T> &vec, int rollDistance,
                                 Eigen::SparseVector<T> *output)
    {

        Eigen::SparseVector<T> buildingVec = vec;
        buildingVec.setZero();
        for (Eigen::SparseVector<T>::InnerIterator it(vec); it; ++it) {
            int newIndex = it.index() + rollDistance;
            if (newIndex >= 0 && newIndex < buildingVec.rows()) {
                buildingVec.coeffRef(newIndex) = it.value();
            }
        }
        buildingVec.prune(0.0);
        *output = buildingVec;
    }

    /*!
     * @brief Returns a sparse vector w/ all values below thresholdValue removed.
     */
    template <typename T>
    static void sparseVectorThresholdByValue(Eigen::SparseVector<T> &vec, double thresholdValue)
    {
        for (Eigen::SparseVector<T>::InnerIterator it(vec); it; ++it) {
            if (it.value() < thresholdValue) {
                vec.coeffRef(it.index()) = 0;
            }
        }
        vec.prune(0.0);
    }

    /*!
     * @brief Varies the values of each index individually by a separate random amount for each.
     */
    template <typename T>
    static void sparseVectorJitter(Eigen::SparseVector<T> &vec, double jitterPercent)
    {

        jitterPercent = jitterPercent > 1 || jitterPercent < 0 ? 1 : 1 - jitterPercent;

        for (Eigen::SparseVector<T>::InnerIterator it(vec); it; ++it) {
            vec.coeffRef(it.index()) = it.value() * randomDoubleNumberInRange(jitterPercent, 1);
        }
    }

    /*!
     * @brief Adds randomly generated noise to vector as a function of max value of sparse vector.
     *
     *  NOTE:  numPoints is set to 1000 by default.  If sparse vector is less than 2000 points,
     * Regular vectors should be used instead as there is no benifit to using sparse vector for such
     * small vectors.
     */
    template <typename T>
    static void sparseVectorAddNoise(Eigen::SparseVector<T> &vec, double noisePercentOfMax,
                                     int numPoints = 1000)
    {

        int vecSize = vec.rows();

        ////Asserts that no more than half the scan is composed of noise.
        Q_ASSERT(numPoints < static_cast<int>(vecSize * 0.5));

        noisePercentOfMax = noisePercentOfMax > 1 || noisePercentOfMax < 0 ? 1 : noisePercentOfMax;
        Eigen::VectorXd rando(numPoints);
        rando.setRandom();
        rando = rando.cwiseAbs();

        T modPercent = sparseVectorMax(vec) * noisePercentOfMax;
        rando *= modPercent;

        for (int i = 0; i < rando.rows(); ++i) {
            int insertionIndex = randomIntNumberInRange(0, vecSize);
            T randoCalrissian = static_cast<T>(rando.row(i).maxCoeff());
            vec.coeffRef(insertionIndex) += randoCalrissian;
        }

        vec.prune(0.0);
    }

    static Eigen::RowVectorXd generateGaussianCurve(double length)
    {
        const int nrolls = 20000 * length; // number of experiments
        std::default_random_engine generator;

        double sigmaDivisor = 8;
        std::normal_distribution<double> distribution(std::floor(length / 2),
                                                      length / sigmaDivisor);

        Eigen::RowVectorXd vec(static_cast<int>(length));
        vec.setZero();
        for (int i = 0; i < nrolls; ++i) {
            int number = static_cast<int>(distribution(generator));
            if ((number >= 0) && (number < static_cast<int>(length))) {
                vec(number) += 1;
            }
        }

        if (vec.maxCoeff() == 0) {
            return vec;
        }

        return vec / vec.maxCoeff();
    }

    // TODO (Drew Nichols 2019-04-12) rethink templating design below this point to make them more
    // generalized.
    template <typename EigenVector>
    static EigenVector returnValuesOfVectorAtIndicies(const EigenVector &vec,
                                                      const std::vector<int> &indexes)
    {
        EigenVector valuesAtIndicies(indexes.size());
        for (size_t i = 0; i < indexes.size(); ++i) {
            valuesAtIndicies(i) = vec(indexes[i]);
        }
        return valuesAtIndicies;
    }

    template <typename T>
    static QVector<T> returnValuesNearestToCutoff(const Eigen::RowVectorXd &vec, double value)
    {

        Eigen::RowVectorXd diff = (vec.array() - value).cwiseAbs();
        std::vector<double> diffVec(diff.data(), diff.data() + diff.size());
        std::sort(diffVec.begin(), diffVec.end());

        QVector<T> returnVec;
        for (size_t i = 0; i < diffVec.size(); ++i) {
            for (size_t j = 0; j < diff.cols(); ++j) {
                if (diff(j) == diffVec[i]) {
                    returnVec.push_back(vec(j));
                    break;
                }
            }
        }
        return returnVec;
    }

    static QVector<int> returnIndexNearestToCutoff(const Eigen::RowVectorXd &vec, double value)
    {

        Eigen::RowVectorXd diff = (vec.array() - value).cwiseAbs();
        std::vector<double> diffVec(diff.data(), diff.data() + diff.size());
        std::sort(diffVec.begin(), diffVec.end());

        QVector<int> returnVec;
        for (size_t i = 0; i < diffVec.size(); ++i) {
            for (int j = 0; j < diff.cols(); ++j) {
                if (diff(j) == diffVec[i]) {
                    returnVec.push_back(j);
                    break;
                }
            }
        }
        return returnVec;
    }

    static double calculateRMSE(const Eigen::RowVectorXd &vec1, const Eigen::RowVectorXd &vec2)
    {

        Eigen::RowVectorXd vec = (vec1 - vec2);
        return std::sqrt(vec.cwiseProduct(vec).sum() / vec.cols());
    }

    static double calculateMeanOfVector(const Eigen::RowVectorXd &vec)
    {
        return vec.sum() / vec.cols();
    }

    static double calculateStDevOfVector(const Eigen::RowVectorXd &vec)
    {
        double mean = calculateMeanOfVector(vec);
        Eigen::RowVectorXd diff = vec.array() - mean;
        return std::sqrt(diff.cwiseProduct(diff).sum() / vec.cols());
    }

    static Eigen::RowVectorXd roll(const Eigen::RowVectorXd &vec, int rollDistance)
    {

        Eigen::RowVectorXd returnVec(vec.cols());
        if (rollDistance < 0) {
            Eigen::RowVectorXd front = vec.middleCols(0, std::abs(rollDistance));
            Eigen::RowVectorXd back
                = vec.middleCols(std::abs(rollDistance), vec.cols() - std::abs(rollDistance));
            returnVec << back, front;
            return returnVec;
        }

        else if (rollDistance > 0) {
            Eigen::RowVectorXd front = vec.middleCols(0, vec.cols() - std::abs(rollDistance));
            Eigen::RowVectorXd back
                = vec.middleCols(vec.cols() - std::abs(rollDistance), std::abs(rollDistance));
            returnVec << back, front;
            return returnVec;
        }

        return vec;
    }

    static void fitPolynomialQRDecomposition(const Eigen::MatrixXd points, int order,
                                             QVector<double> *coeff)
    {
        Eigen::MatrixXd A(points.rows(), order + 1);
        Eigen::VectorXd yv_mapped = points.col(1);

        // create decomposition matrix
        for (int i = 0; i < points.rows(); i++) {
            double pointValue = points(i, 0);
            for (size_t j = 0; j < order + 1; j++) {
                A(i, j) = pow(pointValue, j);
            }
        }

        // solve for linear least squares fit
        const Eigen::VectorXd result = A.householderQr().solve(yv_mapped);

        coeff->resize(order + 1);
        for (int i = 0; i <= order; ++i) {
            (*coeff)[i] = result[i];
        }
           
    }
};
_PMI_END

#endif // EIGEN_UTILITIES_H
