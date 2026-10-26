#pragma once

#include <QRect>
#include <QSize>

#include <algorithm>

// The desktop layout's Basecamp overlay (basecamp-experiments#43): where its
// panel sits in a window of `area`. Centred; by default 70% of the width kept
// between 840 and 1000 px, and never closer than 24 px to an edge. The height
// runs between the top bar (`top`) and the dock (`bottom`), 24 px clear of
// each. The home app's view is placed `kInset` px inside the panel, so the
// panel's rounded corners stay visible.
//
// The user can widen or narrow the panel by dragging either side edge; it
// stays centred. A chosen width (`preferredWidth`, 0 = the default) is kept
// between the minimum (840, or the room if the window is smaller) and the
// room (the window less 24 px each side), so a width chosen on a big screen
// re-clamps on a small one and comes back when there is room again.
//
// The Activity sidebar (basecamp-experiments#44) is the same home app in its
// feed mode: a phone-wide column on the window's right edge, from under the
// top bar to the bottom, with a 1 px border on its left.
namespace HomeOverlayGeometry {

constexpr int kMinWidth = 840;
constexpr int kMaxWidth = 1000;   // the default's cap, not the user's
constexpr int kEdge = 24;
constexpr int kInset = 8;
constexpr int kSidebarWidth = 400;

// The widest the panel can be: the window less kEdge each side.
inline int maxWidth(const QSize& area)
{
    return std::max(0, area.width() - 2 * kEdge);
}

// The narrowest a drag can make it: kMinWidth, or the room when smaller.
inline int minWidth(const QSize& area)
{
    return std::min(kMinWidth, maxWidth(area));
}

// The width with no choice made: 70% of the window, 840..1000, in the room.
inline int defaultWidth(const QSize& area)
{
    return std::min(std::clamp(area.width() * 7 / 10, kMinWidth, kMaxWidth), maxWidth(area));
}

// The width used for a chosen one (0 or less = the default).
inline int panelWidth(const QSize& area, int preferredWidth = 0)
{
    if (preferredWidth <= 0)
        return defaultWidth(area);
    return std::clamp(preferredWidth, minWidth(area), maxWidth(area));
}

// Dragging the right (or left) edge to `x` (window coordinates): the panel
// stays centred, so its width is twice the edge's distance from the window's
// centre, in range. An edge dragged past the centre gives the minimum.
inline int widthForEdgeAt(const QSize& area, int x, bool rightEdge)
{
    const int twice = rightEdge ? 2 * x - area.width() : area.width() - 2 * x;
    return std::clamp(twice, minWidth(area), maxWidth(area));
}

// Double-clicking an edge: from anything narrower than the widest to the
// widest; from the widest back to the default (returned as 0, "no choice").
inline int toggledWidth(const QSize& area, int preferredWidth)
{
    return panelWidth(area, preferredWidth) < maxWidth(area) ? maxWidth(area) : 0;
}

inline QRect panel(const QSize& area, int top = 0, int bottom = 0, int preferredWidth = 0)
{
    const int w = panelWidth(area, preferredWidth);
    const int h = std::max(0, area.height() - top - bottom - 2 * kEdge);
    return QRect((area.width() - w) / 2, top + kEdge, w, h);
}

inline QRect content(const QSize& area, int top = 0, int bottom = 0, int preferredWidth = 0)
{
    return panel(area, top, bottom, preferredWidth).adjusted(kInset, kInset, -kInset, -kInset);
}

inline QRect sidebar(const QSize& area, int top = 0)
{
    const int w = std::clamp(kSidebarWidth, 0, std::max(0, area.width()));
    const int h = std::max(0, area.height() - top);
    return QRect(area.width() - w, top, w, h);
}

inline QRect sidebarContent(const QSize& area, int top = 0)
{
    const QRect s = sidebar(area, top);
    return s.width() > 1 ? s.adjusted(1, 0, 0, 0) : s;
}

} // namespace HomeOverlayGeometry
