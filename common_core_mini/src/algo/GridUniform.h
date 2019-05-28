#ifndef GRIDUNIFORM_H
#define GRIDUNIFORM_H

#include <QVector>
#include <QDebug>
#include <common_errors.h>
#include <math_utils.h>
#include <pmi_common_core_mini_export.h>

_PMI_BEGIN

class PlotBase;

struct PMI_COMMON_CORE_MINI_EXPORT GridUniform
{
    double start_x=0;
    double scale_x=1;

    QVector<double> y_array;

    void clear() {
        y_array.clear();
        start_x = 0;
        scale_x = 1;
    }

    int size() const {
        return y_array.size();
    }

    //TODO: bool tooLowX
    int tooLow(double x) const {
        return(x < start_x);
    }

    //TODO: bool tooHighX
    int tooHigh(double x) const {
        return(x > start_x + (y_array.size()-1)*scale_x);
    }

    //TODO: rename iy to indexToWorldY, ix to indexToWorldX
    double iy(int index) const {
        if (index < 0 || index >= y_array.size())
            return(0);
        return y_array[index];
    }

    double ix(int index) const {
        return (double) index * scale_x + start_x;
    }

    //TODO: rename to setAtIndexY
    void setY(int index, double value) {
        if (index < 0 || index >= y_array.size())
            return;
        y_array[index] = value;
    }

    //TODO: getWorldStartX
    double getStartX() const {
        return start_x;
    }

    //TODO: getWorldStepX
    double getScaleX() const {
        return scale_x;
    }

    //TODO: getYFromWorldX
    double getYFromX(double x) const {
        //convert x to index and get y value
        double fx = (x - start_x) / scale_x;
        int indx = int(fx); // next integer to the left
        if (indx > fx) {
            indx = indx-1;
        }
        double fup = fx - indx;
        double f1 = 1.0 - fup;
        double yval = f1*iy(indx) + fup*iy(indx+1) ;
        return yval;
    }

    //addYFromWorldX
    void addYFromX(double x, double y) {
        //convert x to index and get y value
        double fx = (x - start_x) / scale_x;
        int indx = int(fx); // next integer to the left
        if (indx > fx) {
            indx = indx-1;
        }
        double fup = fx - indx;
        double f1 = 1.0 - fup;
        if (indx + 2 < y_array.size() && indx >= 1) {
            y_array[indx] += f1*y;
            y_array[indx+1] += fup*y;
            // this helps prevent gaps in M spectrum at high charge
            y_array[indx-1] += 0.5*f1*y;
            y_array[indx+2] += 0.5*fup*y;
        }
    }


    void setAllYTo(double val) {
        for (int i = 0; i < y_array.size(); i++) {
            y_array[i] = val;
        }
    }

    /*!
     * \brief apply gaussian smoothing
     * \param sigma unit is world space (e.g. Dalton)
     */
    void smooth(double sigma);

    /*!
     * \brief apply mexican hat smoothing
     * \param sigma unit is world space (e.g. Dalton)
     * \param sigma2 unit is world space (e.g. Dalton)
     */
    void smooth_mex_hat(double sigma, double sigma2, double weightOfTwo);

    void normalize(double new_max) {
        if (y_array.size() <= 0)
            return;
        double max_val = y_array[0];
        for (int i = 0; i < y_array.size(); i++) {
            if (y_array[i] > max_val) {
                max_val = y_array[i];
            }
        }
        double wt = new_max / max_val;
        if (wt > 0.0) {
            for (int i = 0; i < y_array.size(); i++) {
                y_array[i] = wt*y_array[i];
            }
        }
    }

    double getMaximumYValue() const {
        double val = 0;
        for (int i = 0; i < y_array.size(); i++) {
            if (y_array[i] > val)
                val = y_array[i];
        }
        return val;
    }

    double getSum() const {
        double sum = 0;
        for(int i = 0; i < y_array.size(); i++) {
           sum+=y_array[i];
        }
        return sum;
    }

    enum ResampleMethod {
         ResampleMethod_LowerResolutionNotResampled  //if this is of lower sample, we won't resample instead return same size as this.
        ,ResampleMethod_ResampleExactly //regardless of this, resample
    };

    void createReample(ResampleMethod method, int size, GridUniform * gridOut) const;

    void applyStopList(double stoop_tolerance, const QVector<double> & stoplist);

    enum SaveCSVOption {
         SaveCSVOption_SaveXY
        ,SaveCSVOption_SaveYOnly
    };

    Err saveTofile(const QString & filename, SaveCSVOption option = SaveCSVOption_SaveYOnly) const;

    Err loadFromFile(const QString & filename);

    void makePlot(PlotBase * plot) const;

    void makeCentroidedPlot(PlotBase * plot, int topK, double minimumDistance) const;

    double getNoiseSigma_Slow(float fraction) const;
    double getNoiseSigma(float fraction) const;

    Err initGridByMzBinSpace(double start_x, double end_x, double mz_bin_space);

    Err initGridBySize(double x_start, double x_end, int size);

    enum ArithmeticType { ArithmeticType_Add, ArithmeticType_Subtract };

    void accumulate_binarySearch(const PlotBase & plot, ArithmeticType type = ArithmeticType_Add);

    void accumulate(const PlotBase & plot, ArithmeticType type = ArithmeticType_Add);

    void accumulate_with_xlist(const PlotBase & plot, const std::vector<double> & xs, ArithmeticType type = ArithmeticType_Add);

    void toByteArray_float(QByteArray & ba) const;
    void fromByteArray_float(const QByteArray & ba);

    Err accumulate(const GridUniform & grid, ArithmeticType type = ArithmeticType_Add);

private:
    struct CacheXList {
        bool isCacheValid(const GridUniform & grid) const;
        void makeCache(const GridUniform & grid);

        double start_x=0;
        double scale_x=1;
        std::vector<double> m_cachedXList;
        std::vector<double> m_cachedEvaluatedValues;
    };
    CacheXList m_cache;
};

/*
inline QDebug operator<< (QDebug d, const GridUniform &grid) {
    double sum = grid.getSum();
    d << "GridUniform Information:";
    double min_val = 0, max_val=0;
    for(int i = 0; i < grid.y_array.size(); i++) {
        if (i == 0) {
            min_val = grid.y_array[i];
            max_val = grid.y_array[i];
        } else {
            min_val = pmi::Min(min_val, grid.y_array[i]);
            max_val = pmi::Max(max_val, grid.y_array[i]);
        }
    }

    d << "start_x=" << grid.start_x << ", scale_x=" << grid.scale_x << ", size=" << grid.size() << ", sum,min,max =" << sum << "," << min_val << "," << max_val;
    if (sum == 0) {
        d << "Warning: Sum of all y values was zero";
    }

    return d;
}
*/

_PMI_END

#endif // GRIDUNIFORM_H
