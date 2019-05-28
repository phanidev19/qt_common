// ********************************************************************************************
//  Centroid.cpp : Gaussian Smoothing and Centroid
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

#include <algorithm>
#include <utility>
#include "Centroid.h"
//#include "PicoUtil.h"
//#include "PicoLocalCompress.h"

#define MIN_CENTROID_POINTS 10
#define NEAR_ZERO 1E-4
#define GAUSS_TEMPLATE_COUNT 101
#define GAUSS_TEMPLATE_RANGE 3.0
#define MIN_GAUSS_RANGE 0.5
#define INTENSITY_THR 0.0

using namespace  std;

#ifdef _MSC_VER
_PICO_BEGIN // visual studio only
#else
namespace pmi { // needed for QT with pwiz
#endif


struct sort_by_x {
    inline bool operator()(const std::pair<double, double> &left, const std::pair<double, double> &right)
    {
        return left.first < right.first;
    }
};

struct sort_by_y {
    inline bool operator()(const std::pair<double, double> &left, const std::pair<double, double> &right)
    {
        return left.second < right.second;
    }
};

struct sort_by_mz {
    inline bool operator()(const std::pair<int, double> &left, const std::pair<int, double> &right)
    {
        return left.first < right.first;
    }
};

struct sort_by_intens {
    inline bool operator()(const std::pair<int, double> &left, const std::pair<int, double> &right)
    {
        return left.second > right.second;
    }
};

/*
struct sort_by_x_then_y {
    inline bool operator()(const std::pair<double, double> &left, const std::pair<double, double> &right) {
        if (left.first == right.first) {
            return left.second > right.second;  //have the most intense show up first.
        }
        return left.first < right.first;
    }
};*/


Centroid::Centroid(void)
{
}


Centroid::~Centroid(void)
{
}

#ifdef PMI_MS_OUTPUT_DEBUG_CENTROID_SMOOTHING_TO_FILE
static void output_to_file(const std::vector<std::pair<double, double> > &data, const std::wstring & filename)
{
    FILE * fp = nullptr;
    if (0 != _wfopen_s(&fp, filename.c_str(), L"w")) {
        return;
    }

    for (unsigned int i = 0; i < data.size(); i++) {
        fprintf(fp, "%f, %f\n", data[i].first, data[i].second);
    }

    if (fp) fclose_safe(fp);
}
#endif
//#define OUTPUT_DEBUG_SMOOTHING_TO_FILE

// smooth and centroid
bool Centroid::smooth_and_centroid(std::vector<std::pair<double, double> > &data, double start_sigma, double end_sigma,
    const UncertaintyScaling uncertainty_type, int top_k, double merge_radius, bool adjust_intensity,
    bool sort_mz, std::vector<std::pair<double, double> > &centroided_out)
{
    centroided_out.clear();
    if (data.size() < MIN_CENTROID_POINTS) { // bypass if too few points, no filtering
        for (unsigned int ii = 0; ii<data.size(); ii++) {
            double intens = data[ii].second;
            if (intens>0.0)
                centroided_out.push_back(std::pair<double, double>(data[ii].first, intens));
        }
        return true;
    }

    // create table 
    std::vector<double> gaussian_table = makeUnitGaussianTable(GAUSS_TEMPLATE_RANGE, GAUSS_TEMPLATE_COUNT);
    if (gaussian_table.size() == 0)
        return false;

    // gaussian smooth the data
    std::vector<std::pair<int, double> > smoothed_data;
    if (!gaussian_smooth2(data, start_sigma, end_sigma, uncertainty_type, gaussian_table, smoothed_data)) {
        return false;
    }

#ifdef PMI_MS_OUTPUT_DEBUG_CENTROID_SMOOTHING_TO_FILE
    static int count = 0;
    count++;
    if (count % 500 == 0) {
        std::string before_file = toString(count) + "_before_smooth_" + toString(uncertainty_value, 5);
        std::string after_file = toString(count) + "_after_smooth_" + toString(uncertainty_value, 5);

        output_to_file(data, before_file);
        output_to_file(smoothed_data, after_file);
    }
#endif

    // make max indicies
    std::vector<std::pair<int, double> > max_list = computeMaxIndexList2(data, smoothed_data);
    consolidateMergeRadius(data, merge_radius, max_list);

#ifdef PMI_MS_OUTPUT_DEBUG_CENTROID_SMOOTHING_TO_FILE
    if (count % 500 == 0) {
        std::string after_centroided_file = toString(count) + "_after_centroided_" + toString(uncertainty_value, 5);
        output_to_file(centroided_out, after_centroided_file);
    }
#endif

    smoothed_data.clear();
    std::vector<std::pair<int, double> >(smoothed_data).swap(smoothed_data);

    // retain top k points
    retainIntensePoints2(data, max_list, top_k, false, centroided_out);

    max_list.clear(); // free capacity
    std::vector<std::pair<int, double> >(max_list).swap(max_list);

    if (adjust_intensity)
        restoreIntensity(centroided_out, data, start_sigma, end_sigma, uncertainty_type);

    data.clear(); //<---------- d_kletter fix bad cleanup
    std::vector<std::pair<double, double> >(data).swap(data);

    if (sort_mz)
        std::sort(centroided_out.begin(), centroided_out.end(), sort_by_x());

    return true;
}

bool Centroid::smooth_and_centroid(std::vector<std::pair<double, double> > &data,
                                   const CentroidOptions &options,
                                   std::vector<std::pair<double, double> > &centroided_out)
{
    return smooth_and_centroid(data, options.startSigma, options.endSigma, options.uncertaintyType,
                               options.topK, options.mergeRadius, options.adjustIntencity,
                               options.sortMz, centroided_out);
}

// Gaussian smoothing
bool Centroid::gaussian_smooth2(const std::vector<std::pair<double, double> > &in, double start_sigma, double end_sigma, const UncertaintyScaling uncertainty_type, const std::vector<double> &gaussian_table, std::vector<std::pair<int, double> > &smoothed_data)
{
    smoothed_data.clear();
    ptrdiff_t in_size = in.size();
    if (in_size <= 0) // || start_kernel.size() <= 0 || start_kernel.size() % 2 == 0)
        return false;
    if (uncertainty_type == SquareRootUncertainty) {
        cout << "not yet supported: Square Root Uncertainty mode" << endl; return false;
    }

    smoothed_data.reserve(in_size);
    double start_stddev = start_sigma, stddev_slope = 0, x_start = in[0].first;
    if (uncertainty_type == LinearUncertainty) {
        double mz_range = in[in_size - 1].first - x_start;
        stddev_slope = (end_sigma - start_sigma) / mz_range;
    }

    const ptrdiff_t count = gaussian_table.size() / 2;
    double std_dev = start_sigma;
    double half_kernel_width = 3 * std_dev;

    for (ptrdiff_t in_idx = 0; in_idx < in_size; in_idx++) {
        std::pair<double, double> in_point = in[in_idx];
        if (in_point.second < NEAR_ZERO) {
            if (in_idx < in_size - 1) {
                std::pair<double, double> in_point2 = in[in_idx + 1];
                if (in_point2.second < NEAR_ZERO) {
                    smoothed_data.push_back(std::pair<int, double>(static_cast<int>(in_idx), 0));
                    in_idx++; smoothed_data.push_back(std::pair<int, double>(static_cast<int>(in_idx), 0));
                    continue;
                }
            }
        }
        double point_x_position = in_point.first;

        if (uncertainty_type == LinearUncertainty) {
            std_dev = start_stddev + (point_x_position - x_start)*stddev_slope;
            half_kernel_width = 3 * std_dev;
        }
        double start_convolution = point_x_position - half_kernel_width;
        double end_convolution = point_x_position + half_kernel_width;
        double count_per_stdev = (count - 1) / std_dev / 6.0;

        // search up for points to convolute and add contribution to point
        double point_y_position = in_point.second*gaussian_table[count];
        for (ptrdiff_t point_idx = in_idx + 1; point_idx < in_size; point_idx++) {
            std::pair<double, double> signal_point = in[point_idx];
            if (signal_point.second > NEAR_ZERO) {
                double cur_x_position = signal_point.first;
                if (cur_x_position > end_convolution)
                    break;
                double kernel_x_position = (cur_x_position - point_x_position)*count_per_stdev; // in kernel space
                const ptrdiff_t kernel_index = count + my_round(kernel_x_position);
                if (kernel_index >= 0 && kernel_index < (int)gaussian_table.size()) {
                    double kernel_value = gaussian_table[kernel_index];
                    point_y_position += signal_point.second*kernel_value;
                }
            }
        }

        // search down for points to convolute and add contribution to point
        for (ptrdiff_t point_idx = in_idx - 1; point_idx >= 0; point_idx--) {
            std::pair<double, double> signal_point = in[point_idx];
            if (signal_point.second > NEAR_ZERO) {
                double cur_x_position = signal_point.first;
                if (cur_x_position < start_convolution)
                    break;
                double kernel_x_position = (cur_x_position - point_x_position)*count_per_stdev; // in kernel space
                const ptrdiff_t kernel_index = count + my_round(kernel_x_position);
                if (kernel_index >= 0 && kernel_index < (int)gaussian_table.size()) {
                    double kernel_value = gaussian_table[kernel_index];
                    point_y_position += signal_point.second*kernel_value;
                }
            }
        }
        smoothed_data.push_back(std::pair<int, double>(static_cast<int>(in_idx), point_y_position));
    }
    return true;
}


// get gaussian value -- only call this once
inline double Centroid::getGaussianValue(double mean, double std_dev, double x_position)
{
    if (std_dev <= 0) { // dirac delta condition
        if (x_position == 0) {
            return 1;
        }
        return 0;
    }
    double variance = std_dev*std_dev;
    double term = x_position - mean;
    double term_squared = term*term;
    double full_exponent = -term_squared / 2 / variance;
    double value = pow(E_VAL, full_exponent);
    //double norm_value = 1 / (sqrt(PI) * std_dev);
    //return value*norm_value;
    return value;
}

// make gaussian table
inline std::vector<double> Centroid::makeUnitGaussianTable(double range, int count)
{
    std::vector<double> gaussian_table;
    if (range <= MIN_GAUSS_RANGE) {
        std::cout << "gaussian template range should be greater than 0.5" << endl; return gaussian_table;
    }
    if (count % 2 == 0) {
        std::cout << "gaussian template count is required to not be even" << endl; return gaussian_table;
    }
    double norm = sqrt(8.0*std::atan(1.0));
    for (double xx = -range; xx <= range; xx += 2 * range / (count - 1)) {
        gaussian_table.push_back(exp(-xx*xx / 2.0) / norm);
    }
    return gaussian_table;
}

// compute max index list
inline std::vector<std::pair<int, double> > Centroid::computeMaxIndexList2(const std::vector<std::pair<double, double> > &data, const std::vector<std::pair<int, double> > &smooth_data)
{
    (void)data;
    std::vector<std::pair<int, double> > index_list;
    if (smooth_data.size() < 2)
        return index_list;

    int prev_index = -1; double prev_sm_intense = 0.0; //, prev_x = 0.0;
    if (smooth_data[0].second > smooth_data[1].second) {
        prev_index = 0; prev_sm_intense = smooth_data[0].second; //prev_x = data[0].first;
        index_list.push_back(std::pair<int, double>(smooth_data[0].first, prev_sm_intense));
    }

    for (int i = 1; i < (int)smooth_data.size() - 1; i++) {
        double b = smooth_data[i].second;
        if (b > smooth_data[i + 1].second) {
            double a = smooth_data[i - 1].second;
            if (a<b) {
                if (prev_index < 0) {
                    prev_index = i; prev_sm_intense = b; //prev_x = data[i].first;
                    index_list.push_back(std::pair<int, double>(smooth_data[i].first, b));
                } else {
                    /*  double dx = data[i].first - prev_x;
                    if (dx > merge_radius){*/
                    prev_index = i; prev_sm_intense = b; //prev_x = data[i].first;
                    index_list.push_back(std::pair<int, double>(smooth_data[i].first, b));
                    /*                    } else {
                    if (b > prev_sm_intense){
                    prev_index = i; prev_sm_intense = b; prev_x = data[i].first;
                    index_list[index_list.size() - 1].first = i;
                    }
                    }*/
                }
            } else if (a == b) {
                /*This is a maxima or just a slant? E.g.
                //   ___       \___
                //  /   \  or      \
        //Find the previous non-equavalent value to determine extrema */
                for (int j = i - 2; j >= 0; j--) {
                    a = smooth_data[j].second;
                    if (a < b) {
                        index_list.push_back(std::pair<int, double>(smooth_data[i].first, b)); //prev_index = i; prev_sm_intense = smooth_data[i]; prev_x = data[i].first;
                        break;
                    } else if (a > b) {
                        break;
                    }
                }
            }
        }
    }

    double a = smooth_data[smooth_data.size() - 2].second;
    double b = smooth_data[smooth_data.size() - 1].second;
    if (b > a) {
        index_list.push_back(std::pair<int, double>(static_cast<int>(smooth_data.size()) - 1, b));
    } else if (b == a) {
        //find the previous non-equavalent value to determine extrema
        for (ptrdiff_t j = static_cast<ptrdiff_t>(smooth_data.size()) - 3; j >= 0; j--) {
            a = smooth_data[j].second;
            if (a < b) {
                index_list.push_back(std::pair<int, double>(smooth_data[smooth_data.size() - 1].first, b));
                break;
            } else if (a > b) {
                break;
            }
        }
    }
    return index_list;
}

// restore intensity
inline void Centroid::restoreIntensity(std::vector<std::pair<double, double> > &centroided_out, const std::vector<std::pair<double, double> > &data, double start_sigma, double end_sigma, const UncertaintyScaling uncertainty_type)
{
    if (uncertainty_type == SquareRootUncertainty) {
        cout << "caution: Square Root Uncertainty mode not yet supported -- using fixed uncertainty mode" << endl;
    }

    double start_stddev = start_sigma, stddev_slope = 0, x_start = data[0].first;
    if (uncertainty_type == LinearUncertainty) {
        double mz_range = data[data.size() - 1].first - x_start;
        stddev_slope = (end_sigma - start_sigma) / mz_range;
    }

    double std_dev = start_sigma;

    unsigned int idx = 0;
    double prv_x = 0, x_pos = 0, nxt_x = centroided_out[0].first;
    for (unsigned int ii = 0; ii<centroided_out.size(); ii++) {
        prv_x = x_pos; x_pos = nxt_x; //double std_dev = 0; 
        if (ii<centroided_out.size() - 1)
            nxt_x = centroided_out[ii + 1].first;
        else
            nxt_x = centroided_out[centroided_out.size() - 1].first + 8.0*sigma_factor;

        if (uncertainty_type == LinearUncertainty) {
            std_dev = start_stddev + (x_pos - x_start)*stddev_slope;
        }

        double kernel_width = std_dev*sigma_factor; // use fixed width for now // std_dev*sigma_factor;
        double start_mz = x_pos - kernel_width, end_mz = x_pos + kernel_width;
        if (prv_x>start_mz)
            start_mz = (start_mz + prv_x) / 2;
        if (nxt_x<end_mz)
            end_mz = (end_mz + nxt_x) / 2;
        while (data[idx].first<x_pos)
            idx++;
        double max1 = data[idx].second, max = max1, last = max; bool prv = false, cur = false;
        double thr = max * intensity_thr;
        for (unsigned int jj = idx + 1; jj<data.size(); jj++) {
            double yy = data[jj].second;
            if (yy <= thr)
                break;
            if (data[jj].first>end_mz)
                break;
            prv = cur; cur = last>yy; last = yy;
            if (prv && cur)
                break;
            if (yy>max) max = yy;
        }
        last = max1; prv = false; cur = false;
        for (int jj = idx - 1; jj >= 0; jj--) {
            double yy = data[jj].second;
            if (yy <= thr)
                break;
            if (data[jj].first<start_mz)
                break;
            prv = cur; cur = last>yy; last = yy;
            if (prv && cur)
                break;
            if (yy>max) max = yy;
        }
        centroided_out[ii].second = max;
    }
}


// make points from index
inline void Centroid::makePointsFromIndex(const std::vector<std::pair<double, double> > &m_pointList, const std::vector<int> &index_list, std::vector<std::pair<double, double> > & points)
{
    points.clear(); points.reserve(index_list.size());
    for (unsigned int i = 0; i < index_list.size(); i++) {
        int index = index_list[i];
        if (index < 0 || index >= int(m_pointList.size())) {
            continue;
        }
        points.push_back(m_pointList[index]);
    }
}
/*
inline void Centroid::makePointsFromIndexWeightedAverageOfNeighbors(const std::vector<std::pair<double, double> > &m_pointList, const std::vector<int> &index_list, std::vector<std::pair<double, double> > & points)
{
    points.clear(); points.reserve(index_list.size());
    for (unsigned int i = 0; i < index_list.size(); i++) {
        int index = index_list[i];
        if (index < 0 || index >= int(m_pointList.size())) {
            continue;
        }

        if (index > 0 && index < int(m_pointList.size()) - 1) {
            std::pair<double, double> left = m_pointList[index - 1];
            std::pair<double, double> mid = m_pointList[index];
            std::pair<double, double> right = m_pointList[index + 1];
            if (left.second < 0) left.second = 0;
            if (mid.second < 0) mid.second = 0;
            if (right.second < 0) right.second = 0;

            double pmin = min(min(left.second, mid.second), right.second);
            double denominator = left.second + mid.second + right.second - 3 * pmin;
            if (denominator > 1e-05){
                //double new_mz = (left.first * left.second + mid.first * mid.second + right.first * right.second) / denominator;
                //mid.first = new_mz;
                //points.push_back(mid);
                double new_mz = (left.first * (left.second - pmin) + mid.first * (mid.second - pmin) + right.first * (right.second - pmin)) / denominator;
                mid.first = new_mz;
                points.push_back(mid);
            } else {
                points.push_back(m_pointList[index]);
            }
        } else {
            points.push_back(m_pointList[index]);
        }
    }
}
*/

// remove duplicate points
inline void Centroid::removeDuplicatePoints(std::vector<std::pair<double, double> > &plist, bool do_sort_by_x, double fudge)
{
    std::vector<std::pair<double, double> > temp_list;
    temp_list.reserve(plist.size());
    if (do_sort_by_x) {
        std::sort(plist.begin(), plist.end(), sort_by_x());
    }
    for (unsigned int i = 0; i < plist.size(); i++) {
        if (i == 0) {
            temp_list.push_back(plist[i]);
        } else {
            double diff = plist[i].first - plist[i - 1].first;
            if (diff <= fudge) {
                std::cout << "points in x values too close, removing later one.  plist[i].x():" << plist[i].first << ", plist[i-1].x():" << plist[i - 1].first << std::endl;
            } else {
                temp_list.push_back(plist[i]);
            }
        }
    }
    swap(plist, temp_list);
    if (plist.size() != temp_list.size()) {
        std::cout << "removed tmplist.size(), plist.size()" << temp_list.size() << "," << plist.size() << std::endl;
    }
    temp_list.clear();
    std::vector<std::pair<double, double> >(temp_list).swap(temp_list);
}

// retain only the top intesity points
inline void Centroid::retainIntensePoints2(std::vector<std::pair<double, double> > &data, std::vector<std::pair<int, double> > &index_list, int top_k, bool sort_mz, std::vector<std::pair<double, double> > &centroided_out)
{
    //(void)sort_mz;
    if (top_k > 0 && top_k < (int)index_list.size()) {
        //std::sort(index_list.rbegin(), index_list.rend(), sort_by_intens());
        std::partial_sort(index_list.begin(), index_list.begin() + top_k, index_list.end(), sort_by_intens());
        index_list.resize(top_k);
        std::sort(index_list.begin(), index_list.end(), sort_by_mz());
    }
    centroided_out.reserve(index_list.size());
    for (unsigned int ii = 0; ii < index_list.size(); ii++) {
        int index = index_list[ii].first;
        if (index > 0 && index < int(data.size()) - 1) {
            std::pair<double, double> left = data[index - 1];
            std::pair<double, double> mid = data[index];
            std::pair<double, double> right = data[index + 1];
            if (left.second < 0) left.second = 0;
            if (mid.second < 0) mid.second = 0;
            if (right.second < 0) right.second = 0;

            double pmin = min(min(left.second, mid.second), right.second);
            double denominator = left.second + mid.second + right.second - 3 * pmin;
            if (denominator > 1e-05){
                //double new_mz = (left.first * left.second + mid.first * mid.second + right.first * right.second) / denominator;
                //mid.first = new_mz;
                //points.push_back(mid);
                double new_mz = (left.first * (left.second - pmin) + mid.first * (mid.second - pmin) + right.first * (right.second - pmin)) / denominator;
                mid.first = new_mz;
                mid.second = index_list[ii].second;
                centroided_out.push_back(mid);
            } else {
                centroided_out.push_back(std::pair<double, double>(data[index].first, index_list[ii].second));
            }
        } else {
            centroided_out.push_back(std::pair<double, double>(data[index].first, index_list[ii].second));
        }
    }
}

// consolidate by merge radius
void Centroid::consolidateMergeRadius(const std::vector<std::pair<double, double> > &data, double merge_radius, std::vector<std::pair<int, double> > &max_list)
{
    std::vector<std::pair<int, double> > sorted_list; sorted_list.reserve(max_list.size());
    for (unsigned int ii = 0; ii < max_list.size(); ii++) {
        sorted_list.push_back(std::pair<int, double>(ii, max_list[ii].second));
    }
    std::sort(sorted_list.begin(), sorted_list.end(), sort_by_intens());

    std::vector<int> remove; remove.resize(max_list.size());
    for (unsigned int ii = 0; ii < sorted_list.size(); ii++) {
        int idxi = sorted_list[ii].first; double vali = sorted_list[ii].second;
        int idi = max_list[idxi].first; double mzi = data[idi].first;
        if (remove[idxi] == 0) {
            for (unsigned int jj = idxi + 1; jj < max_list.size(); jj++) {
                int idj = max_list[jj].first; double mzj = data[idj].first;
                double dx = mzj - mzi;
                if (dx > merge_radius)
                    break;
                if (remove[jj] == 0) {
                    double valj = data[idj].second;
                    if (valj < vali - INTENSITY_THR)
                        remove[jj] = 1;
                }
            }
            for (int jj = idxi - 1; jj >= 0; jj--) {
                int idj = max_list[jj].first; double mzj = data[idj].first;
                double dx = mzi - mzj;
                if (dx > merge_radius)
                    break;
                if (remove[jj] == 0) {
                    double valj = data[idj].second;
                    if (valj < vali - INTENSITY_THR)
                        remove[jj] = 1;
                }
            }
        }
    }
    std::vector<std::pair<int, double> > updated_list;
    for (unsigned int ii = 0; ii < max_list.size(); ii++) {
        if (remove[ii] == 0) {
            updated_list.push_back(max_list[ii]);
        }
    }
    max_list.clear();
    max_list.swap(updated_list);
}

// always rounding midpoint up (1.3->1, 1.5->2, 1.6->2) for positive val
inline int Centroid::my_round(double val)
{
    return (int)(val + .5);
}

#ifdef _MSC_VER
_PICO_END // visual studio only
#else
} //pmi // needed for QT with pwiz
#endif
