#include <QtTest>
#include <random>
#include <time.h>

#include "GridUniform.h"
#include "PlotBase.h"

_PMI_BEGIN
class GridUniformTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void Accumulate_ReturnsSameResultMoreQuicklyThanOldAccumulate_GivenSamePlotBase();
};

bool point2d_unique(point2d x, point2d y) {
    return x.x() == y.x();
}

void GridUniformTest::Accumulate_ReturnsSameResultMoreQuicklyThanOldAccumulate_GivenSamePlotBase() {
    const int TEN_THOUSAND = 10000;
    const int HUNDRED_THOUSAND = TEN_THOUSAND * 10;

    const double RAND_DISTR_MIN = -TEN_THOUSAND;
    const double RAND_DISTR_MAX = TEN_THOUSAND;

    const int NUM_POINTS_POINTLIST = 2000;

    const int TEN_MILLION = 10000000;
    const int NUM_GRID_POINTS = TEN_MILLION;
    
    const int GRID_X_MIN = -TEN_THOUSAND * 1.1;
    const int GRID_X_MAX = TEN_THOUSAND * 1.1;

    PlotBase pb;
    
    // seed random engine with 1
    std::mt19937 eng(1);
    std::uniform_real_distribution<> distr(RAND_DISTR_MIN, RAND_DISTR_MAX);
    
    for (int i = 0; i < NUM_POINTS_POINTLIST; i++) {
        double randB = distr(eng);
        // double randB = distr(eng);
        double A = i;
        pb.addPoint(point2d(A, randB));
    }

    pb.sortPointListByX();

    point2dList & pbPointList = pb.getPointList();

    auto last = std::unique(pbPointList.begin(), pbPointList.end(), point2d_unique);
    pbPointList.erase(last, pbPointList.end());

    clock_t old_start, old_end, new_start, new_end;

    GridUniform grid_old, grid_new;

    grid_old.initGridBySize(0, 2000, NUM_GRID_POINTS);
    grid_new.initGridBySize(0, 2000, NUM_GRID_POINTS);

    const int ITERATION_NUMBER = 1;

    old_start = clock();
    for (int i = 0; i < ITERATION_NUMBER; i++) {
        grid_old.accumulate_binarySearch(pb, pmi::GridUniform::ArithmeticType_Add);
    }
    old_end = clock();

    // this code is used if experimental accumulate_two is used (because it requires the Xs list to be pre populated
    std::vector<double> xs;
    xs.reserve(grid_new.size());
    for (int i = 0; i < grid_new.size(); i++) {
        double x = grid_new.ix(i);  //converting from index space to world space; i.e. from i to m/z (which is x)
        xs.push_back(x);
    }

    new_start = clock();
    for (int i = 0; i < ITERATION_NUMBER; i++) {
        grid_new.accumulate_with_xlist(pb, xs, pmi::GridUniform::ArithmeticType_Add);
    }
    new_end = clock();

    double old_run_time_seconds = ((double)old_end - (double)old_start) / CLOCKS_PER_SEC;
    double new_run_time_seconds = ((double)new_end - (double)new_start) / CLOCKS_PER_SEC;

    qDebug() << "old run time (seconds)=" << old_run_time_seconds << " new_run_time_seconds=" << new_run_time_seconds << " ratio=" << old_run_time_seconds / new_run_time_seconds;

    QCOMPARE(grid_old.size(), grid_new.size());

    QCOMPARE(old_run_time_seconds < new_run_time_seconds, false);

    for (int i = 0; i < grid_old.size(); i++) {
        QCOMPARE(grid_old.y_array[i], grid_new.y_array[i]);
    }
}

_PMI_END

QTEST_GUILESS_MAIN(pmi::GridUniformTest)

#include "GridUniformTest.moc"
