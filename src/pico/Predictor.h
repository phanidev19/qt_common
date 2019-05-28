// ********************************************************************************************
// Predictor.cpp :  An intenal prediction module
//
//      @Author: D.Kletter    (c) Copyright 2014, PMI Inc.
// 
// This module reads in a highly compressed byspec2 file and produces an uncompressed byspec2 file.
// The algorithm uses a proprietary run length hop coding that leverages the sparse data nature
// of the underlying data.
//
// The algorithm employes an internal predicotr to interpolate and extrapolate time series data based on 
// existing observations. The inteface includes a call for adding new observation points, and a separate
// call to predict the outcome.
//
// void Predictor::addPoint(const double xx, const double yy) 
//      @Arguments:  <xx>  the x coordinate of an observation point
//                   <yy>  the y coordinate of an observation point
//
// void Predictor::Predict()
//
//      @Returns: true if successful, false otherwise
//
// *******************************************************************************************

#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <stdlib.h> 
#include <assert.h> 
#include <boost/numeric/ublas/matrix.hpp>
#include "PicoCommon.h"

_PICO_BEGIN

class Predictor
{
public:

    // Constructor:
    Predictor(int fit_degree);

    // Destructor:
    ~Predictor();

    // initialize
    void initialize(double* data);

    // add point
    void addPoint(double xx, double yy);

    // predictor
    double* predict();

    boost::numeric::ublas::matrix<double> getMat();
    boost::numeric::ublas::vector<double> getVect();

private:
    // private vars
    int size, dblsize;
    boost::numeric::ublas::matrix<double> mat;
    boost::numeric::ublas::vector<double> vect;
    int cnt;

    // private functions

    // matrix scalar divide
    void mat_scalar_divide(boost::numeric::ublas::matrix<double> &mat1, int ii, int jj, int cols);

    // matrix reorder
    void mat_reorder(boost::numeric::ublas::matrix<double> &mat1);

    //matrix subtract row
    void mat_subtract_row(boost::numeric::ublas::matrix<double> &mat1, int ii, int jj, int rows, int cols);

};

_PICO_END


#endif
