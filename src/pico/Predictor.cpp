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

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4996) // xmemory warning C4996: 'std::_Uninitialized_copy0': Function call with
                              // parameters that may be unsafe - this call relies on the caller to check
                              // that the passed values are correct.
#endif

#include <stdio.h>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>
#include "Predictor.h"

using namespace boost::numeric::ublas;

_PICO_BEGIN

Predictor::Predictor(int fit_degree)
{
    assert(fit_degree > 0);
    size = fit_degree + 1;
    dblsize = 2 * size - 1;
    matrix<double> mat1 (size, size+1); mat=mat1;
    for (unsigned int ii = 0; ii < mat.size1(); ii++)
        for (unsigned int jj = 0; jj < mat.size2(); jj++)
            mat (ii, jj) = 0;
    boost::numeric::ublas::vector<double> vect1 (dblsize); vect=vect1;
    for (int ii = 0; ii < dblsize; ii++)
        vect[ii] = 0;
    cnt = 0;
}

Predictor::~Predictor()
{

}

matrix<double> Predictor::getMat()
{
    return mat;
}

boost::numeric::ublas::vector<double> Predictor::getVect(){
    return vect;
}

double* Predictor::predict()
{
    vector<double> a_col = vect;
    matrix<double> a_mat (size, size+1);
    for (unsigned int ii=0; ii<mat.size1(); ii++) 
        for (unsigned int jj=0; jj<mat.size2(); jj++) 
            a_mat(ii, jj) = mat(ii, jj);
        
    a_col[0] += cnt;
    for (int ii=0; ii<size; ii++) 
        for (int jj=0; jj<size; jj++) 
            a_mat(ii, jj) = a_col[ii+jj];

    mat_reorder(a_mat);
/*
    for (unsigned int ii = 0; ii < a_mat.size1 (); ii++){
        for (unsigned int jj = 0; jj < a_mat.size2 (); jj++)
            std::cout << a_mat(ii,jj) << ", " ;
        std::cout << std::endl;
    }
*/
    double* result = (double *)malloc(4*sizeof(double));
    if (result == NULL){
        std::cout << "can't allocate result memory\n";
        return NULL;
    }
    for (int jj=0; jj<size; jj++)
        result[jj] = a_mat(jj, size);
    return result; 
}

void Predictor::initialize(double* data)
{
    for (int ii = 0; ii < dblsize; ii++)
        vect[ii] = data[ii];
    for (int ii=0; ii<size; ii++) 
        mat(ii, size) = data[dblsize+ii];
    for (unsigned int ii = 0; ii < mat.size1(); ii++)
        for (unsigned int jj = 0; jj < mat.size1(); jj++)
            mat (ii, jj) = 0;
}

// add x,y point
void Predictor::addPoint(double xx, double yy) {
    cnt++;
    for (int pp=1; pp<dblsize; pp++) 
        vect[pp] += pow(xx, pp); 

    mat(0, size) += yy;  
    for (int pp=1; pp<size; pp++) 
        mat(pp, size) += pow(xx, pp) * yy;  
}

// matrix scalar divide
void Predictor::mat_scalar_divide(matrix<double> &mat1, int ii, int jj, int cols) {
    for (int kk=jj+1; kk<cols; kk++)
        mat1(ii, kk) /= (mat1(ii, jj)+1E-12); 
    mat1(ii, jj) = 1.0; 
}
    
// matrix reorder
void Predictor::mat_reorder(matrix<double> &mat1) {
    const size_t rows = mat1.size1(), cols = mat1.size2();
    size_t ii = 0, jj = 0;
    while (ii<rows && jj<cols) {
        size_t kk = ii; // first non zero
        while (kk <rows && mat1(kk, jj) == 0) 
            kk++;
        if (kk <rows) {
            if (kk != ii){ 
                matrix_row<matrix<double>> rowi (mat1, ii); 
                matrix_row<matrix<double>> rowj (mat1, jj); 
                rowi.swap(rowj);
            }
            if (mat1(ii, jj) != 1) {
                mat_scalar_divide(mat1, static_cast<int>(ii), static_cast<int>(jj), static_cast<int>(cols));
            }
            mat_subtract_row(mat1, static_cast<int>(ii), static_cast<int>(jj), static_cast<int>(rows), static_cast<int>(cols));
            ii++;
        }
        jj++;
    }
}
    
//matrix subtract row
void Predictor::mat_subtract_row(matrix<double> &mat1, int ii, int jj, int rows, int cols) {
    for (int kk=0; kk <rows; kk++) {
        if (kk!=ii && mat1(kk, jj) != 0) {
            for (int mm=jj+1; mm<cols; mm++) 
                mat1(kk, mm) -= mat1(kk, jj) * mat1(ii, mm); 
            mat1(kk, jj) = 0; 
        }
    }
}

_PICO_END

#if defined(_MSC_VER)
#pragma warning(pop)
#endif
