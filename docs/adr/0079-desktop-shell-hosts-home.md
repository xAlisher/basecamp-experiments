# ADR-079: The desktop layout hosts the home app as an overlay and an Activity sidebar

**Status:** Accepted (experiment) · 2026-10-25 · Extends ADR-070 (home was Basecampium only). Issues #42, #43, #44.

## Context
Basecamp Legacy's desktop layout had its own Spotlight page for Ctrl+K, hardcoded dock entries for Applications, Packages and Settings, and showed the search role app as a window. The home app now does search, apps and activity better, and system apps are ordinary apps.

## Decision
- **The home role's app is hosted by the desktop layout,** outside any window, reached only through its role-gated `shell` object. One instance serves two surfaces, never both at once:
  - **Overlay:** Ctrl+K, the bar's Search button and the dock's Basecamp icon toggle it. It is centred (840 to 1000 px wide by default; the user can drag its side edges wider, see BASECAMP-OS-SPEC 2.1) and ends a gap above the dock. Esc or a click outside closes it. It replaces Spotlight.
  - **Activity sidebar:** a button next to the clock opens home in a feed-only mode in a 400 px column on the right edge: search, pills and cards.
- **Opening an app** from either surface (Recent apps, an Apps row, a card's open action, or the dock) opens or raises its window and closes the surface.
- **The dock:** Basecamp (λ, a smaller logo) pinned first, then open apps only. No hardcoded links. Apps that only fill a background role (search) never appear in the dock.
- **The dock stays above** the overlay's dim and keeps working while either surface is open.

## Consequences
+ The same home, search and activity in both shells; the desktop loses a parallel search page.
+ Applications, Packages and Settings are found the same way as every other app.
- Ctrl+K and Esc are verified by hand only (the headless inspector can't send them); tracked as wetware on #43.
- The top bar grew from 30 to 36 px to fit a 32 px Activity button.
- Loading the search app directly still opens a visible window with no dock entry.

## Evidence
`src/MainContainer.cpp` (`positionHomeOverlay`, `launchFromDock`, dock rules), `src/HomeOverlayGeometry.h`, `tests/home_overlay_geometry_test.cpp`, `src/Basecamp/Shell/HomeOverlay.qml`, `src/DesktopArea.cpp`. Commits f3cae96, 9aec913, b6a10b4, 0122957, 2e4942f. Screens: [desktop-overlay-1600](../basecamp-shell/desktop-overlay-1600.png), [desktop-activity-1600](../basecamp-shell/desktop-activity-1600.png).
