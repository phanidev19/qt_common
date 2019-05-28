/*
 * Copyright (C) 2012-2014 Protein Metrics Inc. - All Rights Reserved.
 * Unauthorized copying or distribution of this file, via any medium is strictly prohibited.
 * Confidential.
 */
 
#include "Convolution1dCore.h"
#include <QList>
#include <cmath>

_PMI_BEGIN
_CORE_BEGIN

bool makeSavitzkyGolayKernel(const int order, const int numb_pnts, std::vector<double> & vkrn)
{
	/*if vin.size() - 1 <= vkrn.size(){
		return false
	}*/

	vkrn.clear();
	if (order == 2){
		if (numb_pnts == 5){
			vkrn.push_back(1/35.0*(-3.0));
			vkrn.push_back(1/35.0*(12.0));
			vkrn.push_back(1/35.0*(17.0));
			vkrn.push_back(1/35.0*(12.0));
			vkrn.push_back(1/35.0*(-3.0));
		}else if (numb_pnts == 7){
			vkrn.push_back(1/21.0*(-2.0));
			vkrn.push_back(1/21.0*(3.0));
			vkrn.push_back(1/21.0*(6.0));
			vkrn.push_back(1/21.0*(7.0));
			vkrn.push_back(1/21.0*(6.0));
			vkrn.push_back(1/21.0*(3.0));
			vkrn.push_back(1/21.0*(-2.0));
		}
	}else if (order == 3){
		if (numb_pnts == 5){
			vkrn.push_back(1/35.0*(-3.0));
			vkrn.push_back(1/35.0*(12.0));
			vkrn.push_back(1/35.0*(17.0));
			vkrn.push_back(1/35.0*(12.0));
			vkrn.push_back(1/35.0*(-3.0));
		}else if (numb_pnts == 7){
			vkrn.push_back(1/21.0*(-2.0));
			vkrn.push_back(1/21.0*(3.0));
			vkrn.push_back(1/21.0*(6.0));
			vkrn.push_back(1/21.0*(7.0));
			vkrn.push_back(1/21.0*(6.0));
			vkrn.push_back(1/21.0*(3.0));
			vkrn.push_back(1/21.0*(-2.0));
		}else if (numb_pnts == 9){
			vkrn.push_back(1/231.0*(-21.0));
			vkrn.push_back(1/231.0*(14.0));
			vkrn.push_back(1/231.0*(39.0));
			vkrn.push_back(1/231.0*(54.0));
			vkrn.push_back(1/231.0*(59.0));
			vkrn.push_back(1/231.0*(54.0));
			vkrn.push_back(1/231.0*(39.0));
			vkrn.push_back(1/231.0*(14.0));
			vkrn.push_back(1/231.0*(-21.0));
		}else if (numb_pnts == 11){
			vkrn.push_back(1/429.0*(-36.0));
			vkrn.push_back(1/429.0*(9.0));
			vkrn.push_back(1/429.0*(44.0));
			vkrn.push_back(1/429.0*(69.0));
			vkrn.push_back(1/429.0*(84.0));
			vkrn.push_back(1/429.0*(89.0));
			vkrn.push_back(1/429.0*(84.0));
			vkrn.push_back(1/429.0*(69.0));
			vkrn.push_back(1/429.0*(44.0));
			vkrn.push_back(1/429.0*(9.0));
			vkrn.push_back(1/429.0*(-36.0));
		}else if (numb_pnts == 13){
			vkrn.push_back(1/143.0*(-11.0));
			vkrn.push_back(1/143.0*(0.0));
			vkrn.push_back(1/143.0*(9.0));
			vkrn.push_back(1/143.0*(16.0));
			vkrn.push_back(1/143.0*(21.0));
			vkrn.push_back(1/143.0*(24.0));
			vkrn.push_back(1/143.0*(25.0));
			vkrn.push_back(1/143.0*(24.0));
			vkrn.push_back(1/143.0*(21.0));
			vkrn.push_back(1/143.0*(16.0));
			vkrn.push_back(1/143.0*(9.0));
			vkrn.push_back(1/143.0*(0.0));
			vkrn.push_back(1/143.0*(-11.0));
			
		}
	}
    return !vkrn.empty();
}

double makeGaussian(double mean, double std_dev, double x) 
{
    if (std_dev <= 0) {
        if (x == 0)
            return 1;
        return 0;
    }

    const double PI = 3.14159265359;
    double variance2 = std_dev*std_dev; 
    double term = x-mean;                                        // center at zero
    return exp(-(term*term)/variance2)/sqrt(PI*variance2); 
}  


bool makeGaussianKernel(double std_dev, std::vector<double> & vkrn)
{
	int width;
	width = int(3.0 * std_dev + 1 - 1e-16);
	//if (int(3.0*std_dev) - 3.0*std_dev == 0){
	//	width = int(3.0*std_dev);
	//}else{
	//	width = int(3.0*std_dev) + 1;
	//}
	vkrn.clear();

	double x;
	double value;
	double sumOfElems = 0;
	std::vector<double> nonNormalvkrn;
	for(int i=0; i<2*width+1; i++){
		x = (i-width);
		value = makeGaussian(0.0, std_dev, x);
		nonNormalvkrn.push_back(value);
		sumOfElems += value;
	}
	for (int i=0; i<2*width+1; i++){
		vkrn.push_back(nonNormalvkrn[i]/sumOfElems);                         // noramlize descrete values from distribution
	}

	return true;                                                   // need come up with a valid successs condition
}

static bool conv(const double *in, double *out, const size_t in_size, const double *kernel,
                 const size_t kernel_size, int boundary_type)
{
    if (in_size == 0 || kernel_size == 0) {
        return false;
    }
    if (kernel_size % 2 == 0) {
        return false; // only accept kernels with an odd numbers
    }

    // implement boundary assumptions
    double lb = 0;
    double rb = 0;
    if (boundary_type == kBoundaryTypeZero) {
        lb = 0;
        rb = 0;
    } else if (boundary_type == kBoundaryTypeCopyEnd) {
        lb = in[0];
        rb = in[in_size - 1];
    }
    const size_t kernelHalf = kernel_size / 2;

    for (size_t i = 0; i < in_size; i++) {
        out[i] = 0;

        size_t n = 0;
        ptrdiff_t m = i + kernelHalf;
        for ( ; n < kernel_size; n++, m--) {
            if (m < 0) {
                out[i] += lb * kernel[n];
            } else if (m >= static_cast<ptrdiff_t>(in_size)) {
                out[i] += rb * kernel[n];
            } else {
                out[i] += in[m] * kernel[n];
            }
        }
    }
    return true;
}

bool conv(const std::vector<double> &in, const std::vector<double> &kernel, int boundary_type,
          std::vector<double> &out)
{
    if (in.empty())
        return false;
    if (kernel.empty())
        return false;

    out.resize(in.size());
    return conv(&in[0], &out[0], in.size(), &kernel[0], kernel.size(), boundary_type);
}

bool convForReal(const std::vector<double> &in, const std::vector<double> &kernel,
                 std::vector<double> &out)
{
    const ptrdiff_t in_size = in.size();
    const ptrdiff_t kernel_size = kernel.size();
    if (in_size == 0 || kernel_size == 0) {
        return false;
    }
    const ptrdiff_t out_size = in.size() + kernel.size() - 1;
    out.resize(out_size);

    for (ptrdiff_t n = 0; n < out_size; n++) {
        for (ptrdiff_t m = kernel_size - 1; m >= 0; m--) {
            if (n < m || n + 1 > in_size + m) {
                out[n] += 0;
            } else {
                out[n] += in[n - m] * kernel[m];
            }
        }
    }
    return true;
}

_CORE_END
_PMI_END


