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
- **Two ways to ship it:** as a built-in **layout** of the default shell (this prototype, the easy
  upstream path), or later as a **shell package**: a `ui_shell` plugin, signed by its builder but not
  verified by Logos, that replaces Basecamp's UI shell, so the community can build their own (§7).

### Terms

| Term | What it changes | Code? | Ships as |
|---|---|---|---|
| **Theme** | How Basecamp looks: colours, gradients, wallpaper, icons, control styles | No, data | `.theme.json` / bundle |
| **Layout** | How the default shell arranges things: `sidebar`, `desktop`, later `mobile` | Yes | Built into Basecamp |
| **Shell** | The whole UI around the apps, replaceable | Yes | Shell package (`ui_shell`) |

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

### 2.1 Basecamp overlay (replaced Spotlight, ADR-079, #43)

- **Ctrl+K**, the status bar's Search and the dock's Basecamp icon toggle the home app in a panel
  over a dimmed desktop, centred, from under the status bar to a gap above the dock. **Esc** or a
  click outside closes it. On the desktop the shell owns Ctrl+K.
- **Width:** by default 70 % of the window, kept between 840 and 1000 px. The user can drag either
  side edge to resize it; it stays centred, between 840 px (or the window, if smaller) and the
  window less 24 px each side. The edges show a horizontal resize cursor and a grip on hover; the
  hit area is 16 px. **Double-click** an edge toggles the default and the widest.
- The chosen width is remembered across restarts (setting `BasecampShell/homeOverlayWidth`, px) and
  clamped to the window on use: a smaller window narrows the panel, a bigger one brings the chosen
  width back. Toggling back to the default forgets it.
- Home's view follows the panel live, so its widget grid gains a column per 400 px as it widens
  (HOME-GRID-SPEC). The Activity sidebar stays 400 px and is not resizable.

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

- **Layout:** Window (default) / Desktop, with "Takes effect after a restart".
- **Wallpaper** (desktop layout): thumbnails for the built-ins (**Basecamp** tents, **Plain**) and
  **Choose image…** (PNG or JPEG, up to 32 MB and 8192 px; copied into Basecamp's data folder).
  Applies immediately. *Later:* the theme's `backgrounds.workspace` as a choice.

## 7. Shell packages (future)

Basecamp's UI is already a plugin: `plugins/main_ui/main_ui.so`, loaded by the app through
`IShellView` and given a narrow `IShellHost` (about eight operations: sections, load / unload a UI
module, display names, the observer). A build check (`nix/symbol-gate.nix`) keeps the shell from
linking the Logos runtime, so it never touches core modules, keys or the network directly. A **shell
package** is a package that provides another `main_ui`.

### 7.1 What it can do

Replace everything the default shell draws: navigation, how apps are presented (tabs, windows,
full-screen cards), search, the welcome surface, the status area. Apps, core modules and themes
are unchanged; a shell hosts the same app widgets and uses the same design system, so themes apply.

### 7.2 What Basecamp needs first

1. **Shell choice:** Settings → Appearance → **Shell** (Default, or an installed one); loaded from the
   user's plugins folder. Today the built-in shell always wins (embedded first, then the user folder).
2. **Package type** `ui_shell` in the manifest, installable from the Package Manager, with
   `hostAbiVersion` declared; a mismatch falls back to the default shell instead of stopping the app
   (today it is fatal).
3. **Signed, not verified by Logos:** builders distribute a shell like an app, as a signed LGX
   module. The signature says who built it and that the package wasn't altered; it does not mean
   Logos reviewed it, just as Logos doesn't review third-party apps. Users decide which builders to
   trust and are responsible for the shells they install. A shell sees every app's window and every
   keystroke, so Basecamp shows the signer and a clear warning before a shell is installed or turned
   on, and refuses an unsigned or altered shell package. Basecamp doesn't check signatures yet;
   shells would be the first place it must.
4. **Safe mode:** if a shell fails to load or crashes at start, the next start uses the default shell;
   a key held at launch forces it.
5. **Shared pieces as a library:** the parts any shell needs (app hosting, intents, overlay dialogs,
   the shortcut bridge) exposed so a shell doesn't copy the default one's internals.

### 7.3 Path

Ship **Basecamp OS as a layout** first (no new trust model, same mechanism the mobile layout needs).
Open the shell package type once Basecamp checks signatures; Basecamp OS can then also be published as a
shell package to prove it.

## 8. Not in the prototype

Real OS windows · multiple monitors · snapping and tiling · virtual desktops · remembering window
positions · desktop icons or search · a theme-suggested layout · mobile (that is the third layout,
`mobile`, from the mobile-readiness audit).

## 9. Open questions

1. **Ownership:** shell navigation belongs to the Basecamp team; is a second layout welcome upstream,
   or does it stay an experiment?
2. **Focus:** with several apps visible, keyboard focus follows the active window. Shortcuts that
   assumed one visible app need checking. Apps that run in their own process receive their own keys,
   so Ctrl+K can't open Spotlight while one of them has focus (the status bar hint still works).
3. **Wallpaper image:** comes from the node onboarding work (`node_welcome_BG.png`); confirm the
   source and licence before anything ships.
4. **Windowed vs full screen:** does the desktop layout make sense in a window, or only full screen?
5. **Shell packages:** worth opening to the community, and when? Depends on Basecamp checking signatures
   and on the Basecamp team owning the `IShellView` / `IShellHost` contract as a public API.
