#include <QtTest>
#include "pmi_core_defs.h"
#include "TileLevelSelector.h"

_PMI_BEGIN

class TileLevelSelectorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testLevelSelectionSameZoom();
};

void TileLevelSelectorTest::testLevelSelectionSameZoom()
{
    QVector<QPoint> levels{ 
        QPoint(1,1), 
        QPoint(2,2), 
        QPoint(3,3), 
        QPoint(4,4),  
        QPoint(5,5),
        QPoint(6,6),
        QPoint(7,7),
        QPoint(8,8),
    };

    TileLevelSelector selector(levels);
    
    QCOMPARE(selector.findMaxXLevel(), 8);
    QCOMPARE(selector.findMaxYLevel(), 8);

    // zoom, expected level 
    QVector< QPair<qreal, int> > data = {
        { 1.0, 1 },
        { 0.755, 1 },
        { 0.75, 2 }, // border value
        { 0.74, 2 },
        { 0.5, 2 },
        { 0.25, 3 },
        { 0.125, 4 },
        { 0.0625, 5 },
        { 0.03125, 6 },
        { 0.015625, 7 },
        { 0.0078125, 8 }
    }; 
    
    for (const auto &item : data) {
        QPoint level = selector.selectLevelByScaleFactor(item.first, item.first);
        QCOMPARE(level, QPoint(item.second, item.second));
    }
}

_PMI_END

QTEST_MAIN(pmi::TileLevelSelectorTest)

#include "TileLevelSelectorTest.moc"
