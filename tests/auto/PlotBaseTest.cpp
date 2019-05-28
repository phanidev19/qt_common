#include <QtTest>

#include "PlotBase.h"
#include <iostream>
#include <fstream>
// setprecision
#include <iomanip>
#include <random>
#include <time.h>

_PMI_BEGIN

// If defined, this will enable tests that take several minutes to finish
// #define PMI_TEST_SLOW

#define QCOMPARE_KEL(actual, expected, desc) \
do {\
    if (!QTest::qCompare(actual, expected, #actual, desc, __FILE__, __LINE__))\
        return;\
} while (0)

const int POINT_COUNT = 10;

class PlotBaseTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:

    void testCreate();
    void testEvaluate();

    void testEvaluateSlowSimple();

    void testEvaluateLinearOutOfBoundsLeft();
    void testEvaluateLinearOutOfBoundsRight();
    void testEvaluateLinearExactlyOnPoints();
    void testEvaluateLinearBetweenTwoPositivePoints();

    void testGetIndexLessOrEqual();

    void EvaluateLinear_ReturnsSameValuesAsEvaluateSlow_GivenSamePointsAsEvaluateSlow();
    void EvaluateLinear_Works_TempTest();

    void EvaluateLinear_ReturnsCorrectValues_GivenSameEvaluationPointMultipleTimes();
    void EvaluateLinear_ReturnsCorrectValues_GivenPointListWithDuplicates();
    void EvaluateLinear_ReturnsCorrectValues_GivenSameEvaluationPointMultipleTimesAndPointListWithDuplicates();

    void EvaluateLinear_ReturnsCorrectValues_GivenEvalValsListInitializedWithGarbageValues();

    void EvaluateLinear_ReturnsAllZeroes_GivenEvalValsInitializedWithGarbageValuesAndStartEvalOutOfBoundsRight();

    void GetIndexLessThanValLinear_ReturnsCorrectIndex_GivenExactXvals();
    void GetIndexLessThanValLinear_ReturnsCorrectIndex_GivenNormalVals();
    void GetIndexLessThanValLinear_ReturnsLastIndex_GivenOutOfBoundsRight();
    void GetIndexLessThanValLinear_ReturnsNegativeOne_GivenOutOfBoundsLeft();
    void GetIndexLessThanValLinear_ReturnsCorrectIndices_GivenStartIdx();
    void GetIndexLessThanValLinear_ReturnsCorrectIndices_GivenInvalidStartIndices();
    void GetIndexLessThanValLinear_ReturnsCorrectIndices_GivenPointListWithDuplicates();

    void testCentroidData();

    void testResizePointListToLower_data();
    void testResizePointListToLower();

    void testResizePointListToHigher_data();
    void testResizePointListToHigher();

    void testSwap();

#ifdef PMI_TEST_SLOW
    void EvaluateLinear_ReturnsSameValuesAsEvaluateSlow_GivenSameMillionRandomPointsAsEvaluateSlow();
    void testEvaluateSlow();
#endif
};

void PlotBaseTest::testCreate()
{
    point2dList points;
    for (int i = 0; i < POINT_COUNT; i++) {
        points.push_back(QPointF(i, i*i));
    }

    PlotBase base(points);
    QCOMPARE(base.getPointListSize(), POINT_COUNT);
    QVERIFY(base.name().isNull());
}

void PlotBaseTest::testEvaluateSlowSimple()
{
    // create some data to interpolate
    point2dList points; // [0,1,2,...]
    for (int i = 0; i < 10; ++i) {
        points.push_back(QPointF(i, sin(i)));
    }
    PlotBase pb(points);

    // create list of interesting x values (values in between 0.5, 1.5, 2.5, ...)
    std::vector<double> xs;
    for (size_t i = 0; i < points.size(); ++i) {
        qreal x = points.at(i).x() + 0.5;
        xs.push_back(x);
    }

    std::vector<double> ys;
    pb.evaluate_slow(xs, &ys);
    
    QVERIFY(xs.size() == ys.size());
    // verify that the values are same as with PlotBase::evaluate
    for (size_t i = 0; i < xs.size(); ++i) {
        qreal x = xs.at(i);
        qreal y = pb.evaluate(x);
        QCOMPARE(ys.at(i), y);
    }

}

void PlotBaseTest::testGetIndexLessOrEqual()
{
    point2dList data = {
        QPointF(390.0, 0),
        QPointF(399.0, 0),
        QPointF(410.5, 0)
    };

    
    pt2idx index = GetIndexLessOrEqual(data, 410, false);
    QCOMPARE(index, 1);
}

void pmi::PlotBaseTest::testEvaluate()
{
    PlotBase pb;
    pb.addPoint(point2d(0,0));
    pb.addPoint(point2d(10, 100));

    QCOMPARE(pb.evaluate(0), 0.0);
    QCOMPARE(pb.evaluate(1.5), 15.0);
    QCOMPARE(pb.evaluate(10), 100.0);
}

void pmi::PlotBaseTest::testEvaluateLinearOutOfBoundsLeft()
{
    PlotBase pb;
    pb.addPoint(point2d(0,100));
    pb.addPoint(point2d(100,200));

    std::vector<double> xs, evaluatedValues;
    xs.push_back(-100000000);
    xs.push_back(-100);
    xs.push_back(-0.00001);
    pb.evaluate_linear(xs, &evaluatedValues);

    for (unsigned i = 0; i < evaluatedValues.size(); i++) {
        QCOMPARE(evaluatedValues.at(i), 0.0);
    }
}

void pmi::PlotBaseTest::testEvaluateLinearOutOfBoundsRight()
{
    PlotBase pb;
    pb.addPoint(point2d(0,100));
    pb.addPoint(point2d(100,200));

    std::vector<double> xs, evaluatedValues; 
    xs.push_back(100.0001);
    xs.push_back(200);
    xs.push_back(100000000);
    pb.evaluate_linear(xs, &evaluatedValues);

    for (unsigned i = 0; i < evaluatedValues.size(); i++) {
        QCOMPARE(evaluatedValues.at(i), 0.0);
    }
}

void pmi::PlotBaseTest::testEvaluateLinearExactlyOnPoints()
{
    //evaluate_linear(const std::vector<double> &xs, std::vector<double> * values
    PlotBase pb;

    double y_valA = 100, y_valB = 125.32, y_valC = 200.249834;
    double x_valA = 0.0001, x_valB = 125, x_valC = 300.209384;

    pb.addPoint(point2d(x_valA, y_valA));
    pb.addPoint(point2d(x_valB, y_valB));
    pb.addPoint(point2d(x_valC, y_valC));

    std::vector<double> xs, evaluatedValues;
    xs.push_back(x_valA);
    xs.push_back(x_valB);
    xs.push_back(x_valC);
    pb.evaluate_linear(xs, &evaluatedValues);

    QCOMPARE(evaluatedValues.at(0), y_valA);
    QCOMPARE(evaluatedValues.at(1), y_valB);
    QCOMPARE(evaluatedValues.at(2), y_valC);
}

void pmi::PlotBaseTest::testEvaluateLinearBetweenTwoPositivePoints() 
{
    PlotBase pb;

    pb.addPoint(point2d(100, 100));
    pb.addPoint(point2d(200, 200));

    std::vector<double> xs, evaluatedValues;
    double step = 0.01;
    for (double i = 100.0; i < 200.0; i += step) {
        xs.push_back(i);
    }

    pb.evaluate_linear(xs, &evaluatedValues);

    for (unsigned i = 0; i < xs.size(); i++) {
        QCOMPARE(evaluatedValues.at(i), i * step + 100);
    }
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsSameValuesAsEvaluateSlow_GivenSamePointsAsEvaluateSlow() {
    PlotBase pb;

    pb.addPoint(point2d(-100, 30.54));
    pb.addPoint(point2d(13.43, 289.54));
    pb.addPoint(point2d(100, 145.125));
    pb.addPoint(point2d(150, -151.12));
    pb.addPoint(point2d(200, 565.1435));

    std::vector<double> xs, evaluatedValuesLinear, evaluatedValuesSlow;

    const double x_start = -125.0, x_end = 225.0, step = 0.01;

    for (double i = x_start; i < x_end; i += step) {
        xs.push_back(i);
    }

    pb.evaluate_linear(xs, &evaluatedValuesLinear);
    pb.evaluate_slow(xs, &evaluatedValuesSlow);

    QCOMPARE(evaluatedValuesLinear.size(), evaluatedValuesSlow.size());
    QCOMPARE(evaluatedValuesLinear.size(), xs.size());

    for (unsigned i = 0; i < xs.size(); i++) {
        std::string desc = "i=" + std::to_string(i);
        QCOMPARE_KEL(evaluatedValuesLinear.at(i), evaluatedValuesSlow.at(i), desc.c_str());
    }
}

void pmi::PlotBaseTest::EvaluateLinear_Works_TempTest() {
    PlotBase pb;

    pb.addPoint(point2d(-100, 30.54));
    pb.addPoint(point2d(13.43, 289.54));
    pb.addPoint(point2d(100, 145.125));
    //pb.addPoint(point2d(100, 1000.251));
    pb.addPoint(point2d(150, -151.12));
    pb.addPoint(point2d(200, 565.1435));

    std::vector<double> xs, evaluatedValuesLinear, evaluatedValuesSlow;

    const double x_start = -125.0, x_end = 225.0, step = 0.01;

    for (double i = x_start; i < x_end; i += step) {
        xs.push_back(i);
    }

    pb.evaluate_linear(xs, &evaluatedValuesLinear);
    pb.evaluate_slow(xs, &evaluatedValuesSlow);

    QCOMPARE(evaluatedValuesLinear.size(), evaluatedValuesSlow.size());
    QCOMPARE(evaluatedValuesLinear.size(), xs.size());

    for (unsigned i = 13843; i < xs.size(); i++) {
        std::string desc = "i=" + std::to_string(i);
        QCOMPARE_KEL(evaluatedValuesLinear.at(i), evaluatedValuesSlow.at(i), desc.c_str());
    }
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsCorrectValues_GivenSameEvaluationPointMultipleTimes() {
    PlotBase pb;
    
    pb.addPoint(point2d(100, 10));
    pb.addPoint(point2d(150, 100));
    pb.addPoint(point2d(200, 50));

    std::vector<double> xs, evaluatedValues;

    xs.push_back(150);
    xs.push_back(150);
    xs.push_back(150);
    
    pb.evaluate_linear(xs, &evaluatedValues);

    QCOMPARE(evaluatedValues.size(), xs.size());

    for (unsigned i = 0; i < xs.size(); i++) {
        QCOMPARE(evaluatedValues.at(i), 100.0);
    }
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsCorrectValues_GivenPointListWithDuplicates() {
    PlotBase pb;

    pb.addPoint(point2d(0, 100));
    pb.addPoint(point2d(100, 12));
    pb.addPoint(point2d(100, 123));
    pb.addPoint(point2d(100, 1234));
    pb.addPoint(point2d(200, 50));

    std::vector<double> xs, evaluatedValues;

    xs.push_back(-10);
    xs.push_back(50);
    xs.push_back(100);
    xs.push_back(150);
    xs.push_back(175);  
    xs.push_back(200);
    xs.push_back(250);

    pb.evaluate_linear(xs, &evaluatedValues);

    QCOMPARE(evaluatedValues.size(), xs.size());

    QCOMPARE(evaluatedValues.at(0), 0.0);
    QEXPECT_FAIL("", "No current plans to handle duplicates (broken as of PMI-1299)", Continue);
    QCOMPARE(evaluatedValues.at(1), 667.0);
    QEXPECT_FAIL("", "No current plans to handle duplicates (broken as of PMI-1299)", Continue);
    QCOMPARE(evaluatedValues.at(2), 1234.0);
    QCOMPARE(evaluatedValues.at(3), 642.0);
    QCOMPARE(evaluatedValues.at(4), 346.0);
    QCOMPARE(evaluatedValues.at(5), 50.0);
    QCOMPARE(evaluatedValues.at(6), 0.0);
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsCorrectValues_GivenSameEvaluationPointMultipleTimesAndPointListWithDuplicates() {
    PlotBase pb;
    
    pb.addPoint(point2d(0, 100));
    pb.addPoint(point2d(100, 12));
    pb.addPoint(point2d(100, 123));
    pb.addPoint(point2d(100, 1234));
    pb.addPoint(point2d(200, 50));

    std::vector<double> xs, evaluatedValues;

    xs.push_back(-10);
    xs.push_back(50);
    xs.push_back(100);
    xs.push_back(150);
    xs.push_back(175);
    xs.push_back(200);
    xs.push_back(250);

    xs.push_back(-10);
    xs.push_back(50);
    xs.push_back(100);
    xs.push_back(150);
    xs.push_back(175);
    xs.push_back(200);
    xs.push_back(250);

    pb.evaluate_linear(xs, &evaluatedValues);

    QCOMPARE(evaluatedValues.size(), xs.size());

    QCOMPARE(evaluatedValues.at(0), 0.0);
    QEXPECT_FAIL("", "No current plans to handle duplicates (broken as of PMI-1299)", Continue);
    QCOMPARE(evaluatedValues.at(1), 667.0);
    QEXPECT_FAIL("", "No current plans to handle duplicates (broken as of PMI-1299)", Continue);
    QCOMPARE(evaluatedValues.at(2), 1234.0);
    QCOMPARE(evaluatedValues.at(3), 642.0);
    QCOMPARE(evaluatedValues.at(4), 346.0);
    QCOMPARE(evaluatedValues.at(5), 50.0);
    QCOMPARE(evaluatedValues.at(6), 0.0);

    QCOMPARE(evaluatedValues.at(0), 0.0);
    QEXPECT_FAIL("", "No current plans to handle duplicates (broken as of PMI-1299)", Continue);
    QCOMPARE(evaluatedValues.at(1), 667.0);
    QEXPECT_FAIL("", "No current plans to handle duplicates (broken as of PMI-1299)", Continue);
    QCOMPARE(evaluatedValues.at(2), 1234.0);
    QCOMPARE(evaluatedValues.at(3), 642.0);
    QCOMPARE(evaluatedValues.at(4), 346.0);
    QCOMPARE(evaluatedValues.at(5), 50.0);
    QCOMPARE(evaluatedValues.at(6), 0.0);
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsCorrectValues_GivenEvalValsListInitializedWithGarbageValues() {
    PlotBase pb;

    pb.addPoint(point2d(100, 10));
    pb.addPoint(point2d(200, 10));
    pb.addPoint(point2d(300, 10));

    std::vector<double> xs, evaluatedValues;

    xs.push_back(50);
    xs.push_back(100);
    xs.push_back(150);
    xs.push_back(200);
    xs.push_back(250);
    xs.push_back(300);
    xs.push_back(350);

    evaluatedValues.clear();

    // fill evaluatedValues with garbage values
    evaluatedValues.push_back(2340.546);
    evaluatedValues.push_back(541);
    evaluatedValues.push_back(4521.154);
    evaluatedValues.push_back(-3498.234);
    evaluatedValues.push_back(-234);
    evaluatedValues.push_back(-384.3);
    evaluatedValues.push_back(-214.34);
    evaluatedValues.push_back(343.42);
    evaluatedValues.push_back(14651.31);


    pb.evaluate_linear(xs, &evaluatedValues);

    QCOMPARE(evaluatedValues.size(), xs.size());

    QCOMPARE(evaluatedValues.at(0), 0.0);
    for (unsigned i = 1; i < xs.size() - 1; i++) {
        QCOMPARE(evaluatedValues.at(i), 10.0);
    }
    QCOMPARE(evaluatedValues.at(6), 0.0);
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsAllZeroes_GivenEvalValsInitializedWithGarbageValuesAndStartEvalOutOfBoundsRight() {
    PlotBase pb;

    pb.addPoint(point2d(100, 10));
    pb.addPoint(point2d(200, 10));
    pb.addPoint(point2d(300, 10));

    std::vector<double> xs, evaluatedValues;

    xs.push_back(350);
    xs.push_back(400);
    xs.push_back(450);
    xs.push_back(500);

    evaluatedValues.clear();

    // fill evaluatedValues with garbage values
    evaluatedValues.push_back(2340.546);
    evaluatedValues.push_back(541);
    evaluatedValues.push_back(4521.154);
    evaluatedValues.push_back(-3498.234);

    pb.evaluate_linear(xs, &evaluatedValues);

    QCOMPARE(evaluatedValues.size(), xs.size());

    for (unsigned i = 0; i < xs.size() ; i++) {
        QCOMPARE(evaluatedValues.at(i), 0.0);
    }
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsCorrectIndex_GivenExactXvals() {
    PlotBase pb;

    pb.addPoint(point2d(1, 10));
    pb.addPoint(point2d(2, 100));
    pb.addPoint(point2d(3, 1000));

    QCOMPARE(pb.getIndexLessThanValLinear(1), -1);
    QCOMPARE(pb.getIndexLessThanValLinear(2), 0);
    QCOMPARE(pb.getIndexLessThanValLinear(3), 1);
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsCorrectIndex_GivenNormalVals() {
    PlotBase pb;

    pb.addPoint(point2d(1, 10));
    pb.addPoint(point2d(2, 100));
    pb.addPoint(point2d(3, 1000));

    QCOMPARE(pb.getIndexLessThanValLinear(1.2), 0);
    QCOMPARE(pb.getIndexLessThanValLinear(2.5), 1);
    QCOMPARE(pb.getIndexLessThanValLinear(3.7), 2);
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsLastIndex_GivenOutOfBoundsRight() {
    PlotBase pb;

    pb.addPoint(point2d(1, 10));
    pb.addPoint(point2d(2, 100));

    QCOMPARE(pb.getIndexLessThanValLinear(10000), pb.getPointListSize() - 1);
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsNegativeOne_GivenOutOfBoundsLeft() {
    PlotBase pb;

    pb.addPoint(point2d(1, 10));
    pb.addPoint(point2d(2, 100));

    QCOMPARE(pb.getIndexLessThanValLinear(-10000), -1);
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsCorrectIndices_GivenStartIdx() {
    PlotBase pb;

    pb.addPoint(point2d(23, 10));
    pb.addPoint(point2d(25, 100));
    pb.addPoint(point2d(27, 1000));

    QCOMPARE(pb.getIndexLessThanValLinear(23, 2), 1);
    QCOMPARE(pb.getIndexLessThanValLinear(25, 2), 1);
    QCOMPARE(pb.getIndexLessThanValLinear(27, 2), 1);
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsCorrectIndices_GivenInvalidStartIndices() {
    PlotBase pb;

    pb.addPoint(point2d(23, 10));
    pb.addPoint(point2d(25, 100));
    pb.addPoint(point2d(27, 1000));

    QCOMPARE(pb.getIndexLessThanValLinear(23, 50), -1);
    QCOMPARE(pb.getIndexLessThanValLinear(25, -30), 0);
    QCOMPARE(pb.getIndexLessThanValLinear(27, 30.23), 1);
}

void pmi::PlotBaseTest::GetIndexLessThanValLinear_ReturnsCorrectIndices_GivenPointListWithDuplicates() {
    PlotBase pb;

    pb.addPoint(point2d(0, 100));
    pb.addPoint(point2d(100, 12));
    pb.addPoint(point2d(100, 123));
    pb.addPoint(point2d(100, 1234));
    pb.addPoint(point2d(200, 50));

    QCOMPARE(pb.getIndexLessThanValLinear(50), 0);
    QCOMPARE(pb.getIndexLessThanValLinear(100), 0);
    QCOMPARE(pb.getIndexLessThanValLinear(150), 3);
    QCOMPARE(pb.getIndexLessThanValLinear(175), 3);
    QCOMPARE(pb.getIndexLessThanValLinear(200), 3);
    QCOMPARE(pb.getIndexLessThanValLinear(250), 4);
}

void pmi::PlotBaseTest::testCentroidData() {
    PlotBase pb;

    pb.addPoint(point2d(0 , 0));
    pb.addPoint(point2d(5 , 10));
    pb.addPoint(point2d(10, -2));
    pb.addPoint(point2d(15, 10));
    pb.addPoint(point2d(20, 0));
    pb.addPoint(point2d(25, 10));
    pb.addPoint(point2d(30, 5));

    point2dList out;

    pb.makeCentroidedPoints(&out, PlotBase::CentroidWeightedAverage_RelativeWeight);
    QCOMPARE((int)out.size(), 3);

    QCOMPARE(out[0].x(), 5.0);
    QCOMPARE(out[0].y(), 10.0);

    QCOMPARE(out[1].x(), 15.0);
    QCOMPARE(out[1].y(), 10.0);

//w    0    10     5
//x    20   25    30
//p.x  10/15 * 25 + 5/15*30 = 26.6666
//p.y  eval(26.6666) --> 8.333  Note: didn't verify by hand (took the result from eval) but looks right.

    QCOMPARE(out[2].x(), 26.66666666666);
    QCOMPARE(out[2].y(), 8.333333333333);
}

bool comparePointsBetween(point2dList first, point2dList second, size_t from, size_t to)
{
    if (first.size() < to || second.size() < to) {
        return false;
    }

    for (size_t i = from; i < to; ++i) {
        if (first[i] != second[i]) {
            return false;
        }
    }
    return true;
}

void PlotBaseTest::testResizePointListToLower_data()
{
    QTest::addColumn<point2dList>("input");
    QTest::addColumn<int>("targetSize");
    QTest::addColumn<point2dList>("expectedResult");
    QTest::addColumn<bool>("isSorted");

    point2dList sortedFully{point2d(0, 0), point2d(3, -5), point2d(8, 15), point2d(10, 1)};
    point2dList sortedFullyResult{point2d(0, 0), point2d(3, -5), point2d(8, 15)};

    QTest::newRow("sorted fully trim") << sortedFully << 3 << sortedFullyResult << true;

    point2dList sortedPartially{point2d(0, 0), point2d(3, -5), point2d(8, 15), point2d(7, 1), point2d(6, 3)};
    point2dList sortedPartiallyResultSorted{point2d(0, 0), point2d(3, -5), point2d(8, 15)};
    point2dList sortedPartiallyResultNotSorted{point2d(0, 0), point2d(3, -5), point2d(8, 15), point2d(7, 1)};

    QTest::newRow("sorted partially trim to sorted")
            << sortedPartially << 3 << sortedPartiallyResultSorted << true;
    QTest::newRow("sorted partially trim to unsorted")
            << sortedPartially << 4 << sortedPartiallyResultNotSorted << false;

    point2dList unsorted{point2d(0, 0), point2d(-3, -5), point2d(8, 15), point2d(-10, 1)};
    point2dList unsortedResult{point2d(0, 0), point2d(-3, -5), point2d(8, 15)};

    QTest::newRow("unsorted trim") << unsorted << 3 << unsortedResult << false;
}

void PlotBaseTest::testResizePointListToLower()
{
    QFETCH(point2dList, input);
    QFETCH(int, targetSize);
    QFETCH(point2dList, expectedResult);
    QFETCH(bool, isSorted);

    PlotBase plotBase(input);
    plotBase.resizePointList(static_cast<size_t>(targetSize));

    point2dList result = plotBase.getPointList();
    QVERIFY(comparePointsBetween(input, result, 0, static_cast<size_t>(targetSize)));
    QCOMPARE(plotBase.isSortedAscendingX(), isSorted);
}

void PlotBaseTest::testResizePointListToHigher_data()
{
    QTest::addColumn<point2dList>("input");
    QTest::addColumn<int>("targetSize");
    QTest::addColumn<point2dList>("expectedResult");
    QTest::addColumn<bool>("isSorted");

    point2dList sortedFully{point2d(0, 0), point2d(3, -5), point2d(8, 15), point2d(10, 1)};
    point2dList sortedFullyResult{point2d(0, 0), point2d(3, -5), point2d(8, 15),
                                    point2d(10, 1), point2d(0, 0), point2d(0, 0)};

    QTest::newRow("sorted fully expand with values lower than last one")
            << sortedFully << 6 << sortedFullyResult << false;

    point2dList sortedFullyLT0{point2d(-5, 0), point2d(-3, -5), point2d(-1, 15)};
    point2dList sortedFullyLT0Result{point2d(-5, 0), point2d(-3, -5), point2d(-1, 15),
                                    point2d(0, 0), point2d(0, 0)};

    QTest::newRow("sorted fully expand with values higher than last one")
            << sortedFullyLT0 << 5 << sortedFullyLT0Result << true;

    point2dList unsorted{point2d(0, 0), point2d(-3, -5), point2d(8, 15), point2d(-10, 1)};
    point2dList unsortedResult{point2d(0, 0), point2d(-3, -5), point2d(8, 15), point2d(-10, 1), point2d(0, 0)};

    QTest::newRow("unsorted expand") << unsorted << 5 << unsortedResult << false;

    QTest::newRow("zeros") << point2dList{ point2d(0.0, 0.0), point2d(0.0, 0.0), point2d(0.0, 0.0) }
                           << 5
                           << point2dList{ point2d(0.0, 0.0), point2d(0.0, 0.0), point2d(0.0, 0.0),
                                           point2d(0.0, 0.0), point2d(0.0, 0.0) }
                           << true;
}

void PlotBaseTest::testResizePointListToHigher()
{
    QFETCH(point2dList, input);
    QFETCH(int, targetSize);
    QFETCH(point2dList, expectedResult);
    QFETCH(bool, isSorted);

    PlotBase plotBase(input);
    plotBase.resizePointList(static_cast<size_t>(targetSize));

    point2dList result = plotBase.getPointList();
    QVERIFY(comparePointsBetween(result, expectedResult, 0, static_cast<size_t>(targetSize)));
    QCOMPARE(plotBase.isSortedAscendingX(), isSorted);
}

void PlotBaseTest::testSwap()
{
    point2dList data1 = { point2d(0.0, 0.0), point2d(2.0, 2.0), point2d(1.0, 1.0), };

    PlotBase pb1(data1);
    pb1.name() = "pb1";

    PlotBase pb2; // empty
    pb2.name() = "pb2";
    QVERIFY(pb2.getPointList().empty());

    pb1.swap(pb2);
    
    // now pb1 is empty
    QVERIFY(pb1.getPointList().empty());
    
    QCOMPARE(pb2.getPointList(), data1);
    QCOMPARE(pb2.name(), QString("pb1"));
    QCOMPARE(pb2.isSortedAscendingX(), false);
}

#ifdef PMI_TEST_SLOW

inline double dRand(double dMin, double dMax)
{
    // rand, by default, seeds with srand(1)
    double d = (double)rand() / RAND_MAX;
    return dMin + d * (dMax - dMin);
}

bool point2d_unique(point2d x, point2d y) {
    return x.x()==y.x();
}

void pmi::PlotBaseTest::EvaluateLinear_ReturnsSameValuesAsEvaluateSlow_GivenSameMillionRandomPointsAsEvaluateSlow() {
    const int ONE_MILLION = 1000000;
    const int FIVE_MILLION = ONE_MILLION * 5;
    const int TEN_THOUSAND = 10000;
    const int FIFTY_THOUSAND = 50000;
    const int FIVE_HUNDRED_THOUSAND = 500000;

    const double RAND_DOUBLE_MIN = -FIFTY_THOUSAND;
    const double RAND_DOUBLE_MAX = FIFTY_THOUSAND;

    PlotBase pb;

    std::mt19937 eng(1);
    std::uniform_real_distribution<> distr(RAND_DOUBLE_MIN, RAND_DOUBLE_MAX);

    for (int i = 0; i < ONE_MILLION; i++) {
        double randA = distr(eng);
        double randB = distr(eng);
        pb.addPoint(point2d(randA, randB));
    }

    std::vector<double> xs, evaluatedValuesLinear, evaluatedValuesSlow;

    const double OVERSTEP = 10.0;
    const double x_start = RAND_DOUBLE_MIN * OVERSTEP, x_end = RAND_DOUBLE_MAX * OVERSTEP, step = 0.1;

    const double POINTS_THAT_WILL_BE_PUSHED = double(x_end - x_start) / step + 1;
    qDebug() << "max_size_vector xs: " << xs.max_size() << " points about to push: " << POINTS_THAT_WILL_BE_PUSHED;

    xs.reserve(POINTS_THAT_WILL_BE_PUSHED);
    for (double i = x_start; i < x_end; i += step) {
        xs.push_back(i);
    }


    pb.sortPointListByX();
    point2dList & pbPointList = pb.getPointList();
    qDebug() << "sizebefore=" << pbPointList.size();
    auto last = std::unique(pbPointList.begin(), pbPointList.end(), point2d_unique);

    std::ofstream myfile;
    myfile.open("E:\\PMI-dev\\work_bench\\out.csv");

    myfile << "x,y\n";

    for (int i = 0; i < (int)pbPointList.size(); i++) {
        myfile << std::setprecision(20) << pbPointList[i].x() << "," << pbPointList[i].y() << "\n";
    }

    myfile.close();

    // remove duplicates
    pbPointList.erase(last, pbPointList.end());
    qDebug() << "sizeafter=" << pbPointList.size();

    clock_t linear_start, linear_end, slow_start, slow_end;


    evaluatedValuesSlow.resize(xs.size());
    slow_start = clock();
    pb.evaluate_slow(xs, &evaluatedValuesSlow);
    slow_end = clock();

    double seconds_run_time = ((double)slow_end - (double)slow_start) / CLOCKS_PER_SEC;

    evaluatedValuesLinear.resize(xs.size());

    qDebug() << "slow run time (seconds): " << seconds_run_time;

    linear_start = clock();
    evaluatedValuesLinear.clear();
    pb.evaluate_linear(xs, &evaluatedValuesLinear);
    linear_end = clock();

    seconds_run_time = ((double)linear_end - (double)linear_start) / CLOCKS_PER_SEC;

    qDebug() << "linear run time (seconds): " << seconds_run_time;

    QCOMPARE(evaluatedValuesLinear.size(), evaluatedValuesSlow.size());
    QCOMPARE(evaluatedValuesLinear.size(), xs.size());

    for (unsigned i = 0; i < xs.size(); i++) {
        std::string desc = "actual=" + std::to_string(evaluatedValuesLinear.at(i)) + " expected=" + std::to_string(evaluatedValuesSlow.at(i)) + " i=" + std::to_string(i);
        QCOMPARE_KEL(evaluatedValuesLinear.at(i), evaluatedValuesSlow.at(i), desc.c_str());
    }
}

void PlotBaseTest::testEvaluateSlow()
{
    // create list sorted by x with 15000 items
    const int COUNT = 15000;
    point2dList points;
    points.reserve(COUNT);
    for (int i = 0; i < COUNT; ++i) {
        QPointF item(i, sin(i));
        points.push_back(item);
    }

    PlotBase input(points);

    // create list of x values with particular step
    qreal min = input.getPoint(0).x();
    qreal max = input.getPoint(input.getPointListSize() - 1).x();
    qreal step = 0.0001;
    int xsCount = qCeil((max - min) / step) + 1;

    const int TILE_WIDTH = 64;

    // evaluate 64 xs per cols (similar to tiles) -- we cannot allocate xs and ys with big size (step is very small number)

    /*QBENCHMARK*/ {
        std::vector<double> xs;
        xs.reserve(64);

        std::vector<double> ys;
        ys.reserve(xs.size());
        int cols = xsCount / TILE_WIDTH;
        qDebug() << "Cols" << cols;
        for (int i = 0; i < cols; ++i){

            int xStartPos = i * TILE_WIDTH;
            for (int tileIndex = 0; tileIndex < TILE_WIDTH; ++tileIndex){
                qreal x = min + (xStartPos + i) * step;
                xs.push_back(x);
            }

            input.evaluate_slow(xs, &ys);

            // verify xs, ys
            QCOMPARE(xs.size(), ys.size());
            for (size_t j = 0; j < xs.size(); ++j) {
                qreal y = input.evaluate(xs.at(j));
                QCOMPARE(ys.at(j), y);
            }

            xs.clear();
            ys.clear();

            if (i % 20000 == 0){
                qDebug() << "Processed" << i << "columns (" << qRound((i / (double)cols) * 100) << "%)";
            }
        }
    }
}

#endif

_PMI_END

QTEST_MAIN(pmi::PlotBaseTest)

#include "PlotBaseTest.moc"
