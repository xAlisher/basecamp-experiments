# Basecamp OS: a desktop layout for Basecamp

**Status:** Prototype implemented · 2026-10-16 · branch `basecamp-os-theme` (on top of `theming-v2`)
**Audience:** Basecamp shell developers, design review

## TL;DR

- **A second shell layout, not a theme.** Basecamp gets a `desktop` layout next to today's `sidebar`
  layout: full screen, a wallpaper, a dock at the bottom, and every app in its own movable window.
- **Applications, Package Manager and Settings become apps:** each opens in a window from the dock,
  like any installed app.
- **Default Basecamp style:** all chrome (dock, title bars, buttons) is drawn from palette tokens, so
  Dark is the reference look and any theme still applies.
- **Wallpaper:** the Basecamp tents photo from the node onboarding welcome page by default; changeable
  in Settings → Appearance (built-ins or your own image).
- **Themes stay data-only.** A layout is code shipped with Basecamp; a theme only colours it.

## 1. Layouts

| Layout | What | Default |
|---|---|---|
| `sidebar` | Today: sidebar, workspace tabs, sections in the content area | yes |
| `desktop` | Full screen, wallpaper, dock, apps in windows | no |

- Chosen in **Settings → Appearance → Layout**. Takes effect on the next start (the shell is built once
  per run). `BASECAMP_LAYOUT=desktop` overrides it for testing.
- Later: a theme may *suggest* a layout (`"layout": "desktop"`); the user's choice wins, as with
  density. Not in the prototype.

## 2. Desktop

```
┌───────────────────────────────────────────────────────────────┐
│                        wallpaper                              │
│   ┌─ Settings ─────────────── – □ × ┐                         │
│   │                                  │   ┌─ Wallet ───── – □ × ┐│
│   │                                  │   │                     ││
│   └──────────────────────────────────┘   └─────────────────────┘│
│                                                               │
│        ┌──────────────────────────────────────────────┐       │
│        │ [▦] [⬡] [⚙] │ [app] [app] [app] [app]        │ dock  │
│        └──────── •  ────────────•─────•───────────────┘       │
└───────────────────────────────────────────────────────────────┘
```

- **Full screen** when the layout is `desktop`. F11 toggles full screen.
- **Wallpaper** runs behind the whole desktop, status bar included. **Fit** (default) shows the whole
  image and fills the spare room with a blurred, dimmed copy; **Fill** covers and crops; **Stretch**
  stretches. For a perfect fit, match the screen (e.g. 5120 × 1440 on a 32:9 display).
- **Frosted glass:** the status bar, title bars, window frames and the shell's own windows draw a
  blurred copy of the wallpaper under a translucent tint. Apps keep their own backgrounds.
- **Tiling:** a status bar toggle lays the visible windows out as equal-width columns with 10 px gaps,
  and re-lays them out as windows open, close or hide.
- **Status bar** (30 px, top, `backgroundSecondary`): the λ and **Logos Basecamp** on the left; a
  **Search · Ctrl K** hint (opens Spotlight) and the clock on the right.

### 2.1 Spotlight

- **Ctrl+K** (or the status bar hint) opens Spotlight over a dimmed desktop: the workspace welcome
  page's search in a floating panel, search field focused, **Recently Closed** below.
- Typing shows the same results and filters as the workspace search. Opening a result opens or raises
  its window. **Esc** or a click outside closes it.
- On the desktop the shell owns Ctrl+K: the hidden views' own Ctrl+K shortcuts are disabled or lose
  to it.

## 3. Dock

- Floating panel, bottom centre, 12 px above the screen edge. `backgroundSecondary` at 90 %,
  `borderHairline` border, `radiusXlarge` corners.
- **Groups**, split by a thin separator:
  1. System apps: **Applications**, **Package Manager**, **Settings**.
  2. Installed apps (the sidebar's launcher list), in the same order as the sidebar.
- **Tiles:** 48 px icons on a 56 px pitch; full-bleed icons fill the tile, others sit on a `surface`
  tile. Hover lifts the tile and shows the name above it; the theme's hover glow applies.
- **Running indicator:** a small dot under every app with an open window (accent colour; the
  front-most window's dot is wider).
- **Click:** not running → launch / open; running → restore and raise its window.

## 4. Windows

- Each app (and each system app) is a window inside the desktop.
- **Title bar** (36 px, `backgroundSecondary`): app icon and name on the left; **minimise**,
  **maximise** and **close** circle buttons on the right (`surface`, hover `accentOrange`; close hovers
  `error`).
- **Move:** drag the title bar. **Resize:** from any edge or corner (a 4 px frame). **Maximise:**
  double-click the title bar or the button; fills the desktop between the status bar and the dock.
- **Minimise** hides the window; the dock restores it.
- **Close:** an app window unloads its module (same as closing a tab today); a system app window just
  closes.
- **New windows** open at about 70 % of the desktop (at most 1280 × 860), centred, each next one
  offset by 32 px.
- Active window: `borderStrong` outline; others `border`.

## 5. System apps

| App | Content |
|---|---|
| Applications | The App Manager view, pinned in its own window |
| Package Manager | `package_manager_ui` (loaded on first open, as today) |
| Settings | The Settings view, pinned in its own window |

Existing navigation keeps working: anything that switches the shell section today (intents such as
`basecamp.settings.open`, install flows, "Show all results") opens or raises the matching window.

## 6. Settings → Appearance

- **Layout:** Sidebar / Desktop, with "Takes effect after a restart".
- **Wallpaper** (desktop layout): thumbnails for the built-ins (**Basecamp** tents, **Plain**) and
  **Choose image…** (PNG or JPEG, up to 32 MB and 8192 px; copied into Basecamp's data folder).
  Applies immediately. *Later:* the theme's `backgrounds.workspace` as a choice.

## 7. Not in the prototype

Real OS windows · multiple monitors · snapping and tiling · virtual desktops · remembering window
positions · desktop icons or search · a theme-suggested layout · mobile (that is the third layout,
`mobile`, from the mobile-readiness audit).

## 8. Open questions

1. **Ownership:** shell navigation belongs to the Basecamp team; is a second layout welcome upstream,
   or does it stay an experiment?
2. **Focus:** with several apps visible, keyboard focus follows the active window. Shortcuts that
   assumed one visible app need checking.
3. **Wallpaper image:** comes from the node onboarding work (`node_welcome_BG.png`); confirm the
   source and licence before anything ships.
4. **Windowed vs full screen:** does the desktop layout make sense in a window, or only full screen?
