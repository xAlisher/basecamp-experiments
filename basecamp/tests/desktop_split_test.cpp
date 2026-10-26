// srcdeps: DesktopArea.cpp AppTileIcon.cpp
// libdeps: Logos::DesignSystem
//
// Unit tests for the Desktop layout's split view (DesktopArea, contributed by
// vpavlin/basecampos): ratio snapping, pair ranking with weekly halving, the
// split-pairs.json round trip, the layout in landscape and portrait, and the
// rules that end a split (tiling, minimise, close, maximise).
//
//   nix build .#unit-tests -L
#include "DesktopArea.h"

#include <QtTest/QtTest>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QLabel>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace {

constexpr qint64 kDay = 86400000;

void processDeferred()
{
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QCoreApplication::sendPostedEvents();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

QString pairsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/split-pairs.json");
}

} // namespace

class DesktopAreaSplitTest : public QObject
{
    Q_OBJECT

    using Pair = DesktopArea::SplitPair;

    DesktopArea* m_area = nullptr;
    QTemporaryDir m_home;   // the build sandbox's HOME isn't writable

    AppWindow* open(const QString& key)
    {
        auto* label = new QLabel(key);
        return m_area->openWindow(key, key.toUpper(), QString(), label);
    }

    static Pair pair(const QString& a, const QString& b, int count, qint64 last, double ratio = 0.5)
    {
        Pair p;
        p.a = a;
        p.b = b;
        p.aTitle = a.toUpper();
        p.bTitle = b.toUpper();
        p.count = count;
        p.last = last;
        p.ratio = ratio;
        return p;
    }

    // The split's own widgets on the viewport (their classes live in
    // DesktopArea.cpp): the divider is translucent, the picker opaque.
    QWidget* overlay(Qt::WidgetAttribute attr) const
    {
        for (QWidget* c : m_area->viewport()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (!qobject_cast<QMdiSubWindow*>(c) && c->testAttribute(attr) && c->isVisible())
                return c;
        return nullptr;
    }
    QWidget* divider() const { return overlay(Qt::WA_TranslucentBackground); }
    QWidget* picker() const { return m_area->m_picker ? overlay(Qt::WA_OpaquePaintEvent) : nullptr; }

    // The window's title bar: the frame's first child, 36 px high.
    static QWidget* titleBar(AppWindow* w)
    {
        processDeferred();
        for (QWidget* c : w->widget()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
            if (c->height() == 36 && c != w->content())
                return c;
        return nullptr;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_home.isValid());
        qputenv("HOME", m_home.path().toUtf8());
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(QStringLiteral("desktop-split-test"));
    }

    void init()
    {
        QFile::remove(pairsPath());
        m_area = new DesktopArea;
        m_area->resize(1280, 800);
        m_area->show();
        processDeferred();
    }

    void cleanup()
    {
        delete m_area;
        m_area = nullptr;
        QFile::remove(pairsPath());
    }

    void snapsToThirdsAndHalf()
    {
        QCOMPARE(DesktopArea::snapSplitRatio(0.34), 1.0 / 3);
        QCOMPARE(DesktopArea::snapSplitRatio(0.48), 0.5);
        QCOMPARE(DesktopArea::snapSplitRatio(0.65), 2.0 / 3);
        QCOMPARE(DesktopArea::snapSplitRatio(0.42), 0.42);   // between snaps: kept
        QCOMPARE(DesktopArea::snapSplitRatio(0.05), 0.2);    // clamped
        QCOMPARE(DesktopArea::snapSplitRatio(0.95), 0.8);
    }

    void scoreHalvesEveryWeek()
    {
        const qint64 now = 1'800'000'000'000;
        QCOMPARE(pair("a", "b", 8, now).score(now), 8.0);
        QCOMPARE(pair("a", "b", 8, now - 7 * kDay).score(now), 4.0);
        QCOMPARE(pair("a", "b", 8, now - 14 * kDay).score(now), 2.0);
        QVERIFY(qAbs(pair("a", "b", 8, now - 7 * kDay / 2).score(now) - 8 / std::sqrt(2.0)) < 1e-9);
        QCOMPARE(pair("a", "b", 8, now + kDay).score(now), 8.0);   // a clock set back: no boost
    }

    void pairsRoundTripAndRank()
    {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        QList<Pair> in{ pair("a", "b", 1, now, 0.4),
                        pair("c", "a", 5, now, 0.3),
                        pair("a", "d", 8, now - 28 * kDay) };   // 8 → 0.5 after four weeks
        m_area->savePairs(in);
        QVERIFY(QFile::exists(pairsPath()));

        const QList<Pair> out = m_area->loadPairs();
        QCOMPARE(out.size(), 3);
        QCOMPARE(out[0].a, QStringLiteral("c"));   // saved most used first
        QCOMPARE(out[0].count, 5);
        QCOMPARE(out[0].ratio, 0.3);
        QCOMPARE(out[0].aTitle, QStringLiteral("C"));
        QCOMPARE(out[2].a, QStringLiteral("a"));
        QCOMPARE(out[2].b, QStringLiteral("d"));
        QCOMPARE(out[2].last, now - 28 * kDay);

        // Seen from "a": c (stored the other way round, so the ratio flips),
        // then b, then the faded d.
        const QList<Pair> partners = m_area->partnersOf(QStringLiteral("a"));
        QCOMPARE(partners.size(), 3);
        QCOMPARE(partners[0].b, QStringLiteral("c"));
        QCOMPARE(partners[0].a, QStringLiteral("a"));
        QVERIFY(qAbs(partners[0].ratio - 0.7) < 1e-9);
        QCOMPARE(partners[1].b, QStringLiteral("b"));
        QCOMPARE(partners[2].b, QStringLiteral("d"));
        QVERIFY(qAbs(m_area->rememberedRatio("a", "c") - 0.7) < 1e-9);
        QVERIFY(qAbs(m_area->rememberedRatio("c", "a") - 0.3) < 1e-9);
        QCOMPARE(m_area->rememberedRatio("a", "zzz"), 0.5);
    }

    void savingKeepsThirty()
    {
        QList<Pair> in;
        for (int i = 0; i < 40; ++i)
            in << pair("a", QString::number(i), i + 1, QDateTime::currentMSecsSinceEpoch());
        m_area->savePairs(in);
        const QList<Pair> out = m_area->loadPairs();
        QCOMPARE(out.size(), 30);
        QCOMPARE(out.first().count, 40);
        QCOMPARE(out.last().count, 11);
    }

    void corruptFileReadsEmpty()
    {
        QFile f(pairsPath());
        QDir().mkpath(QFileInfo(pairsPath()).absolutePath());
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not json");
        f.close();
        QVERIFY(m_area->loadPairs().isEmpty());
    }

    void splitLandscapeSideBySide()
    {
        open("a");
        open("b");
        open("c");
        m_area->setSplit("a", "b", 0.5);
        QVERIFY(m_area->splitActive());
        QVERIFY(m_area->inSplit(m_area->window("a")));
        QVERIFY(!m_area->inSplit(m_area->window("c")));
        const QRect ra = m_area->window("a")->geometry();
        const QRect rb = m_area->window("b")->geometry();
        QCOMPARE(ra.top(), rb.top());
        QVERIFY(ra.right() < rb.left());
        QVERIFY(qAbs(ra.width() - rb.width()) <= 1);
        QVERIFY(m_area->maximizedRect().contains(ra.united(rb)));
        QVERIFY(divider());

        // Recorded as a used pair with its ratio.
        const QList<Pair> pairs = m_area->loadPairs();
        QCOMPARE(pairs.size(), 1);
        QCOMPARE(pairs[0].count, 1);
        QCOMPARE(pairs[0].a, QStringLiteral("a"));
    }

    void splitRespectsDockReserve()
    {
        m_area->setBottomReserve(120);
        open("a");
        open("b");
        m_area->setSplit("a", "b", 0.5);
        QVERIFY(m_area->window("a")->geometry().bottom() < 800 - 120);
    }

    void splitPortraitStacked()
    {
        m_area->resize(800, 1280);
        processDeferred();
        open("a");
        open("b");
        m_area->setSplit("a", "b", 1.0 / 3);
        QVERIFY(m_area->splitVertical());
        const QRect ra = m_area->window("a")->geometry();
        const QRect rb = m_area->window("b")->geometry();
        QCOMPARE(ra.left(), rb.left());
        QVERIFY(ra.bottom() < rb.top());
        QVERIFY(ra.height() < rb.height());   // a third above, two thirds below

        // Turned to landscape: side by side again.
        m_area->resize(1280, 800);
        processDeferred();
        QVERIFY(!m_area->splitVertical());
        QCOMPARE(m_area->window("a")->geometry().top(), m_area->window("b")->geometry().top());
    }

    void dividerSnapsAndSwaps()
    {
        open("a");
        open("b");
        m_area->setSplit("a", "b", 0.5);
        QWidget* d = divider();
        QVERIFY(d);
        const QRect inner = m_area->maximizedRect().adjusted(10, 10, -10, -10);
        const int y = d->height() / 2;
        QTest::mousePress(d, Qt::LeftButton, Qt::NoModifier, QPoint(d->width() / 2, y));
        // Drag to just past a third: it snaps onto the third.
        const int x = inner.left() + int(inner.width() * 0.345);
        QMouseEvent move(QEvent::MouseMove, d->mapFrom(m_area->viewport(), QPoint(x, inner.top() + 50)),
                         d->mapToGlobal(QPoint(0, 0)), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(d, &move);
        QCOMPARE(m_area->m_splitRatio, 1.0 / 3);
        QTest::mouseRelease(d, Qt::LeftButton, Qt::NoModifier, d->rect().center());
        QVERIFY(m_area->window("a")->width() < m_area->window("b")->width());
        QVERIFY(qAbs(m_area->loadPairs().first().ratio - 1.0 / 3) < 1e-9);
        QCOMPARE(m_area->loadPairs().first().count, 1);   // a resize isn't another use

        // Double-click swaps the sides and keeps each window's size.
        QTest::mouseDClick(divider(), Qt::LeftButton);
        QCOMPARE(m_area->m_splitA, QStringLiteral("b"));
        QVERIFY(qAbs(m_area->m_splitRatio - 2.0 / 3) < 1e-9);
        QVERIFY(m_area->window("b")->x() < m_area->window("a")->x());
        QVERIFY(m_area->window("a")->width() < m_area->window("b")->width());
    }

    void tilingAndSplitExcludeEachOther()
    {
        open("a");
        open("b");
        m_area->setSplit("a", "b");
        m_area->setTiling(true);
        QVERIFY(m_area->tiling());
        QVERIFY(!m_area->splitActive());

        m_area->beginSplit(m_area->window("a"));
        QVERIFY(!m_area->tiling());
        QVERIFY(m_area->splitActive());
    }

    void onlyOtherWindowJoinsAtOnce()
    {
        open("a");
        open("b");
        m_area->beginSplit(m_area->window("a"));
        QCOMPARE(m_area->m_splitB, QStringLiteral("b"));
        QVERIFY(!m_area->m_picker);
    }

    void pickerWaitsThenDockAppFillsHalf()
    {
        open("a");
        open("b");
        open("c");
        m_area->beginSplit(m_area->window("a"));
        QVERIFY(m_area->splitActive());
        QVERIFY(m_area->m_splitB.isEmpty());
        QVERIFY(m_area->m_picker);
        QVERIFY(picker());
        QCOMPARE(picker()->geometry(), m_area->splitRect(1));

        // An app opened (from the dock) while the picker waits takes the half.
        open("d");
        processDeferred();
        QCOMPARE(m_area->m_splitA, QStringLiteral("a"));
        QCOMPARE(m_area->m_splitB, QStringLiteral("d"));
        QVERIFY(!m_area->m_picker);
    }

    void restoredWindowFillsWaitingHalf()
    {
        open("a");
        open("b");
        open("c");
        m_area->minimize(m_area->window("c"));
        m_area->beginSplit(m_area->window("a"));
        m_area->restoreAndRaise(m_area->window("c"));
        processDeferred();
        QCOMPARE(m_area->m_splitB, QStringLiteral("c"));
    }

    void closedPartnerIsLaunched()
    {
        m_area->savePairs({ pair("a", "z", 3, QDateTime::currentMSecsSinceEpoch()) });
        open("a");
        open("b");
        QSignalSpy launch(m_area, &DesktopArea::launchRequested);
        m_area->beginSplit(m_area->window("a"));
        QVERIFY(m_area->m_picker);   // a remembered partner: the picker, not b at once
        m_area->choosePartner(QStringLiteral("z"));
        QCOMPARE(launch.count(), 1);
        QCOMPARE(launch.first().first().toString(), QStringLiteral("z"));

        open("z");   // its window arrives
        processDeferred();
        QCOMPARE(m_area->m_splitB, QStringLiteral("z"));
        QCOMPARE(m_area->loadPairs().first().count, 4);
    }

    void openingWhileSplitReplacesActiveSide()
    {
        open("a");
        open("b");
        m_area->setSplit("a", "b");
        m_area->setActiveSubWindow(m_area->window("b"));
        open("c");
        processDeferred();
        QCOMPARE(m_area->m_splitA, QStringLiteral("a"));
        QCOMPARE(m_area->m_splitB, QStringLiteral("c"));
    }

    void minimiseOrCloseLeavesSplitOtherMaximised()
    {
        open("a");
        open("b");
        m_area->setSplit("a", "b");
        m_area->minimize(m_area->window("a"));
        QVERIFY(!m_area->splitActive());
        QVERIFY(m_area->window("b")->isMaximizedOnDesktop());

        m_area->restoreAndRaise(m_area->window("a"));
        m_area->setSplit("a", "b");
        m_area->removeWindow(m_area->window("b"));
        QVERIFY(!m_area->splitActive());
        QVERIFY(m_area->window("a")->isMaximizedOnDesktop());
    }

    void maximiseButtonLeavesSplit()
    {
        open("a");
        open("b");
        m_area->setSplit("a", "b");
        AppWindow* a = m_area->window("a");
        QWidget* bar = titleBar(a);
        QVERIFY(bar);
        // Buttons right to left: close, maximise, minimise, split (14 px, 10 px gaps).
        const QPoint maximise(bar->width() - 14 - 7 - 24, bar->height() / 2);
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, maximise);
        QVERIFY(!m_area->splitActive());
        QVERIFY(a->isMaximizedOnDesktop());
    }

    void splitButtonStartsSplit()
    {
        open("a");
        open("b");
        AppWindow* b = m_area->window("b");
        QWidget* bar = titleBar(b);
        QVERIFY(bar);
        const QPoint split(bar->width() - 14 - 7 - 3 * 24, bar->height() / 2);
        QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier, split);
        QVERIFY(m_area->splitActive());
        QCOMPARE(m_area->m_splitA, QStringLiteral("b"));
        QCOMPARE(m_area->m_splitB, QStringLiteral("a"));
    }
};

QTEST_MAIN(DesktopAreaSplitTest)
#include "desktop_split_test.moc"
