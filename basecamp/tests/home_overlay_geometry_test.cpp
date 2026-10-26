// Unit tests for where the desktop layout's Basecamp overlay puts its panel
// (src/HomeOverlayGeometry.h, basecamp-experiments#43): centred, 840 to 1000
// px wide, the height between the top bar and the dock, never off the window;
// and the Activity sidebar's column on the right edge (#44).
//
// Run: nix build .#unit-tests -L

#include "HomeOverlayGeometry.h"

#include <QtTest/QtTest>

class HomeOverlayGeometryTest : public QObject {
    Q_OBJECT

private slots:
    void laptopIsCentredBetweenTheBounds()
    {
        const QRect p = HomeOverlayGeometry::panel(QSize(1280, 800));
        QCOMPARE(p.width(), 896);
        QCOMPARE(p.x(), (1280 - 896) / 2);
        QCOMPARE(p.y(), HomeOverlayGeometry::kEdge);
        QCOMPARE(p.height(), 800 - 2 * HomeOverlayGeometry::kEdge);
    }

    void staysBetweenTheBarAndTheDock()
    {
        const QRect p = HomeOverlayGeometry::panel(QSize(1280, 800), 30, 92);
        QCOMPARE(p.top(), 30 + HomeOverlayGeometry::kEdge);
        QCOMPARE(p.y() + p.height(), 800 - 92 - HomeOverlayGeometry::kEdge);
    }

    void wideScreenStopsAtTheMaximum()
    {
        const QRect p = HomeOverlayGeometry::panel(QSize(1920, 1080));
        QCOMPARE(p.width(), HomeOverlayGeometry::kMaxWidth);
        QCOMPARE(p.center().x(), QRect(0, 0, 1920, 1080).center().x());
    }

    void smallWindowKeepsTheMinimumUntilTheEdges()
    {
        QCOMPARE(HomeOverlayGeometry::panel(QSize(1000, 700)).width(), HomeOverlayGeometry::kMinWidth);
        const QRect p = HomeOverlayGeometry::panel(QSize(800, 600));
        QCOMPARE(p.width(), 800 - 2 * HomeOverlayGeometry::kEdge);
        QCOMPARE(p.x(), HomeOverlayGeometry::kEdge);
    }

    void contentSitsInsideThePanel()
    {
        const QSize area(1440, 900);
        const QRect p = HomeOverlayGeometry::panel(area, 30, 92);
        const QRect c = HomeOverlayGeometry::content(area, 30, 92);
        QVERIFY(p.contains(c));
        QCOMPARE(c.x() - p.x(), HomeOverlayGeometry::kInset);
        QCOMPARE(p.bottom() - c.bottom(), HomeOverlayGeometry::kInset);
    }

    void tinyAreaNeverGoesNegative()
    {
        const QRect p = HomeOverlayGeometry::panel(QSize(20, 20), 30, 92);
        QVERIFY(p.width() >= 0);
        QVERIFY(p.height() >= 0);
    }

    // A width the user chose by dragging an edge.
    void chosenWidthIsUsedAndStaysCentred()
    {
        const QSize area(1920, 1080);
        const QRect p = HomeOverlayGeometry::panel(area, 30, 92, 1400);
        QCOMPARE(p.width(), 1400);
        QCOMPARE(p.x(), (1920 - 1400) / 2);
        QCOMPARE(p.top(), 30 + HomeOverlayGeometry::kEdge);
        QCOMPARE(HomeOverlayGeometry::panel(area, 30, 92, 0), HomeOverlayGeometry::panel(area, 30, 92));
    }

    void chosenWidthIsClampedToTheRange()
    {
        const QSize area(1920, 1080);
        QCOMPARE(HomeOverlayGeometry::panelWidth(area, 500), HomeOverlayGeometry::kMinWidth);
        QCOMPARE(HomeOverlayGeometry::panelWidth(area, 5000), 1920 - 2 * HomeOverlayGeometry::kEdge);
        QCOMPARE(HomeOverlayGeometry::maxWidth(area), 1872);
        QCOMPARE(HomeOverlayGeometry::minWidth(area), HomeOverlayGeometry::kMinWidth);
    }

    void storedWidthWiderThanTheWindowReclamps()
    {
        // Chosen on a 1920 screen, used in a 1280 window, then back.
        const int chosen = 1872;
        const QRect small = HomeOverlayGeometry::panel(QSize(1280, 800), 0, 0, chosen);
        QCOMPARE(small.width(), 1280 - 2 * HomeOverlayGeometry::kEdge);
        QCOMPARE(small.x(), HomeOverlayGeometry::kEdge);
        QCOMPARE(HomeOverlayGeometry::panel(QSize(1920, 1080), 0, 0, chosen).width(), chosen);
        // Smaller than the minimum: the room wins, never off the window.
        const QRect tiny = HomeOverlayGeometry::panel(QSize(700, 500), 0, 0, 1200);
        QCOMPARE(tiny.width(), 700 - 2 * HomeOverlayGeometry::kEdge);
        QCOMPARE(HomeOverlayGeometry::panelWidth(QSize(700, 500), 300), 700 - 2 * HomeOverlayGeometry::kEdge);
        QVERIFY(HomeOverlayGeometry::panelWidth(QSize(20, 20), 900) >= 0);
    }

    void draggingEitherEdgeResizesSymmetrically()
    {
        const QSize area(1920, 1080);
        // The right edge at x = 1660 is 700 px right of the centre (960).
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 1660, true), 1400);
        // The left edge at the mirror place gives the same width.
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 260, false), 1400);
        // Past the window's margin, or into the middle: the range's ends.
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 1915, true), 1872);
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 2, false), 1872);
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 1000, true), HomeOverlayGeometry::kMinWidth);
        // An edge dragged across the centre narrows, never flips to wide.
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 10, true), HomeOverlayGeometry::kMinWidth);
        QCOMPARE(HomeOverlayGeometry::widthForEdgeAt(area, 1900, false), HomeOverlayGeometry::kMinWidth);
        // The result places the dragged edge under the pointer.
        const QRect p = HomeOverlayGeometry::panel(area, 0, 0, HomeOverlayGeometry::widthForEdgeAt(area, 1660, true));
        QCOMPARE(p.x() + p.width(), 1660);
        QCOMPARE(p.x(), 260);
    }

    void doubleClickTogglesDefaultAndWidest()
    {
        const QSize area(1920, 1080);
        // From the default (no choice) to the widest.
        const int wide = HomeOverlayGeometry::toggledWidth(area, 0);
        QCOMPARE(wide, 1872);
        // From the widest back to the default.
        QCOMPARE(HomeOverlayGeometry::toggledWidth(area, wide), 0);
        QCOMPARE(HomeOverlayGeometry::panelWidth(area, 0), HomeOverlayGeometry::kMaxWidth);
        // From a dragged width in between, to the widest.
        QCOMPARE(HomeOverlayGeometry::toggledWidth(area, 1300), 1872);
        // A stored width beyond this window counts as the widest here.
        QCOMPARE(HomeOverlayGeometry::toggledWidth(QSize(1280, 800), 1872), 0);
    }

    void contentFollowsAChosenWidth()
    {
        const QSize area(1920, 1080);
        const QRect c = HomeOverlayGeometry::content(area, 30, 92, 1872);
        QCOMPARE(c.width(), 1872 - 2 * HomeOverlayGeometry::kInset);
        QCOMPARE(c.x(), HomeOverlayGeometry::kEdge + HomeOverlayGeometry::kInset);
    }

    // The Activity sidebar (basecamp-experiments#44).
    void sidebarHugsTheRightEdgeUnderTheBar()
    {
        const QRect s = HomeOverlayGeometry::sidebar(QSize(1280, 800), 36);
        QCOMPARE(s.width(), HomeOverlayGeometry::kSidebarWidth);
        QCOMPARE(s.x() + s.width(), 1280);
        QCOMPARE(s.top(), 36);
        QCOMPARE(s.y() + s.height(), 800);
    }

    void sidebarContentLeavesTheBorder()
    {
        const QSize area(1440, 900);
        const QRect s = HomeOverlayGeometry::sidebar(area, 36);
        const QRect c = HomeOverlayGeometry::sidebarContent(area, 36);
        QCOMPARE(c.x(), s.x() + 1);
        QCOMPARE(c.right(), s.right());
        QCOMPARE(c.height(), s.height());
    }

    void sidebarNarrowerThanTheWindowNeverOverflows()
    {
        const QRect s = HomeOverlayGeometry::sidebar(QSize(300, 500), 36);
        QCOMPARE(s.x(), 0);
        QCOMPARE(s.width(), 300);
        const QRect t = HomeOverlayGeometry::sidebar(QSize(0, 10), 36);
        QVERIFY(t.width() >= 0);
        QVERIFY(t.height() >= 0);
    }
};

QTEST_GUILESS_MAIN(HomeOverlayGeometryTest)
#include "home_overlay_geometry_test.moc"
