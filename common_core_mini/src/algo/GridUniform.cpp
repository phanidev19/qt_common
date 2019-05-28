#include "GridUniform.h"
#include "Convolution1dCore.h"
#include "CsvReader.h"
#include "PlotBase.h"
#include "pmi_common_core_mini_debug.h"

#include <math_utils.h>

#include <QFile>
#include <QTextStream>

_PMI_BEGIN

const QChar FIELD_SEPARATOR_CHAR = ',';

static Err
makeFile(QString filename, const std::vector<double> & list)
{
    Err e = kNoErr;
    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly)) {
        e = kFileOpenError; ree;
    }

    QTextStream out(&file);

    for (int i = 0; i < (int)list.size(); i++) {
        out << list[i] << endl;
    }

    return e;
}

static double makeGaussian(double mean, double std_dev, double x)
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

static bool makeMexicanHatKernel(double std_dev, double std_dev2, double weight_of_2, std::vector<double> & vkrn)
{
    if (std_dev2 < std_dev) {
        debugCoreMini() << "error in usage std_dev,std_dev2=" << std_dev << "," << std_dev2;
        std::swap(std_dev,std_dev2);
    }
    if (weight_of_2 >= 1) {
        debugCoreMini() << "weight_of_2 too weight_of_2=" << weight_of_2 << "," << "changed to 0.9";
        weight_of_2 = 0.9;
    }

    int width = int(3.0 * std_dev2 + 1 - 1e-16);
    vkrn.clear();

    double x;
    double value,value2;
    double sumOfElems = 0;
    std::vector<double> nonNormalvkrn;
    for(int i=0; i<2*width+1; i++){
        x = (i-width);
        value = makeGaussian(0.0, std_dev, x);
        value2 = makeGaussian(0.0, std_dev2, x)* weight_of_2;
        double val_final = value - value2;
        nonNormalvkrn.push_back(val_final);
        sumOfElems += val_final;
    }
    for (int i=0; i<2*width+1; i++){
        vkrn.push_back(nonNormalvkrn[i]/sumOfElems);                         // noramlize descrete values from distribution
    }

    return true;                                                   // need come up with a valid successs condition
}



void GridUniform::smooth(double std_dev_units) {
    std::vector<double> in = y_array.toStdVector();
    std::vector<double> kernel;
    std::vector<double> out;

    double std_dev = std_dev_units / scale_x;
    core::makeGaussianKernel(std_dev, kernel);
    core::conv(in, kernel, kBoundaryTypeZero, out);

    y_array = QVector<double>::fromStdVector(out);
}

void GridUniform::smooth_mex_hat(double std_dev_units, double std_dev_units2, double weightOfTwo) {
    std::vector<double> in = y_array.toStdVector();
    std::vector<double> kernel;
    std::vector<double> out;

    double std_dev = std_dev_units / scale_x;
    double std_dev2 = std_dev_units2 / scale_x;
    double weight_of_2 = weightOfTwo;
    makeMexicanHatKernel(std_dev, std_dev2, weight_of_2, kernel);

    core::conv(in, kernel, kBoundaryTypeZero, out);

    y_array = QVector<double>::fromStdVector(out);
}

void GridUniform::applyStopList(double stopTolerance, const QVector<double> & _stopMassList) {

    if (stopTolerance < scale_x) {
        stopTolerance = scale_x;
    }
    QVector<double> stopMassList = _stopMassList;
    std::sort(stopMassList.begin(), stopMassList.end());

    double replacement_value = 0.0;
    double x = start_x;
    int j = 0;
    for (int i = 0; i < y_array.size(); i++) {
        // Get stop_mass for this x value  (assumes stop masses are sorted and at least 4*stop_tolerance apart)
        if (stopMassList[j] < x - stopTolerance) {
            j++;
        }
        if (j >= stopMassList.size()) {
            break;
        }
        // If just before a stop mass pick up replacement_value
        if (x < stopMassList[j] - stopTolerance && x + scale_x >= stopMassList[j] - stopTolerance) {
            replacement_value = y_array[i];
        }
        // Do the replacement
        if (x > stopMassList[j] - stopTolerance && x <= stopMassList[j] + stopTolerance) {
            y_array[i] = replacement_value;
        }
        x += scale_x;
    }
}



Err GridUniform::saveTofile(const QString & filename, SaveCSVOption option) const {
    Err e = kNoErr;

    QFile file(filename);
    if(!file.open(QIODevice::WriteOnly)) {
        e = kFileOpenError; ree;
    }

    QTextStream out(&file);
    out.setRealNumberPrecision(10);
    // First line is double start_x, double scale_x
    out << start_x << "," << scale_x << endl;
    for (int i = 0; i < y_array.size(); i++) {
        if (option == SaveCSVOption_SaveXY) {
            out << ix(i) << "," << iy(i) << endl;
        } else {
            out << iy(i) << endl;
        }
    }

    return e;
}

// Reads in a file written by saveTofile
Err GridUniform::loadFromFile(const QString &filename)
{
    CsvReader reader(filename);

    reader.setFieldSeparatorChar(FIELD_SEPARATOR_CHAR);

    if (!reader.open()) {
        rrr(kFileOpenError);
    }

    if (!reader.hasMoreRows()) {
        warningCoreMini() << "Empty file" << filename;

        rrr(kError);
    }

    // First line is double start_x, double scale_x, formatted as: start_x,scale_x
    {
        const QStringList startScaleRow = reader.readRow();
        bool ok = true;

        if (startScaleRow.size() == 2) {
            bool ok1 = true;
            bool ok2 = true;

            start_x = startScaleRow[0].toDouble(&ok1);
            scale_x = startScaleRow[1].toDouble(&ok2);

            ok = ok1 && ok2;
        } else {
            // unexpected format
            ok = false;
        }

        if (!ok) {
            warningCoreMini() << "There was a problem with reading GridUniform from file"
                              << filename;

            rrr(kError);
        }
    }

    if (scale_x <= 0.0) {
        rrr(kBadParameterError);
    }

    debugCoreMini() << "Clearing y_array from GridUniform to read a new one from file" << filename;

    y_array.clear();

    {
        // 2 is picked for human readability since line number starts from 1 in text editors
        int lineCount = 2;

        while (reader.hasMoreRows()) {
            const QStringList row = reader.readRow();
            bool ok = true;

            switch (row.size()) {
            case 0:
                // skip empty rows
                debugCoreMini() << "Skip empty row in file at line" << filename << "," << lineCount;

                break;
            case 1:
                y_array.push_back(row[0].toDouble(&ok));

                break;
            case 2:
                y_array.push_back(row[1].toDouble(&ok));

                break;
            default:
                // unexpected format
                ok = false;

                break;
            }

            if (!ok) {
                warningCoreMini()
                    << "There was a problem with reading GridUniform from file at line" << filename
                    << "," << lineCount;

                rrr(kError);
            }

            ++lineCount;
        }
    }

    debugCoreMini() << "GridUniform read from file" << filename;

    return kNoErr;
}

void GridUniform::makePlot(PlotBase * plot) const {
    if (!plot) {
        debugCoreMini() << "plot is null";
        return;
    }
    point2dList & pointList = plot->getPointList();
    pointList.clear();

    point2d obj;
    //TODO: for some reason the last value has #INF value.
    //See https://proteinmetrics.atlassian.net/browse/PMI-395
    //To avoid it, we won't use the last value with -1.
    int y_size = y_array.size() - 1;
    if (y_size <= 0)
        return;

    pointList.reserve(y_size);
    for (int i = 0; i < y_size; i++) {
        obj.rx() = ix(i);
        obj.ry() = iy(i);
        pointList.push_back(obj);
    }
    //Set the flag since we know that the x values are in increasing order.
    plot->setIsSortedAscendingX_noErrorCheck(true);
}

static void keepTopKMostIntensePoints_avoidClosePoints(PlotBase & plot, int topk, double minimumDistance) {
    debugCoreMini() << "<< plot.getPointListSize(), topk" << plot.getPointListSize() << "," << topk;
    if (plot.getPointListSize() > topk) {
        plot.sortPointListByY(false);
        point2dList & points = plot.getPointList();
        point2dList newPoints;
        for (int i = 0; i < (int)points.size(); i++) {
            if ((int)newPoints.size() >= topk) {
                break;
            }
            pmi::SortPointListByX(newPoints, true);
            double dist = pmi::distanceToClosestPoint(newPoints, points[i].x());
            /*
            debugCoreMini() << "i,dist,x" << i << "," << dist << "," << points[i].x();
            for (int j = 0; j < newPoints.size(); j++) {
                debugCoreMini() << newPoints[j] << " dist=" << newPoints[j].x() - points[i].x();
            }
            debugCoreMini() << "-----";
            //pmi::distanceToClosestPoint(newPoints, points[i].x());
            */
            if (dist > 0 && dist < minimumDistance) {
                DEBUG_WARNING_LIMIT(debugCoreMini() << "ignoring point because it's too close. i,dist,x" << i << "," << dist << "," << points[i].x(), 10);
                continue;
            }
            newPoints.push_back(points[i]);
        }
        points = newPoints;
        plot.sortPointListByX(true);
    }
}

void GridUniform::makeCentroidedPlot(PlotBase * outCentroidedPlot, int topK, double minimumDistance) const {
    if (!outCentroidedPlot) {
        debugCoreMini() << "outCentroidedPlot is null";
        return;
    }
    GridUniform tmp = *this;
    double noise_percentile = tmp.getNoiseSigma(0.25);
    double y_intensity_cutoff = noise_percentile * 4;
    double y_intensity_is_noise_cutoff = noise_percentile * 10;
    PlotBase centroidedPlot;
    outCentroidedPlot->clear();

    PlotBase smoothedPlot;

    tmp.smooth(2);  // 10 Dalton sigma
    tmp.makePlot(&smoothedPlot);
    double max_y_value = tmp.getMaximumYValue();

    if (max_y_value < y_intensity_is_noise_cutoff) {
        debugCoreMini() << "This is considered noise M since max_y_value<y_intensity_is_noise_cutoff, max_y_value,y_intensity_is_noise_cutoff,noise_percentile=" << max_y_value << "," << y_intensity_is_noise_cutoff << "," << noise_percentile;
        return;
    }

    smoothedPlot.makeCentroidedPoints(&centroidedPlot.getPointList());
    keepTopKMostIntensePoints_avoidClosePoints(centroidedPlot, topK, minimumDistance);

    //push y-value back up to match original signal
    point2dList & plist = centroidedPlot.getPointList();
    for (int i = 0; i < (int)plist.size(); i++) {
        point2d p = plist[i];
        p.ry() = getYFromX(p.x());
        if (p.y() > y_intensity_cutoff) {
            outCentroidedPlot->addPoint(p);
        }
    }
}


double GridUniform::getNoiseSigma_Slow(float fraction) const
{
    QVector<double> temp = y_array;
    qSort(temp.begin(), temp.end());

    // Quartile point (bigger than 25% of M values) is assumed to be noise
    //  Use this to initialize chargeVec values
    //  This spreads noise m/z blocks all over the M spectrum for smooth background

    double noise_percentile = 0.00001;
    if (fraction < 0) {
        fraction = 0;
    }
    int index = temp.size() * fraction;
    if (temp.size() > 0 && index < temp.size()) {
        noise_percentile = temp[index];
    }
    return noise_percentile;
}

double GridUniform::getNoiseSigma(float fraction) const
{
    QVector<double> temp = y_array;

    // Quartile point (bigger than 25% of M values) is assumed to be noise
    //  Use this to initialize chargeVec values
    //  This spreads noise m/z blocks all over the M spectrum for smooth background

    double noise_percentile = 0.00001;
    if (fraction < 0) {
        fraction = 0;
    }
    int index = temp.size() * fraction;
    if (temp.size() > 0 && index < temp.size()) {
        //http://apiexamples.com/cpp/algorithm/nth_element.html
        std::nth_element(temp.begin(), temp.begin() + index, temp.end());
        noise_percentile = temp[index];

        /*
        double xx = getNoiseSigma_Slow(fraction);
        if (xx != noise_percentile) {
            debugCoreMini() << "ekk";
        }
        else {
            debugCoreMini() << "Matches with " << xx;
        }
        */
    }
    return noise_percentile;
}

Err GridUniform::initGridBySize(double _start_x, double _end_x, int size) {
    clear();
    if (size <= 0 || _start_x >= _end_x) {
        rrr(kBadParameterError);
    }

    start_x = _start_x;
    scale_x = (_end_x - _start_x)/size;
    y_array.resize(size);
    return kNoErr;
}

Err GridUniform::initGridByMzBinSpace(double x_start, double x_end, double mz_bin_space) {
   Err e = kNoErr;

   clear();

   if (mz_bin_space <= 0 || x_start >= x_end) {
       rrr(kBadParameterError);
   }
   start_x = x_start;
   scale_x = mz_bin_space;

   int size = (x_end-x_start)/mz_bin_space + 1;
   y_array.resize(size);
   /*
   0,0.1,0.1 -> size = 2
   for (double x = x_start; x <= x_end; x += mz_bin_space) {
       y_array.push_back(0);
   }
   */

//error:
   return e;
}

void GridUniform::accumulate_binarySearch(const PlotBase & plot, ArithmeticType type)
{
    for (int i = 0; i < size(); i++) {
        double x = ix(i);  // converting from index space to world space; i.e. from i to m/z (which is x)
        double yy = plot.evaluate(x);
        if (type == ArithmeticType_Add) {
            y_array[i] = y_array[i] + yy;
        }
        else {
            y_array[i] = y_array[i] - yy;
        }
    }
}


bool GridUniform::CacheXList::isCacheValid(const GridUniform & grid) const
{
    if (grid.start_x == start_x
        && grid.scale_x == scale_x
        && grid.y_array.size() == m_cachedXList.size() ) {
        return true;
    }
    debugCoreMini() << "invalid cache. start_x,scale_x,y_array:\n"
                << start_x << "," << scale_x << "," << m_cachedXList.size() << " vs\n "
                << grid.start_x << "," << grid.scale_x << "," << grid.y_array.size();

    return false;
}

void GridUniform::CacheXList::makeCache(const GridUniform & grid)
{
    int size = grid.y_array.size();
    m_cachedXList.resize(size);

    start_x = grid.start_x;
    scale_x = grid.scale_x;
    for (int i = 0; i < size; i++) {
        double x = grid.ix(i);  //converting from index space to world space; i.e. from i to m/z (which is x)
        m_cachedXList[i] = x;
    }
}

void GridUniform::accumulate(const PlotBase & plot, ArithmeticType type)
{
    if (!m_cache.isCacheValid(*this)) {
        m_cache.makeCache(*this);
    }

    plot.evaluate_linear(m_cache.m_cachedXList, &m_cache.m_cachedEvaluatedValues);

    for (int i = 0; i < (int)m_cache.m_cachedEvaluatedValues.size(); i++) {
        double yy = m_cache.m_cachedEvaluatedValues[i];
        double & y_array_at_idx = y_array[i];
        if (type == ArithmeticType_Add) {
            y_array_at_idx = y_array_at_idx + yy;
        }
        else {
            y_array_at_idx = y_array_at_idx - yy;
        }
    }
}

void GridUniform::accumulate_with_xlist(const PlotBase & plot, const std::vector<double> & xs, ArithmeticType type)
{
    std::vector<double> evaluated;

    plot.evaluate_linear(xs, &evaluated);

    for (int i = 0; i < (int)evaluated.size(); i++) {
        double yy = evaluated[i];
        double & y_array_at_idx = y_array[i];
        if (type == ArithmeticType_Add) {
            y_array_at_idx = y_array_at_idx + yy;
        }
        else {
            y_array_at_idx = y_array_at_idx - yy;
        }
    }
}

Err GridUniform::accumulate(const GridUniform & grid, ArithmeticType type)
{
    if (grid.y_array.size() != this->y_array.size()
            || !fsame(grid.start_x, start_x, 1e-10)
            || !fsame(grid.scale_x, scale_x, 1e-10) )
    {
        //TODO: allow interpolation; until then, return error
        rrr(kBadParameterError);
    }
    if (type == ArithmeticType_Add) {
        for (int i = 0; i < y_array.size(); i++) {
            y_array[i] += grid.y_array[i];
        }
    } else {
        for (int i = 0; i < y_array.size(); i++) {
            y_array[i] -= grid.y_array[i];
        }
    }
    return kNoErr;
}

void GridUniform::toByteArray_float(QByteArray & ba) const
{
    ba.resize(sizeof(float)*y_array.size());
    float * farray = (float*) ba.data();
    for (int i = 0; i < y_array.size(); i++) {
        farray[i] = y_array[i];
    }
}

void GridUniform::fromByteArray_float(const QByteArray & ba)
{
    int size = ba.size() / sizeof(float);
    y_array.resize(size);
    const float * farray = (float*) ba.data();
    for (int i = 0; i < size; i++) {
        y_array[i] = farray[i];
    }
}

void GridUniform::createReample(ResampleMethod method, int newSize, GridUniform * gridOut) const
{
    if (method == ResampleMethod_LowerResolutionNotResampled || newSize <= 0) {
        if (newSize >= this->y_array.size()) {
            *gridOut = *this;
            return;
        }
    }

    double start_x = this->ix(0);
    double end_x = this->ix(size()-1);
    gridOut->initGridBySize(start_x, end_x, newSize);
    for (int i = 0; i < gridOut->size(); i++) {
        double x_new = gridOut->ix(i);
        double y_new = getYFromX(x_new);
        gridOut->setY(i, y_new);
    }
}

//class GridUniformTest {
//public:
//    GridUniformTest() {
//
//    }
//
//    Err testSaveAndLoad() {
//        Err e = kNoErr;
//        GridUniform grid;
//        grid.
//
//        return e;
//    }
//};

_PMI_END
