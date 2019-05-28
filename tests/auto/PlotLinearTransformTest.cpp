#include <QtTest>
#include "PlotBase.h"

_PMI_BEGIN

const int POINT_COUNT = 10;

class PlotLinearTransformTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testTransform();


};


void PlotLinearTransformTest::testTransform()
{
    point2dList points;
    for (int i = 0; i < POINT_COUNT; i++) {
        points.push_back(QPointF(i, i));
    }

    PlotLinearTransform plotLT;
    // inspired hack by byteArrayToPlotBase call in PlotBase.cpp:1822 TODO: add constructor
    //PlotBase * base = &plotLT;
    point2dList & plist = plotLT.getPointList(); // this is not encapsulation!!!
    plist = points;

    QPointF offsetPoint(0, 5);

    plotLT.setTranslate(offsetPoint);
    point2dList translated = plotLT.getPointList();

    QCOMPARE(points.size(), translated.size());
    for (size_t i = 0; i < points.size(); i++)    {
        QPointF diff = translated.at(i) - points.at(i);
        QCOMPARE(diff, offsetPoint);
    }
}


_PMI_END

QTEST_MAIN(pmi::PlotLinearTransformTest)

#include "PlotLinearTransformTest.moc"
