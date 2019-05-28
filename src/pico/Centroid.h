// ********************************************************************************************
//  Centroid.h : A collection of utility functions
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This class provides peak centroiding based on Gaussian smoothing, derived from pmi_commons 
// api. The class provides access to Gaussian smoothing in addition to centroiding. In either
// case, the input is a peak list vector of pairs of (mz, intensity) points, and the output
// is a similar peak list vector of pairs of centroided/smoothed (mz, intensity) points.  
//
//      @Arguments:  const std::vector<std::pair<double, double> > &data -- the input peak list vector
//                   std::vector<std::pair<double, double> > &centroided_out -- the output peak list vector of centroided peaks
//              or   std::vector<std::pair<double, double> > &smoothed_data -- the output peak list vector of Gaussian-smoothed peaks
//
// The code accepts the following 3 optional arguments:
//         @Params:  uncertainty_value <double> -- default value of 0.01
//                   uncertainty_type <enum> -- one of 3 types defined in centroid.h: constant, square-root, or linear uncertainty
//
// *******************************************************************************************

#ifndef CENTROID_H
#define CENTROID_H

#include <iostream>
#include <fstream>
#include <istream>
#include <vector>
#include <iomanip>
#include <string>

#ifdef _MSC_VER
#  include "PicoCommon.h"
_PICO_BEGIN // visual studio only
#else
#include <stdio.h>
#include <cmath>
namespace pmi { // needed for QT with pwiz
#endif

#if defined(QT_VERSION)
#include <QPoint>
typedef QPointF point2d;
#else

//local qpoint def w/o qt
class point2d
{
public:
    point2d() : xp(0), yp(0)
    {
    }

    point2d(double xpos, double ypos) : xp(xpos), yp(ypos)
    {
    }

    double x() const
    {
        return xp;
    }
    double y() const
    {
        return yp;
    }

    double &rx()
    {
        return xp;
    }
    double &ry()
    {
        return yp;
    }

    void setX(double x)
    {
        xp = x;
    }
    void setY(double y)
    {
        yp = y;
    }

private:
    double xp;
    double yp;
};
#endif

#define sigma_factor 4.0;
#define intensity_thr 0.2;

//#define PI_VAL 3.14159265359
#define E_VAL 2.71828184590
#define ONE_OVER_MILLION 0.000001
#define SQUARE_ROOT_THOUSAND 31.6227766017

enum UncertaintyScaling {
    ConstantUncertainty = 0,
    SquareRootUncertainty,
    LinearUncertainty
};

inline bool point2d_less_x(const point2d & p1, const point2d & p2) {
    return p1.x() < p2.x();
}

inline bool point2d_less_y(const point2d & a, const point2d & b) {
    return a.y() < b.y();
}

inline bool point2d_less_x_and_then_y(const point2d & p1, const point2d & p2) {
    if (p1.x() == p2.x()) {
        return p1.y() > p2.y();  //have the most intense show up first.
    }
    return p1.x() < p2.x();
}

// Gets the interpolated point x from the two points pointA and pointB
inline double interpolate(const point2d & pointA, const point2d & pointB, double x) {
    double XdistanceFromAtoB = pointB.x() - pointA.x();
    if (XdistanceFromAtoB == 0)
        return pointA.y();
    double XdistanceFromXtoA = (x - pointA.x());
    // Gets the percent x is to B.x(). Ex, if x is at A.x(), then it is 0, if x is at B.x(), then it is 1.
    double distanceProportion = XdistanceFromXtoA / (XdistanceFromAtoB);
    // Same as (B.y() - A.y()) * distanceProportion + A.y(). We add the final A.y() in order to scale it to correct location.
    double y = pointB.y() * distanceProportion + pointA.y() * (1 - distanceProportion);
    return y;
}


//! Plot related typedefs
typedef int pt2idx;
typedef std::vector<pt2idx> point2dIdxList;
typedef std::vector<point2d> point2dList;

// output points as indicated by arrayA (column one) and arrayB (column two)
// dir needs to end with '\'
inline void outputArraysToCSV(double* arrayA, float* arrayB, unsigned int length, const std::wstring &filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (unsigned int i = 0; i < length; i++) {
            file << std::setprecision(15) << arrayA[i] << "," << arrayB[i] << "\n";
        }
        file.close();
    }
    else {
        std::wcerr << L"Could not open file " << filename;
    }
}

inline void outputArraysToCSV(double* arrayA, double* arrayB, unsigned int length, const std::wstring &filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        for (unsigned int i = 0; i < length; i++) {
            file << arrayA[i] << "," << arrayB[i] << "\n";
        }
        file.close();
    }
    else {
        std::wcerr << L"Could not open file " << filename;
    }
}

inline void writePointListToCSV(const point2dList &pointList, const std::wstring &filename) {
    double *arrayA = new double[pointList.size()];
    double *arrayB = new double[pointList.size()];
    for (unsigned int i = 0; i < pointList.size(); i++) {
        const point2d &point = pointList[i];
        arrayA[i] = point.x();
        arrayB[i] = point.y();
    }

    outputArraysToCSV(arrayA, arrayB, static_cast<unsigned int>(pointList.size()), filename);

    delete[] arrayA;
    delete[] arrayB;
}

struct CentroidOptions {
    double startSigma = 0.02;
    double endSigma = 0.02;
    pico::UncertaintyScaling uncertaintyType = pico::ConstantUncertainty;
    int topK = 1000;
    double mergeRadius = 0.08;
    bool adjustIntencity = false;
    bool sortMz = true;
};

class Centroid
{
public:
    // constructor:
    Centroid(void);

    // destructor:
    ~Centroid(void);

    // smooth and centroid
    bool smooth_and_centroid(std::vector<std::pair<double, double> > &data, double start_sigma,
                             double end_sigma, const UncertaintyScaling uncertainty_type,
                             int top_k, double merge_radius, bool adjust_intensity,
                             bool sort_mz, std::vector<std::pair<double, double> > &centroided_out);

    bool smooth_and_centroid(std::vector<std::pair<double, double> > &data,
                             const CentroidOptions &options,
                             std::vector<std::pair<double, double> > &centroided_out);

    // gaussian smooth only - for debug or if needed for other purpose
    bool gaussian_smooth2(const std::vector<std::pair<double, double> > &in, double start_sigma, double end_sigma, const UncertaintyScaling uncertainty_type, const std::vector<double> &gaussian_table, std::vector<std::pair<int, double> > &smoothed_data);

private:
    // private vars -- none

    // compute standard deviation
    inline bool compute_stdev(double mz, double uncertainty_value, UncertaintyScaling uncertainty_type, double &uncertainty);

    // get gaussian value
    inline double getGaussianValue(double mean, double std_dev, double x_position);

    // make gaussian table
    inline std::vector<double> makeUnitGaussianTable(double range, int count);

    // compute max index list
    inline std::vector<std::pair<int, double> > computeMaxIndexList2(const std::vector<std::pair<double, double> > &data, const std::vector<std::pair<int, double> > &smooth_data);

    // make points from index
    inline void makePointsFromIndex(const std::vector<std::pair<double, double> > &m_pointList, const std::vector<int> &index_list, std::vector<std::pair<double, double> > & points);

    // make points from index
    //inline void makePointsFromIndexWeightedAverageOfNeighbors(const std::vector<std::pair<double, double> > &m_pointList, const std::vector<int> &index_list, std::vector<std::pair<double, double> > & points);

    // restore intensity
    inline void restoreIntensity(std::vector<std::pair<double, double> > &centroided_out, const std::vector<std::pair<double, double> > &data, double start_sigma, double end_sigma, const UncertaintyScaling uncertainty_type);

    // retain only the top intesity points
    inline void retainIntensePoints2(std::vector<std::pair<double, double> > &data, std::vector<std::pair<int, double> > &index_list, int top_k, bool sort_mz, std::vector<std::pair<double, double> > &centroided_out);

    // remove duplicate points
    inline void removeDuplicatePoints(std::vector<std::pair<double, double> > & plist, bool do_sort_by_x, double fudge = 1e-15);

    // consolidate by merge radius
    void consolidateMergeRadius(const std::vector<std::pair<double, double> > &data, double merge_radius, std::vector<std::pair<int, double> > &max_list);

    inline int my_round(double val);

};

#ifdef _MSC_VER
_PICO_END // visual studio only
#else
} //pmi // needed for QT with pwiz
#endif



#endif // CENTROID_H
