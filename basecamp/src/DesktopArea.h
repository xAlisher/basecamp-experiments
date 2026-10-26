#pragma once

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QPixmap>
#include <QPointer>
#include <QImage>
#include <QQuickImageProvider>
#include <QQuickPaintedItem>

class WindowFrame;
class WindowTitleBar;
class SplitPicker;
class SplitDivider;

// The desktop's backdrop: the wallpaper, composed once per size and fit, and
// a blurred copy that frosted surfaces (status bar, title bars, window frames)
// paint under their tint, as glass. The status bar and the window area are its
// children, so the wallpaper runs behind both.
class DesktopRoot : public QWidget
{
    Q_OBJECT

public:
    explicit DesktopRoot(QWidget* parent = nullptr);

    // An image file or qrc path (empty = plain), and "fill", "fit" or "stretch".
    void setWallpaper(const QString& path, const QString& fit);

    // Paints the blurred backdrop under `w` (any descendant), then `tint`.
    static void paintGlass(QPainter& p, QWidget* w, const QRect& r, const QColor& tint);

    const QPixmap& blurredPixmap() const { return m_blurred; }

    // The blurred backdrop as an image (device pixels), for QML glass.
    QImage blurredImage() const { return m_blurredImage; }
    int revision() const { return m_revision; }

signals:
    void backdropChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild();

    QPixmap m_source;
    QString m_fit = QStringLiteral("fit");
    QPixmap m_composed;
    QPixmap m_blurred;
    QImage m_blurredImage;
    int m_revision = 0;
};

// QML glass for the shell's own windows: a QQuickWidget can't show the
// widgets beneath it, so it draws the blurred backdrop itself. `glass` (a
// context property) says where the widget sits on the desktop; the image is
// "image://backdrop/blur?<revision>".
class GlassInfo : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int offsetX READ offsetX NOTIFY changed)
    Q_PROPERTY(int offsetY READ offsetY NOTIFY changed)
    Q_PROPERTY(int rootWidth READ rootWidth NOTIFY changed)
    Q_PROPERTY(int rootHeight READ rootHeight NOTIFY changed)
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(bool enabled READ enabled CONSTANT)

public:
    GlassInfo(DesktopRoot* root, QWidget* widget, QObject* parent = nullptr);
    int offsetX() const { return m_offset.x(); }
    int offsetY() const { return m_offset.y(); }
    int rootWidth() const;
    int rootHeight() const;
    int revision() const;
    bool enabled() const { return true; }

signals:
    void changed();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void refresh();
    void refreshSoon();
    QPointer<DesktopRoot> m_root;
    QPointer<QWidget> m_widget;
    QPoint m_offset;
    bool m_pending = false;
};

class BackdropProvider : public QQuickImageProvider
{
public:
    explicit BackdropProvider(DesktopRoot* root)
        : QQuickImageProvider(QQuickImageProvider::Image), m_root(root) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override;

private:
    QPointer<DesktopRoot> m_root;
};

// One app on the desktop layout: a frameless QMdiSubWindow with a Basecamp
// title bar (icon, name, minimise / maximise / close) drawn from theme tokens.
// `key` names it: a module name for an app, or "sys:<name>" for the shell's
// own Applications / Package Manager / Settings.
class AppWindow : public QMdiSubWindow
{
    Q_OBJECT

public:
    AppWindow(const QString& key, const QString& title, const QString& iconPath,
              QWidget* content, QWidget* parent = nullptr);

    QString key() const { return m_key; }
    QString iconPath() const { return m_iconPath; }
    QWidget* content() const { return m_content; }

    bool isMaximizedOnDesktop() const { return m_maximized; }
    // Maximised fills the desktop above the dock, not the whole area, so it
    // is tracked here rather than with Qt's own window state.
    void setMaximizedOnDesktop(bool on);
    void toggleMaximized() { setMaximizedOnDesktop(!m_maximized); }
    void setActiveLook(bool active);

    // Gives the content back to its owner (the host deletes app widgets
    // itself) so deleting this window doesn't take the widget with it.
    QWidget* releaseContent();

signals:
    void closeRequested(const QString& key);
    void minimizeRequested(const QString& key);
    void splitRequested(const QString& key);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void moveEvent(QMoveEvent* event) override;

private:
    QString m_key;
    QString m_iconPath;
    QPointer<QWidget> m_content;
    WindowFrame* m_frame = nullptr;
    WindowTitleBar* m_titleBar = nullptr;
    bool m_maximized = false;
    bool m_active = false;
    QRect m_normalGeometry;
};

// The desktop: a wallpaper with app windows on it. The dock floats above it
// (MainContainer owns that); `bottomReserve` keeps maximised windows clear of it.
class DesktopArea : public QMdiArea
{
    Q_OBJECT

public:
    explicit DesktopArea(QWidget* parent = nullptr);

    void setBottomReserve(int px);

    // Tiling: visible windows as equal-width columns with gaps, re-laid out
    // whenever a window opens, closes, hides or the desktop resizes.
    bool tiling() const { return m_tiling; }
    void setTiling(bool on);
    QRect maximizedRect() const;

    AppWindow* window(const QString& key) const;
    AppWindow* windowFor(const QWidget* content) const;
    QList<AppWindow*> appWindows() const;

    // Opens a new window for `content`, or returns the existing one for `key`.
    AppWindow* openWindow(const QString& key, const QString& title,
                          const QString& iconPath, QWidget* content);
    void restoreAndRaise(AppWindow* w);
    void minimize(AppWindow* w);
    // Removes the window, detaching its content first (see releaseContent).
    void removeWindow(AppWindow* w);

    QString activeKey() const;

    // Split view: two windows side by side (one above the other on a portrait
    // screen) with a draggable divider; the other windows stay behind them.
    // Pairs are remembered, most used first, and offered when a split starts.
    bool splitActive() const { return m_splitOn; }
    bool inSplit(const AppWindow* w) const;
    // `w` takes the first half; the other half offers partners (or takes the
    // only other window right away).
    void beginSplit(AppWindow* w);
    // Both windows must be open. ratio < 0: the remembered one for this pair.
    void setSplit(const QString& first, const QString& second, double ratio = -1);
    void exitSplit();
    // A divider position (share of the first side) kept within 0.2..0.8 and
    // pulled onto 1/3, 1/2 or 2/3 when close.
    static double snapSplitRatio(double r);

signals:
    void windowsChanged();
    void closeRequested(const QString& key);
    // A remembered partner that isn't open: MainContainer launches it, and the
    // split completes when its window arrives.
    void launchRequested(const QString& key);

signals:
    void tilingChanged(bool on);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    friend class SplitPicker;
    friend class SplitDivider;
    friend class DesktopAreaSplitTest;   // tests/desktop_split_test.cpp

    void onActivated(QMdiSubWindow* w);
    void retileSoon();
    void retile();

    struct SplitPair {
        QString a, b, aTitle, bTitle, aIcon, bIcon;
        double ratio = 0.5;
        int count = 0;
        qint64 last = 0;   // ms since epoch
        // The use count, halving every week since `last`; `now` < 0: the clock.
        double score(qint64 now = -1) const;
    };
    void layoutSplit();
    bool splitVertical() const;
    QRect splitRect(int side) const;   // 0: first, 1: second
    QRect dividerRect() const;
    void choosePartner(const QString& key);
    // Splits in place of the side the user is working in (opening another
    // app while split).
    void replaceActiveSide(const QString& key);
    // Saves the current pair (count it: a new split, not just a new ratio).
    void recordSplit(bool count);
    QList<SplitPair> loadPairs() const;
    void savePairs(QList<SplitPair> pairs) const;
    QList<SplitPair> partnersOf(const QString& key) const;   // as a=key, b=partner
    double rememberedRatio(const QString& first, const QString& second) const;

    bool m_splitOn = false;
    QString m_splitA, m_splitB;
    double m_splitRatio = 0.5;
    QString m_pendingA, m_pendingB;   // partner launched, waiting for its window
    SplitPicker* m_picker = nullptr;
    SplitDivider* m_divider = nullptr;

    QStringList m_order;   // window keys in opening order, for tiling
    bool m_tiling = false;
    bool m_retilePending = false;
    int m_reserve = 0;
    int m_cascade = 0;
};

// The desktop's top bar: the λ and "Logos Basecamp" on the left; a search
// hint (opens the Basecamp overlay), the clock and the Activity button (opens
// the Activity sidebar, basecamp-experiments#44) on the right.
class DesktopStatusBar : public QWidget
{
    Q_OBJECT

public:
    // Room for the 32 px Activity button with 2 px above and below.
    static constexpr int kHeight = 36;
    static constexpr int kActivitySize = 32;
    explicit DesktopStatusBar(QWidget* parent = nullptr);

    void setTiling(bool on) { m_tiling = on; update(); }
    // The sidebar is open: the button reads as pressed.
    void setActivityOpen(bool on) { if (m_activityOpen != on) { m_activityOpen = on; update(); } }
    // `pos` (this bar's coordinates) is on the Activity button.
    bool isOnActivity(const QPoint& pos) const { return activityRect().contains(pos); }

signals:
    void searchClicked();
    void tileClicked();
    void activityClicked();

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRect activityRect() const;
    QRect clockRect() const;
    QRect searchRect() const;
    QRect tileRect() const;
    bool m_tiling = false;
    bool m_tileHover = false;
    QPixmap m_lambda;
    QPixmap m_activityGlyph;
    bool m_searchHover = false;
    bool m_activityHover = false;
    bool m_activityOpen = false;
};

// Glass painted from C++ under an app's QML we don't own (Package Manager):
// its sandbox refuses image:// URLs, so the backdrop is drawn directly.
class GlassItem : public QQuickPaintedItem
{
    Q_OBJECT

public:
    GlassItem(DesktopRoot* root, QWidget* host, const QColor& tint, QQuickItem* parent);
    void paint(QPainter* painter) override;

private:
    QPointer<DesktopRoot> m_root;
    QPointer<QWidget> m_host;
    QColor m_tint;
};
