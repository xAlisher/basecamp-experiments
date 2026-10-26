#include "DesktopArea.h"

#include "AppTileIcon.h"

#include <algorithm>
#include <cmath>

#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QEnterEvent>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QStandardPaths>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QTimer>
#include <QPaintEvent>

#include <ThemeColors.h>

namespace {

QColor tok(const char* name, const char* fallback)
{
    return Logos::themeColor(QString::fromLatin1(name), QColor(QString::fromLatin1(fallback)));
}

// Window chrome over the lightly blurred wallpaper: nearly opaque, so the
// wallpaper only tints it, and a little more opaque on the active window.
QColor glassTint(bool active)
{
    QColor c = active ? tok("backgroundSecondary", "#262626") : tok("background", "#171717");
    c.setAlphaF(active ? 0.92 : 0.88);
    return c;
}

} // namespace

// ── Title bar ───────────────────────────────────────────────────────────────

// Icon and name on the left, split / minimise / maximise / close circles on the right.
// Drag to move the window, double-click to maximise.
class WindowTitleBar : public QWidget
{
public:
    static constexpr int kHeight = 36;

    enum Button { None = -1, Split = 0, Minimize = 1, Maximize = 2, Close = 3 };

    WindowTitleBar(AppWindow* window, const QString& title, const QString& iconPath)
        : QWidget(nullptr), m_window(window), m_title(title)
    {
        setFixedHeight(kHeight);
        setMouseTracking(true);
        // The app's tile, rounded like everywhere else (AppTileIcon.h).
        const QIcon icon = appTileIcon(iconPath, title, tok("surfaceRaised", "#232323"),
                                       tok("text", "#FFFFFF"));
        if (!icon.isNull())
            m_icon = icon.pixmap(QSize(18, 18), devicePixelRatioF());
    }

    void setActive(bool active) { m_active = active; update(); }

protected:
    QRect buttonRect(int b) const
    {
        const int d = 14, gap = 10, right = 14;
        const int x = width() - right - d - (Close - b) * (d + gap);
        return QRect(x, (height() - d) / 2, d, d);
    }

    int buttonAt(const QPoint& p) const
    {
        // The whole bar height and half the gaps: fingers on a tablet miss 14 px circles.
        for (int b = Split; b <= Close; ++b)
            if (buttonRect(b).adjusted(-5, -(height() - 14) / 2, 5, (height() - 14) / 2).contains(p))
                return b;
        return None;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        DesktopRoot::paintGlass(p, this, rect(), glassTint(m_active));

        int x = 12;
        if (!m_icon.isNull()) {
            p.drawPixmap(QRect(x, (height() - 18) / 2, 18, 18), m_icon);
            x += 26;
        }
        QFont f = font();
        f.setPixelSize(13);
        f.setWeight(QFont::Medium);
        p.setFont(f);
        p.setPen(m_active ? tok("text", "#ffffff") : tok("textSecondary", "#a0a0a0"));
        const QRect textRect(x, 0, buttonRect(Split).left() - x - 12, height());
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                   p.fontMetrics().elidedText(m_title, Qt::ElideRight, textRect.width()));

        for (int b = Split; b <= Close; ++b) {
            const QRectF r = buttonRect(b);
            const bool hot = m_hover == b;
            QColor fill = tok("surfaceRaised", "#333333");
            QColor glyph = tok("textSecondary", "#a0a0a0");
            if (hot) {
                fill = b == Close ? tok("error", "#e5484d") : tok("accentOrange", "#ff8800");
                glyph = tok("accentForeground", "#171717");
            }
            p.setPen(Qt::NoPen);
            p.setBrush(fill);
            p.drawEllipse(r);
            p.setPen(QPen(glyph, 1.4, Qt::SolidLine, Qt::RoundCap));
            p.setBrush(Qt::NoBrush);
            const QPointF c = r.center();
            if (b == Split) {
                p.drawRect(QRectF(c.x() - 4, c.y() - 3, 3.5, 6));
                p.drawRect(QRectF(c.x() + 0.5, c.y() - 3, 3.5, 6));
            } else if (b == Minimize) {
                p.drawLine(QPointF(c.x() - 3.5, c.y()), QPointF(c.x() + 3.5, c.y()));
            } else if (b == Maximize) {
                p.drawRect(QRectF(c.x() - 3, c.y() - 3, 6, 6));
            } else {
                p.drawLine(QPointF(c.x() - 3, c.y() - 3), QPointF(c.x() + 3, c.y() + 3));
                p.drawLine(QPointF(c.x() + 3, c.y() - 3), QPointF(c.x() - 3, c.y() + 3));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (m_dragging) {
            // Moving a window takes it (and its partner) out of a split.
            if (auto* area = qobject_cast<DesktopArea*>(m_window->mdiArea()); area && area->inSplit(m_window))
                area->exitSplit();
            if (m_window->isMaximizedOnDesktop()) {
                // Pull a maximised window out under the cursor, keeping the
                // grab point in proportion.
                const qreal ratio = qreal(e->position().x()) / qMax(1, width());
                m_window->setMaximizedOnDesktop(false);
                m_grab = QPoint(int(ratio * m_window->width()), int(e->position().y()));
            }
            const QPoint parentPos = m_window->parentWidget()
                ? m_window->parentWidget()->mapFromGlobal(e->globalPosition().toPoint())
                : e->globalPosition().toPoint();
            QPoint to = parentPos - m_grab;
            to.setY(qMax(0, to.y()));
            m_window->move(to);
            return;
        }
        const int h = buttonAt(e->position().toPoint());
        if (h != m_hover) {
            m_hover = h;
            setCursor(h == None ? Qt::ArrowCursor : Qt::PointingHandCursor);
            update();
        }
    }

    void leaveEvent(QEvent*) override
    {
        m_hover = None;
        update();
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        m_pressed = buttonAt(e->position().toPoint());
        if (m_pressed == None) {
            m_dragging = true;
            m_grab = e->position().toPoint();
        }
        if (QMdiArea* area = m_window->mdiArea())
            area->setActiveSubWindow(m_window);
        m_window->raise();
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        m_dragging = false;
        const int b = buttonAt(e->position().toPoint());
        if (b != None && b == m_pressed) {
            auto* area = qobject_cast<DesktopArea*>(m_window->mdiArea());
            if (b == Split)
                emit m_window->splitRequested(m_window->key());
            else if (b == Minimize)
                emit m_window->minimizeRequested(m_window->key());
            else if (b == Maximize) {
                if (area && area->inSplit(m_window))
                    area->exitSplit();
                m_window->toggleMaximized();
            }
            else
                emit m_window->closeRequested(m_window->key());
        }
        m_pressed = None;
    }

    void mouseDoubleClickEvent(QMouseEvent* e) override
    {
        if (buttonAt(e->position().toPoint()) == None) {
            if (auto* area = qobject_cast<DesktopArea*>(m_window->mdiArea()); area && area->inSplit(m_window))
                area->exitSplit();
            m_window->toggleMaximized();
        }
    }

private:
    AppWindow* m_window;
    QString m_title;
    QPixmap m_icon;
    bool m_active = false;
    bool m_dragging = false;
    QPoint m_grab;
    int m_hover = None;
    int m_pressed = None;
};

// ── Window frame ────────────────────────────────────────────────────────────

// A thin frame around the title bar and the app: the window's border, and the
// handle for resizing from any edge or corner.
class WindowFrame : public QWidget
{
public:
    static constexpr int kMargin = 4;      // frame thickness
    static constexpr int kCorner = 14;     // corner grab size along each edge

    explicit WindowFrame(AppWindow* window) : QWidget(nullptr), m_window(window)
    {
        setMouseTracking(true);
    }

    void setActive(bool active) { m_active = active; update(); }

protected:
    Qt::Edges edgesAt(const QPoint& p) const
    {
        const int m = kMargin + 2;
        const bool nearL = p.x() < m, nearR = p.x() >= width() - m;
        const bool nearT = p.y() < m, nearB = p.y() >= height() - m;
        // Near a corner, either edge band grabs both edges.
        const bool inL = p.x() < kCorner, inR = p.x() >= width() - kCorner;
        const bool inT = p.y() < kCorner, inB = p.y() >= height() - kCorner;
        Qt::Edges e;
        if (nearL || ((nearT || nearB) && inL)) e |= Qt::LeftEdge;
        if (nearR || ((nearT || nearB) && inR)) e |= Qt::RightEdge;
        if (nearT || ((nearL || nearR) && inT)) e |= Qt::TopEdge;
        if (nearB || ((nearL || nearR) && inB)) e |= Qt::BottomEdge;
        return e;
    }

    static Qt::CursorShape cursorFor(Qt::Edges e)
    {
        const bool l = e & Qt::LeftEdge, r = e & Qt::RightEdge, t = e & Qt::TopEdge, b = e & Qt::BottomEdge;
        if ((l && t) || (r && b)) return Qt::SizeFDiagCursor;
        if ((r && t) || (l && b)) return Qt::SizeBDiagCursor;
        if (l || r) return Qt::SizeHorCursor;
        if (t || b) return Qt::SizeVerCursor;
        return Qt::ArrowCursor;
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        DesktopRoot::paintGlass(p, this, rect(), glassTint(m_active));
        p.setPen(m_active ? tok("borderStrong", "#4a4a4a") : tok("border", "#333333"));
        p.drawRect(rect().adjusted(0, 0, -1, -1));
    }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton)
            return;
        m_edges = edgesAt(e->position().toPoint());
        if (!m_edges)
            return;
        if (m_window->isMaximizedOnDesktop())
            m_window->setMaximizedOnDesktop(false);
        m_pressGlobal = e->globalPosition().toPoint();
        m_startGeometry = m_window->geometry();
        if (QMdiArea* area = m_window->mdiArea())
            area->setActiveSubWindow(m_window);
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!m_edges) {
            setCursor(cursorFor(edgesAt(e->position().toPoint())));
            return;
        }
        const QPoint d = e->globalPosition().toPoint() - m_pressGlobal;
        QRect g = m_startGeometry;
        const int minW = 420, minH = 260;
        if (m_edges & Qt::LeftEdge)   g.setLeft(qMin(g.left() + d.x(), g.right() - minW));
        if (m_edges & Qt::RightEdge)  g.setRight(qMax(g.right() + d.x(), g.left() + minW));
        if (m_edges & Qt::TopEdge)    g.setTop(qMax(0, qMin(g.top() + d.y(), g.bottom() - minH)));
        if (m_edges & Qt::BottomEdge) g.setBottom(qMax(g.bottom() + d.y(), g.top() + minH));
        m_window->setGeometry(g);
    }

    void mouseReleaseEvent(QMouseEvent*) override { m_edges = {}; }

    void leaveEvent(QEvent*) override
    {
        if (!m_edges)
            unsetCursor();
    }

private:
    AppWindow* m_window;
    bool m_active = false;
    Qt::Edges m_edges;
    QPoint m_pressGlobal;
    QRect m_startGeometry;
};

// ── AppWindow ───────────────────────────────────────────────────────────────

AppWindow::AppWindow(const QString& key, const QString& title, const QString& iconPath,
                     QWidget* content, QWidget* parent)
    : QMdiSubWindow(parent, Qt::FramelessWindowHint)
    , m_key(key)
    , m_iconPath(iconPath)
    , m_content(content)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    // Opaque: the glass shows the wallpaper, never the windows beneath, so
    // moving a window doesn't need everything under it repainted.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAutoFillBackground(false);
    setWindowTitle(title);

    // The frame (border + resize handle), then the title bar and the app.
    m_frame = new WindowFrame(this);
    auto* layout = new QVBoxLayout(m_frame);
    const int m = WindowFrame::kMargin;
    layout->setContentsMargins(m, m, m, m);
    layout->setSpacing(0);
    m_titleBar = new WindowTitleBar(this, title, iconPath);
    layout->addWidget(m_titleBar);
    if (content) {
        content->setParent(m_frame);
        content->show();
        layout->addWidget(content, 1);
    }
    setWidget(m_frame);

    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { m_frame->update(); m_titleBar->update(); });
}

void AppWindow::setActiveLook(bool active)
{
    m_active = active;
    m_titleBar->setActive(active);
    m_frame->setActive(active);
}

void AppWindow::setMaximizedOnDesktop(bool on)
{
    if (on == m_maximized)
        return;
    auto* area = qobject_cast<DesktopArea*>(mdiArea());
    if (on) {
        m_normalGeometry = geometry();
        m_maximized = true;
        if (area)
            setGeometry(area->maximizedRect());
    } else {
        m_maximized = false;
        if (m_normalGeometry.isValid())
            setGeometry(m_normalGeometry);
    }
}

QWidget* AppWindow::releaseContent()
{
    QWidget* c = m_content;
    if (c) {
        c->hide();
        c->setParent(nullptr);
    }
    m_content = nullptr;
    return c;
}

void AppWindow::closeEvent(QCloseEvent* event)
{
    // Ctrl+F4 and friends: the owner decides (an app unloads, a system app hides).
    event->ignore();
    emit closeRequested(m_key);
}

void AppWindow::resizeEvent(QResizeEvent* event)
{
    QMdiSubWindow::resizeEvent(event);
}

void AppWindow::moveEvent(QMoveEvent* event)
{
    QMdiSubWindow::moveEvent(event);
    // The glass shows what is under it, which just changed.
    m_frame->update();
    m_titleBar->update();
}

// ── DesktopArea ─────────────────────────────────────────────────────────────

DesktopArea::DesktopArea(QWidget* parent)
    : QMdiArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setActivationOrder(QMdiArea::ActivationHistoryOrder);
    connect(this, &QMdiArea::subWindowActivated, this, &DesktopArea::onActivated);
    // See-through: DesktopRoot paints the wallpaper behind us.
    setBackground(Qt::NoBrush);
    setAutoFillBackground(false);
    viewport()->setAutoFillBackground(false);
}

void DesktopArea::setTiling(bool on)
{
    if (on == m_tiling)
        return;
    m_tiling = on;
    if (on) {
        exitSplit();
        for (AppWindow* w : appWindows())
            w->setMaximizedOnDesktop(false);
        retile();
    }
    emit tilingChanged(on);
}

void DesktopArea::retileSoon()
{
    if (!m_tiling || m_retilePending)
        return;
    m_retilePending = true;
    QTimer::singleShot(0, this, [this]() {
        m_retilePending = false;
        retile();
    });
}

void DesktopArea::retile()
{
    if (!m_tiling)
        return;
    QList<AppWindow*> shown;
    for (const QString& key : m_order)
        if (AppWindow* w = window(key); w && w->isVisible())
            shown << w;
    if (shown.isEmpty())
        return;
    const int gap = 10;
    const QRect area = maximizedRect();
    const int n = shown.size();
    const int colW = (area.width() - (n + 1) * gap) / n;
    for (int i = 0; i < n; ++i) {
        shown[i]->setMaximizedOnDesktop(false);
        shown[i]->setGeometry(area.x() + gap + i * (colW + gap), area.y() + gap,
                              colW, area.height() - 2 * gap);
    }
}

void DesktopArea::setBottomReserve(int px)
{
    m_reserve = px;
    for (AppWindow* w : appWindows())
        if (w->isMaximizedOnDesktop())
            w->setGeometry(maximizedRect());
    layoutSplit();
}

QRect DesktopArea::maximizedRect() const
{
    const QSize s = viewport()->size();
    return QRect(0, 0, s.width(), qMax(200, s.height() - m_reserve));
}

QList<AppWindow*> DesktopArea::appWindows() const
{
    QList<AppWindow*> out;
    for (QMdiSubWindow* w : subWindowList())
        if (auto* aw = qobject_cast<AppWindow*>(w))
            out << aw;
    return out;
}

AppWindow* DesktopArea::window(const QString& key) const
{
    for (AppWindow* w : appWindows())
        if (w->key() == key)
            return w;
    return nullptr;
}

AppWindow* DesktopArea::windowFor(const QWidget* content) const
{
    for (AppWindow* w : appWindows())
        if (w->content() == content)
            return w;
    return nullptr;
}

AppWindow* DesktopArea::openWindow(const QString& key, const QString& title,
                                   const QString& iconPath, QWidget* content)
{
    if (AppWindow* existing = window(key)) {
        restoreAndRaise(existing);
        return existing;
    }
    auto* w = new AppWindow(key, title, iconPath, content);
    addSubWindow(w);
    connect(w, &AppWindow::closeRequested, this, &DesktopArea::closeRequested);
    connect(w, &AppWindow::minimizeRequested, this, [this, w]() { minimize(w); });
    connect(w, &AppWindow::splitRequested, this, [this, w]() { beginSplit(w); });

    // About 70 % of the desktop, centred, each next one 32 px further on.
    const QRect area = maximizedRect();
    // Capped, so a very wide screen still gets a window-sized window.
    const QSize size(qMin(int(area.width() * 0.7), 1280), qMin(int(area.height() * 0.78), 860));
    QPoint pos((area.width() - size.width()) / 2, (area.height() - size.height()) / 2);
    pos += QPoint(32, 32) * (m_cascade % 6) - QPoint(80, 60);
    pos.setX(qMax(0, pos.x()));
    pos.setY(qMax(0, pos.y()));
    ++m_cascade;
    w->setGeometry(QRect(pos, size));
    w->show();
    setActiveSubWindow(w);
    m_order << key;
    retileSoon();
    // A launched partner arrived: complete its split. Any other app opened
    // while split takes the place of the side in use.
    if (key == m_pendingB && window(m_pendingA)) {
        const QString a = m_pendingA;
        QTimer::singleShot(0, this, [this, a, key]() { setSplit(a, key); });
    } else if (m_splitOn && m_splitB.isEmpty() && key != m_splitA) {
        // Opened from the dock while the picker waits: it takes the empty half.
        const QString a = m_splitA;
        QTimer::singleShot(0, this, [this, a, key]() { setSplit(a, key); });
    } else if (m_splitOn && !m_splitB.isEmpty()) {
        replaceActiveSide(key);
    }
    emit windowsChanged();
    return w;
}

void DesktopArea::restoreAndRaise(AppWindow* w)
{
    if (!w)
        return;
    w->show();
    w->raise();
    setActiveSubWindow(w);
    if (m_splitOn && m_splitB.isEmpty() && w->key() != m_splitA) {
        const QString a = m_splitA, b = w->key();
        QTimer::singleShot(0, this, [this, a, b]() { setSplit(a, b); });
    } else if (m_splitOn && !m_splitB.isEmpty() && !inSplit(w)) {
        replaceActiveSide(w->key());
    }
    retileSoon();
    emit windowsChanged();
}

void DesktopArea::minimize(AppWindow* w)
{
    if (!w)
        return;
    if (inSplit(w)) {
        AppWindow* other = window(w->key() == m_splitA ? m_splitB : m_splitA);
        exitSplit();
        if (other)
            other->setMaximizedOnDesktop(true);
    }
    w->hide();
    // The next window in history becomes active, as on a desktop.
    for (QMdiSubWindow* other : subWindowList(QMdiArea::ActivationHistoryOrder))
        if (other != w && other->isVisible())
            setActiveSubWindow(other);
    retileSoon();
    emit windowsChanged();
}

void DesktopArea::removeWindow(AppWindow* w)
{
    if (!w)
        return;
    if (inSplit(w)) {
        AppWindow* other = window(w->key() == m_splitA ? m_splitB : m_splitA);
        exitSplit();
        if (other)
            other->setMaximizedOnDesktop(true);
    }
    if (w->key() == m_pendingA || w->key() == m_pendingB)
        m_pendingA.clear(), m_pendingB.clear();
    w->releaseContent();
    m_order.removeAll(w->key());
    removeSubWindow(w);
    w->deleteLater();
    retileSoon();
    emit windowsChanged();
}

QString DesktopArea::activeKey() const
{
    auto* w = qobject_cast<AppWindow*>(activeSubWindow());
    return w && w->isVisible() ? w->key() : QString();
}

void DesktopArea::onActivated(QMdiSubWindow* active)
{
    for (AppWindow* w : appWindows())
        w->setActiveLook(w == active);
    emit windowsChanged();
}

void DesktopArea::resizeEvent(QResizeEvent* event)
{
    QMdiArea::resizeEvent(event);
    retileSoon();
    layoutSplit();
    for (AppWindow* w : appWindows())
        if (w->isMaximizedOnDesktop())
            w->setGeometry(maximizedRect());
}

// ── DesktopStatusBar ────────────────────────────────────────────────────────

#include <QDateTime>
#include <QHelpEvent>
#include <QToolTip>
#include <QTimer>

DesktopStatusBar::DesktopStatusBar(QWidget* parent)
    : QWidget(parent)
{
    setFixedHeight(kHeight);
    setMouseTracking(true);
    // The clock shows minutes, so repaint it on the minute, not every second:
    // with a Qt Quick view in the window, each repaint costs a whole-window
    // composite, and an idle desktop never stopped drawing (basecamp-theming#20).
    auto* tick = new QTimer(this);
    tick->setSingleShot(true);
    tick->setTimerType(Qt::PreciseTimer);   // a coarse timer may fire seconds early
    auto arm = [tick]() {
        const QTime now = QTime::currentTime();
        tick->start(60000 - (now.second() * 1000 + now.msec()) + 20);
    };
    connect(tick, &QTimer::timeout, this, [this, arm]() {
        update(clockRect());
        arm();
    });
    arm();
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { m_lambda = QPixmap(); m_activityGlyph = QPixmap(); update(); });
}

QRect DesktopStatusBar::activityRect() const
{
    // At the right edge, where the sidebar it opens comes in.
    const int s = kActivitySize;
    return QRect(width() - 8 - s, (height() - s) / 2, s, s);
}

QRect DesktopStatusBar::clockRect() const
{
    return QRect(activityRect().left() - 8 - 150, 0, 150, height());
}

QRect DesktopStatusBar::tileRect() const
{
    const QRect sr = searchRect();
    return QRect(sr.left() - 8 - 28, sr.top(), 28, sr.height());
}

QRect DesktopStatusBar::searchRect() const
{
    // Left of the clock, sized for "Search  Ctrl K".
    return QRect(clockRect().left() - 16 - 120, 4, 120, height() - 8);
}

void DesktopStatusBar::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor tint = tok("backgroundSecondary", "#262626");
    tint.setAlphaF(0.85);
    DesktopRoot::paintGlass(p, this, rect(), tint);
    p.setPen(tok("borderHairline", "#2a2a2a"));
    p.drawLine(0, height() - 1, width(), height() - 1);

    // The λ in the text colour (the asset is white).
    const QColor text = tok("text", "#ffffff");
    if (m_lambda.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QPixmap src = QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/logos-lambda.svg"))
                          .pixmap(QSize(12, 16), dpr);
        if (!src.isNull()) {
            QPainter tp(&src);
            tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tp.fillRect(src.rect(), text);
        }
        m_lambda = src;
    }
    int x = 16;
    if (!m_lambda.isNull()) {
        p.drawPixmap(QRect(x, (height() - 16) / 2, 12, 16), m_lambda);
        x += 22;
    }
    QFont f = font();
    f.setPixelSize(13);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(text);
    p.drawText(QRect(x, 0, 300, height()), Qt::AlignVCenter | Qt::AlignLeft, tr("Logos Basecamp"));

    // Search hint.
    const QRect sr = searchRect();
    if (m_searchHover) {
        p.setPen(Qt::NoPen);
        p.setBrush(tok("surfaceRaised", "#333333"));
        p.drawRoundedRect(sr, 6, 6);
    }
    f.setWeight(QFont::Normal);
    f.setPixelSize(12);
    p.setFont(f);
    p.setPen(tok("textSecondary", "#a0a0a0"));
    p.drawText(sr, Qt::AlignCenter, tr("Search   Ctrl K"));

    // Tiling toggle: three columns; accent while on.
    const QRect tr_ = tileRect();
    if (m_tileHover || m_tiling) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_tiling ? tok("accentOrange", "#ff8800") : tok("surfaceRaised", "#333333"));
        p.drawRoundedRect(tr_, 6, 6);
    }
    {
        const QColor ink = m_tiling ? tok("accentForeground", "#171717") : tok("textSecondary", "#a0a0a0");
        p.setPen(QPen(ink, 1.2));
        p.setBrush(Qt::NoBrush);
        const QRectF g(tr_.center().x() - 7.5, tr_.center().y() - 5.5, 15, 11);
        p.drawRoundedRect(g, 1.5, 1.5);
        p.drawLine(QPointF(g.left() + 5, g.top()), QPointF(g.left() + 5, g.bottom()));
        p.drawLine(QPointF(g.left() + 10, g.top()), QPointF(g.left() + 10, g.bottom()));
    }

    // Clock.
    f.setWeight(QFont::Medium);
    f.setPixelSize(13);
    p.setFont(f);
    p.setPen(text);
    p.drawText(clockRect(), Qt::AlignVCenter | Qt::AlignRight,
               QLocale().toString(QDateTime::currentDateTime(), QStringLiteral("ddd d MMM  HH:mm")));

    // Activity: a pulse line, on a raised square while hovered or open.
    const QRect ar = activityRect();
    if (m_activityHover || m_activityOpen) {
        p.setPen(Qt::NoPen);
        p.setBrush(tok("surfaceRaised", "#333333"));
        p.drawRoundedRect(ar, 6, 6);
    }
    if (m_activityGlyph.isNull()) {
        const qreal dpr = devicePixelRatioF();
        QPixmap src = QIcon(QStringLiteral(":/qt/qml/Basecamp/Icons/Basecamp/Icons/activity.svg"))
                          .pixmap(QSize(18, 18), dpr);
        if (!src.isNull()) {
            QPainter tp(&src);
            tp.setCompositionMode(QPainter::CompositionMode_SourceIn);
            tp.fillRect(src.rect(), text);
        }
        m_activityGlyph = src;
    }
    if (!m_activityGlyph.isNull())
        p.drawPixmap(QRect(ar.center().x() - 8, ar.center().y() - 8, 18, 18), m_activityGlyph);
}

bool DesktopStatusBar::event(QEvent* e)
{
    if (e->type() == QEvent::ToolTip) {
        auto* he = static_cast<QHelpEvent*>(e);
        if (activityRect().contains(he->pos()))
            QToolTip::showText(he->globalPos(), tr("Activity"), this, activityRect());
        else if (searchRect().contains(he->pos()))
            QToolTip::showText(he->globalPos(), tr("Search Basecamp"), this, searchRect());
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(e);
}

void DesktopStatusBar::mouseMoveEvent(QMouseEvent* e)
{
    const bool hover = searchRect().contains(e->position().toPoint());
    const bool tileHover = tileRect().contains(e->position().toPoint());
    const bool activityHover = activityRect().contains(e->position().toPoint());
    if (hover != m_searchHover || tileHover != m_tileHover || activityHover != m_activityHover) {
        m_searchHover = hover;
        m_tileHover = tileHover;
        m_activityHover = activityHover;
        setCursor(hover || tileHover || activityHover ? Qt::PointingHandCursor : Qt::ArrowCursor);
        update();
    }
}

void DesktopStatusBar::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && searchRect().contains(e->position().toPoint()))
        emit searchClicked();
    else if (e->button() == Qt::LeftButton && tileRect().contains(e->position().toPoint()))
        emit tileClicked();
    else if (e->button() == Qt::LeftButton && activityRect().contains(e->position().toPoint()))
        emit activityClicked();
}

void DesktopStatusBar::leaveEvent(QEvent*)
{
    m_searchHover = false;
    m_tileHover = false;
    m_activityHover = false;
    update();
}


// ── DesktopRoot ─────────────────────────────────────────────────────────────

namespace {

// A cheap, smooth blur: shrink hard, then scale back up twice.
QImage blurred(const QImage& src, int factor)
{
    if (src.isNull())
        return src;
    const QSize small(qMax(1, src.width() / factor), qMax(1, src.height() / factor));
    QImage s = src.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    s = s.scaled(small * 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return s.scaled(src.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

} // namespace

DesktopRoot::DesktopRoot(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent);
    connect(Logos::ThemeNotifier::instance(), &Logos::ThemeNotifier::currentChanged,
            this, [this]() { rebuild(); });
}

void DesktopRoot::setWallpaper(const QString& path, const QString& fit)
{
    m_source = path.isEmpty() ? QPixmap() : QPixmap(path);
    m_fit = fit;
    rebuild();
}

void DesktopRoot::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    rebuild();
}

void DesktopRoot::rebuild()
{
    const qreal dpr = devicePixelRatioF();
    const QSize target = size() * dpr;
    if (target.isEmpty())
        return;
    QImage out(target, QImage::Format_ARGB32_Premultiplied);
    out.fill(tok("background", "#171717"));
    if (!m_source.isNull()) {
        const QImage src = m_source.toImage();
        QPainter p(&out);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        auto cover = [&]() {
            const QImage big = src.scaled(target, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            return big.copy((big.width() - target.width()) / 2, (big.height() - target.height()) / 2,
                            target.width(), target.height());
        };
        if (m_fit == QLatin1String("stretch")) {
            p.drawImage(0, 0, src.scaled(target, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        } else if (m_fit == QLatin1String("fill")) {
            p.drawImage(0, 0, cover());
        } else {
            // Fit: the whole image, centred; the spare room is the same image
            // blurred and dimmed, so there are no bars.
            p.drawImage(0, 0, blurred(cover(), 24));
            p.fillRect(out.rect(), QColor(0, 0, 0, 90));
            const QImage fitted = src.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawImage((target.width() - fitted.width()) / 2, (target.height() - fitted.height()) / 2, fitted);
        }
    }
    m_composed = QPixmap::fromImage(out);
    m_composed.setDevicePixelRatio(dpr);
    m_blurredImage = blurred(out, 6);
    m_blurred = QPixmap::fromImage(m_blurredImage);
    m_blurred.setDevicePixelRatio(dpr);
    ++m_revision;
    emit backdropChanged();
    update();
    // Everything frosted shows the new backdrop.
    for (QWidget* w : findChildren<QWidget*>())
        w->update();
}

void DesktopRoot::paintEvent(QPaintEvent* event)
{
    QPainter p(this);
    if (m_composed.isNull())
        p.fillRect(event->rect(), tok("background", "#171717"));
    else
        p.drawPixmap(event->rect(), m_composed,
                     QRectF(QPointF(event->rect().topLeft()) * m_composed.devicePixelRatio(),
                            QSizeF(event->rect().size()) * m_composed.devicePixelRatio()));
}

void DesktopRoot::paintGlass(QPainter& p, QWidget* w, const QRect& r, const QColor& tint)
{
    DesktopRoot* root = nullptr;
    for (QWidget* a = w; a; a = a->parentWidget())
        if ((root = qobject_cast<DesktopRoot*>(a)))
            break;
    if (root && !root->m_blurred.isNull()) {
        const QPoint at = w->mapTo(root, r.topLeft());
        const qreal dpr = root->m_blurred.devicePixelRatio();
        p.drawPixmap(r, root->m_blurred,
                     QRectF(QPointF(at) * dpr, QSizeF(r.size()) * dpr));
    }
    p.fillRect(r, tint);
}


// ── QML glass ───────────────────────────────────────────────────────────────

GlassInfo::GlassInfo(DesktopRoot* root, QWidget* widget, QObject* parent)
    : QObject(parent), m_root(root), m_widget(widget)
{
    if (root)
        connect(root, &DesktopRoot::backdropChanged, this, &GlassInfo::refresh);
    // The widget moves when its window moves; watch every ancestor.
    for (QWidget* w = widget; w; w = w->parentWidget())
        w->installEventFilter(this);
    refresh();
}

int GlassInfo::rootWidth() const { return m_root ? m_root->width() : 0; }
int GlassInfo::rootHeight() const { return m_root ? m_root->height() : 0; }
int GlassInfo::revision() const { return m_root ? m_root->revision() : 0; }

bool GlassInfo::eventFilter(QObject* watched, QEvent* event)
{
    switch (event->type()) {
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::ParentChange:
        if (event->type() == QEvent::ParentChange && m_widget) {
            // Re-parented into a new window: watch the new ancestors.
            for (QWidget* w = m_widget; w; w = w->parentWidget())
                w->installEventFilter(this);
        }
        refreshSoon();
        break;
    default:
        break;
    }
    return QObject::eventFilter(watched, event);
}

// A drag sends many move events per frame (one per watched ancestor, at the
// mouse's rate); follow them once per frame.
void GlassInfo::refreshSoon()
{
    if (m_pending)
        return;
    m_pending = true;
    QTimer::singleShot(16, this, [this]() {
        m_pending = false;
        refresh();
    });
}

void GlassInfo::refresh()
{
    if (!m_root || !m_widget)
        return;
    // Works for widgets outside the root too (the Basecamp overlay floats over it).
    m_offset = m_widget->mapToGlobal(QPoint(0, 0)) - m_root->mapToGlobal(QPoint(0, 0));
    emit changed();
}

QImage BackdropProvider::requestImage(const QString&, QSize* size, const QSize&)
{
    const QImage img = m_root ? m_root->blurredImage() : QImage();
    if (size)
        *size = img.size();
    return img;
}

GlassItem::GlassItem(DesktopRoot* root, QWidget* host, const QColor& tint, QQuickItem* parent)
    : QQuickPaintedItem(parent), m_root(root), m_host(host), m_tint(tint)
{
    setZ(-1000);
    auto fit = [this]() {
        if (parentItem())
            setSize(parentItem()->size());
    };
    fit();
    connect(parent, &QQuickItem::widthChanged, this, fit);
    connect(parent, &QQuickItem::heightChanged, this, fit);
    // Repaint when the window moves or the backdrop changes.
    auto* info = new GlassInfo(root, host, this);
    connect(info, &GlassInfo::changed, this, [this]() { update(); });
}

void GlassItem::paint(QPainter* painter)
{
    const QRectF r(0, 0, width(), height());
    if (m_root && m_host && !m_root->blurredPixmap().isNull()) {
        const QPoint at = m_host->mapToGlobal(QPoint(0, 0)) - m_root->mapToGlobal(QPoint(0, 0));
        const QPixmap& pix = m_root->blurredPixmap();
        const qreal dpr = pix.devicePixelRatio();
        painter->drawPixmap(r, pix, QRectF(QPointF(at) * dpr, r.size() * dpr));
    }
    painter->fillRect(r, m_tint);
}

// ── Split view ──────────────────────────────────────────────────────────────

namespace {
constexpr int kSplitGap = 10;
}

// The handle between the two halves: drag to resize (snaps to thirds and
// half), double-click to swap the sides.
class SplitDivider : public QWidget
{
public:
    explicit SplitDivider(DesktopArea* area)
        : QWidget(area->viewport()), m_area(area)
    {
        setMouseTracking(true);
        setAttribute(Qt::WA_TranslucentBackground);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const bool vertical = m_area->splitVertical();
        const QSizeF pill = vertical ? QSizeF(48, 4) : QSizeF(4, 48);
        QRectF r(QPointF(0, 0), pill);
        r.moveCenter(QRectF(rect()).center());
        QColor c = (m_hover || m_drag) ? tok("accentOrange", "#ff8800") : tok("textSecondary", "#a0a0a0");
        if (!m_hover && !m_drag)
            c.setAlphaF(0.7);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(r, 2, 2);
    }

    void enterEvent(QEnterEvent*) override { m_hover = true; update(); }
    void leaveEvent(QEvent*) override { m_hover = false; update(); }

    void mousePressEvent(QMouseEvent* e) override
    {
        if (e->button() == Qt::LeftButton)
            m_drag = true;
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (!m_drag)
            return;
        const QRect a = m_area->maximizedRect().adjusted(kSplitGap, kSplitGap, -kSplitGap, -kSplitGap);
        const QPoint pos = mapTo(m_area->viewport(), e->position().toPoint());
        double r = m_area->splitVertical() ? double(pos.y() - a.top()) / qMax(1, a.height())
                                           : double(pos.x() - a.left()) / qMax(1, a.width());
        m_area->m_splitRatio = DesktopArea::snapSplitRatio(r);
        m_area->layoutSplit();
    }

    void mouseReleaseEvent(QMouseEvent*) override
    {
        if (m_drag) {
            m_drag = false;
            m_area->recordSplit(false);
            update();
        }
    }

    void mouseDoubleClickEvent(QMouseEvent*) override
    {
        std::swap(m_area->m_splitA, m_area->m_splitB);
        m_area->m_splitRatio = 1.0 - m_area->m_splitRatio;
        m_area->layoutSplit();
        m_area->recordSplit(false);
    }

private:
    DesktopArea* m_area;
    bool m_hover = false;
    bool m_drag = false;
};

// The empty half while a split starts: the window's usual partners first
// (remembered pairs, opened if they aren't), then the other open windows.
class SplitPicker : public QWidget
{
public:
    SplitPicker(DesktopArea* area, const QString& forKey)
        : QWidget(area->viewport()), m_area(area), m_forKey(forKey)
    {
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setAttribute(Qt::WA_OpaquePaintEvent);
        rebuild();
    }

    void rebuild()
    {
        m_rows.clear();
        QStringList seen{ m_forKey };
        const auto add = [this, &seen](const QString& key, const QString& title, const QString& icon) {
            if (seen.contains(key))
                return;
            seen << key;
            Row row;
            row.key = key;
            AppWindow* w = m_area->window(key);
            row.title = w ? w->windowTitle() : title;
            // The app's tile, as in the title bar (AppTileIcon.h).
            const QIcon i = appTileIcon(w ? w->iconPath() : icon, row.title,
                                        tok("surfaceRaised", "#232323"), tok("text", "#FFFFFF"));
            if (!i.isNull())
                row.icon = i.pixmap(QSize(24, 24), devicePixelRatioF());
            row.open = w != nullptr;
            m_rows << row;
        };
        for (const auto& pair : m_area->partnersOf(m_forKey))
            add(pair.b, pair.bTitle, pair.bIcon);
        m_usual = m_rows.size();
        for (AppWindow* w : m_area->appWindows())
            add(w->key(), w->windowTitle(), w->iconPath());
        update();
    }

    void setStatus(const QString& text)
    {
        m_status = text;
        update();
    }

protected:
    static constexpr int kMargin = 24, kTitle = 22, kHeader = 36, kRow = 48;

    QRect closeRect() const { return QRect(width() - kMargin - 20, kMargin, 20, 20); }

    QRect rowRect(int i) const
    {
        // title, a header, the usual partners, a second header, the open windows
        int y = kMargin + kTitle + kHeader + i * kRow;
        if (m_usual > 0 && i >= m_usual)
            y += kHeader;
        return QRect(kMargin, y, width() - 2 * kMargin, kRow - 6);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        DesktopRoot::paintGlass(p, this, rect(), glassTint(true));

        QFont title = font();
        title.setPixelSize(15);
        title.setWeight(QFont::DemiBold);
        QFont label = font();
        label.setPixelSize(12);
        QFont item = font();
        item.setPixelSize(14);

        p.setFont(title);
        p.setPen(tok("text", "#ffffff"));
        p.drawText(QRect(kMargin, kMargin, width() - 2 * kMargin, kTitle), Qt::AlignLeft | Qt::AlignVCenter,
                   m_status.isEmpty() ? QStringLiteral("Split with…") : m_status);
        // close: back to a single window
        p.setPen(QPen(m_hoverClose ? tok("accentOrange", "#ff8800") : tok("textSecondary", "#a0a0a0"), 1.6));
        const QRectF c = QRectF(closeRect()).adjusted(5, 5, -5, -5);
        p.drawLine(c.topLeft(), c.bottomRight());
        p.drawLine(c.topRight(), c.bottomLeft());

        // A section header sits right above its first row.
        const auto header = [&](int firstRow, const QString& text) {
            p.setFont(label);
            p.setPen(tok("textSecondary", "#a0a0a0"));
            p.drawText(QRect(kMargin, rowRect(firstRow).top() - kHeader, width() - 2 * kMargin, kHeader - 8),
                       Qt::AlignLeft | Qt::AlignBottom, text);
        };
        if (m_rows.isEmpty()) {
            p.setFont(item);
            p.setPen(tok("textSecondary", "#a0a0a0"));
            p.drawText(rect().adjusted(kMargin, 0, -kMargin, 0), Qt::AlignCenter | Qt::TextWordWrap,
                       QStringLiteral("Open an app from the dock to put it here."));
            return;
        }
        if (m_usual > 0)
            header(0, QStringLiteral("USUALLY WITH"));
        if (m_rows.size() > m_usual)
            header(m_usual, QStringLiteral("OPEN WINDOWS"));

        for (int i = 0; i < m_rows.size(); ++i) {
            const Row& row = m_rows[i];
            const QRect r = rowRect(i);
            if (i == m_hover) {
                p.setPen(Qt::NoPen);
                p.setBrush(tok("surfaceRaised", "#333333"));
                p.drawRoundedRect(r, 8, 8);
            }
            int x = r.left() + 12;
            if (!row.icon.isNull())
                p.drawPixmap(QRect(x, r.center().y() - 12, 24, 24), row.icon);
            x += 36;
            p.setFont(item);
            p.setPen(tok("text", "#ffffff"));
            p.drawText(QRect(x, r.top(), r.right() - x - 80, r.height()), Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(row.title, Qt::ElideRight, r.right() - x - 80));
            if (!row.open) {
                p.setFont(label);
                p.setPen(tok("textSecondary", "#a0a0a0"));
                p.drawText(QRect(r.right() - 80, r.top(), 68, r.height()), Qt::AlignRight | Qt::AlignVCenter,
                           QStringLiteral("closed"));
            }
        }
    }

    int rowAt(const QPoint& pos) const
    {
        for (int i = 0; i < m_rows.size(); ++i)
            if (rowRect(i).contains(pos))
                return i;
        return -1;
    }

    void mouseMoveEvent(QMouseEvent* e) override
    {
        const int h = rowAt(e->position().toPoint());
        const bool hc = closeRect().adjusted(-6, -6, 6, 6).contains(e->position().toPoint());
        if (h != m_hover || hc != m_hoverClose) {
            m_hover = h;
            m_hoverClose = hc;
            setCursor(h >= 0 || hc ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }

    void leaveEvent(QEvent*) override
    {
        m_hover = -1;
        m_hoverClose = false;
        update();
    }

    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (closeRect().adjusted(-6, -6, 6, 6).contains(e->position().toPoint())) {
            m_area->exitSplit();
            return;
        }
        const int i = rowAt(e->position().toPoint());
        if (i >= 0)
            m_area->choosePartner(m_rows[i].key);
    }

    void keyPressEvent(QKeyEvent* e) override
    {
        if (e->key() == Qt::Key_Escape)
            m_area->exitSplit();
        else
            QWidget::keyPressEvent(e);
    }

private:
    struct Row {
        QString key, title;
        QPixmap icon;
        bool open = false;
    };
    DesktopArea* m_area;
    QString m_forKey;
    QList<Row> m_rows;
    int m_usual = 0;
    int m_hover = -1;
    bool m_hoverClose = false;
    QString m_status;
};

double DesktopArea::snapSplitRatio(double r)
{
    r = qBound(0.2, r, 0.8);
    for (double snap : { 1.0 / 3, 0.5, 2.0 / 3 })
        if (qAbs(r - snap) < 0.025)
            return snap;
    return r;
}

double DesktopArea::SplitPair::score(qint64 now) const
{
    // Used often and lately: the count, fading by half every week.
    if (now < 0)
        now = QDateTime::currentMSecsSinceEpoch();
    const double days = (now - last) / 86400000.0;
    return count * std::pow(0.5, qMax(0.0, days) / 7.0);
}

bool DesktopArea::inSplit(const AppWindow* w) const
{
    return m_splitOn && w && (w->key() == m_splitA || w->key() == m_splitB);
}

bool DesktopArea::splitVertical() const
{
    const QRect a = maximizedRect();
    return a.height() > a.width();
}

QRect DesktopArea::splitRect(int side) const
{
    const QRect a = maximizedRect().adjusted(kSplitGap, kSplitGap, -kSplitGap, -kSplitGap);
    if (splitVertical()) {
        const int h = a.height() - kSplitGap;
        const int first = int(h * m_splitRatio);
        return side == 0 ? QRect(a.left(), a.top(), a.width(), first)
                         : QRect(a.left(), a.top() + first + kSplitGap, a.width(), h - first);
    }
    const int w = a.width() - kSplitGap;
    const int first = int(w * m_splitRatio);
    return side == 0 ? QRect(a.left(), a.top(), first, a.height())
                     : QRect(a.left() + first + kSplitGap, a.top(), w - first, a.height());
}

QRect DesktopArea::dividerRect() const
{
    // The gap plus a finger's width over both windows' edges (it sits above them).
    const int grab = 12;
    const QRect r = splitRect(0);
    return splitVertical() ? QRect(r.left(), r.bottom() + 1 - grab, r.width(), kSplitGap + 2 * grab)
                           : QRect(r.right() + 1 - grab, r.top(), kSplitGap + 2 * grab, r.height());
}

void DesktopArea::layoutSplit()
{
    if (!m_splitOn) {
        if (m_divider)
            m_divider->hide();
        return;
    }
    AppWindow* a = window(m_splitA);
    if (!a) {
        exitSplit();
        return;
    }
    a->setMaximizedOnDesktop(false);
    a->setGeometry(splitRect(0));
    a->show();
    a->raise();
    if (AppWindow* b = window(m_splitB)) {
        b->setMaximizedOnDesktop(false);
        b->setGeometry(splitRect(1));
        b->show();
        b->raise();
        if (!m_divider)
            m_divider = new SplitDivider(this);
        m_divider->setCursor(splitVertical() ? Qt::SplitVCursor : Qt::SplitHCursor);
        m_divider->setGeometry(dividerRect());
        m_divider->show();
        m_divider->raise();
    } else {
        if (m_divider)
            m_divider->hide();
        if (!m_picker) {
            m_picker = new SplitPicker(this, m_splitA);
            m_picker->show();
            m_picker->setFocus();
        }
        m_picker->setGeometry(splitRect(1));
        m_picker->raise();
    }
}

void DesktopArea::beginSplit(AppWindow* w)
{
    if (!w)
        return;
    setTiling(false);
    exitSplit();
    m_splitOn = true;
    m_splitA = w->key();
    m_splitRatio = 0.5;
    // Nothing to choose from: the only other window joins right away.
    QList<AppWindow*> others;
    for (AppWindow* o : appWindows())
        if (o != w)
            others << o;
    if (others.size() == 1 && partnersOf(m_splitA).isEmpty()) {
        setSplit(m_splitA, others.first()->key());
        return;
    }
    layoutSplit();
    setActiveSubWindow(w);
}

void DesktopArea::setSplit(const QString& first, const QString& second, double ratio)
{
    AppWindow* a = window(first);
    AppWindow* b = window(second);
    if (!a || !b || a == b)
        return;
    setTiling(false);
    m_pendingA.clear();
    m_pendingB.clear();
    if (m_picker) {
        m_picker->deleteLater();
        m_picker = nullptr;
    }
    m_splitOn = true;
    m_splitA = first;
    m_splitB = second;
    m_splitRatio = ratio > 0 ? ratio : rememberedRatio(first, second);
    layoutSplit();
    setActiveSubWindow(a);
    recordSplit(true);
    emit windowsChanged();
}

void DesktopArea::exitSplit()
{
    if (!m_splitOn && !m_picker)
        return;
    m_splitOn = false;
    m_splitA.clear();
    m_splitB.clear();
    m_pendingA.clear();
    m_pendingB.clear();
    if (m_picker) {
        m_picker->deleteLater();
        m_picker = nullptr;
    }
    if (m_divider)
        m_divider->hide();
}

void DesktopArea::choosePartner(const QString& key)
{
    if (!m_splitOn || m_splitA.isEmpty())
        return;
    if (window(key)) {
        setSplit(m_splitA, key);
        return;
    }
    m_pendingA = m_splitA;
    m_pendingB = key;
    if (m_picker) {
        QString title = key;
        for (const auto& pair : partnersOf(m_splitA))
            if (pair.b == key)
                title = pair.bTitle;
        m_picker->setStatus(QStringLiteral("Opening %1…").arg(title));
    }
    emit launchRequested(key);
}

void DesktopArea::replaceActiveSide(const QString& key)
{
    if (!m_splitOn || m_splitB.isEmpty() || key == m_splitA || key == m_splitB || !window(key))
        return;
    QMdiSubWindow* active = activeSubWindow();
    const bool firstActive = active && qobject_cast<AppWindow*>(active)
        && static_cast<AppWindow*>(active)->key() == m_splitA;
    // `active` may already be the new window; then the side last used before
    // it is the first one in activation history among the two.
    bool replaceFirst = firstActive;
    if (!active || (static_cast<AppWindow*>(active)->key() != m_splitA
                    && static_cast<AppWindow*>(active)->key() != m_splitB)) {
        for (QMdiSubWindow* sw : subWindowList(QMdiArea::ActivationHistoryOrder)) {
            auto* aw = qobject_cast<AppWindow*>(sw);
            if (aw && aw->key() == m_splitA)
                replaceFirst = true;
            else if (aw && aw->key() == m_splitB)
                replaceFirst = false;
        }
    }
    const QString a = replaceFirst ? key : m_splitA;
    const QString b = replaceFirst ? m_splitB : key;
    QTimer::singleShot(0, this, [this, a, b]() { setSplit(a, b); });
}

namespace {
QString splitPairsPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
        + QStringLiteral("/split-pairs.json");
}
} // namespace

QList<DesktopArea::SplitPair> DesktopArea::loadPairs() const
{
    QList<SplitPair> pairs;
    QFile f(splitPairsPath());
    if (!f.open(QIODevice::ReadOnly))
        return pairs;
    for (const QJsonValue v : QJsonDocument::fromJson(f.readAll()).array()) {
        const QJsonObject o = v.toObject();
        SplitPair p;
        p.a = o.value("a").toString();
        p.b = o.value("b").toString();
        p.aTitle = o.value("aTitle").toString();
        p.bTitle = o.value("bTitle").toString();
        p.aIcon = o.value("aIcon").toString();
        p.bIcon = o.value("bIcon").toString();
        p.ratio = o.value("ratio").toDouble(0.5);
        p.count = o.value("count").toInt();
        p.last = qint64(o.value("last").toDouble());
        if (!p.a.isEmpty() && !p.b.isEmpty())
            pairs << p;
    }
    return pairs;
}

void DesktopArea::savePairs(QList<SplitPair> pairs) const
{
    std::sort(pairs.begin(), pairs.end(), [](const SplitPair& x, const SplitPair& y) { return x.score() > y.score(); });
    while (pairs.size() > 30)
        pairs.removeLast();
    QJsonArray arr;
    for (const SplitPair& p : pairs)
        arr << QJsonObject{ { "a", p.a }, { "b", p.b }, { "aTitle", p.aTitle }, { "bTitle", p.bTitle },
                            { "aIcon", p.aIcon }, { "bIcon", p.bIcon }, { "ratio", p.ratio },
                            { "count", p.count }, { "last", double(p.last) } };
    QDir().mkpath(QFileInfo(splitPairsPath()).absolutePath());
    QFile f(splitPairsPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void DesktopArea::recordSplit(bool count)
{
    AppWindow* a = window(m_splitA);
    AppWindow* b = window(m_splitB);
    if (!m_splitOn || !a || !b)
        return;
    QList<SplitPair> pairs = loadPairs();
    SplitPair* entry = nullptr;
    for (SplitPair& p : pairs)
        if ((p.a == m_splitA && p.b == m_splitB) || (p.a == m_splitB && p.b == m_splitA))
            entry = &p;
    if (!entry) {
        pairs << SplitPair{};
        entry = &pairs.last();
    }
    entry->a = m_splitA;
    entry->b = m_splitB;
    entry->aTitle = a->windowTitle();
    entry->bTitle = b->windowTitle();
    entry->aIcon = a->iconPath();
    entry->bIcon = b->iconPath();
    entry->ratio = m_splitRatio;
    if (count)
        ++entry->count;
    entry->last = QDateTime::currentMSecsSinceEpoch();
    savePairs(pairs);
}

QList<DesktopArea::SplitPair> DesktopArea::partnersOf(const QString& key) const
{
    QList<SplitPair> out;
    for (SplitPair p : loadPairs()) {
        if (p.b == key) {
            std::swap(p.a, p.b);
            std::swap(p.aTitle, p.bTitle);
            std::swap(p.aIcon, p.bIcon);
            p.ratio = 1.0 - p.ratio;
        }
        if (p.a == key)
            out << p;
    }
    std::sort(out.begin(), out.end(), [](const SplitPair& x, const SplitPair& y) { return x.score() > y.score(); });
    return out;
}

double DesktopArea::rememberedRatio(const QString& first, const QString& second) const
{
    for (const SplitPair& p : partnersOf(first))
        if (p.b == second)
            return qBound(0.2, p.ratio, 0.8);
    return 0.5;
}
