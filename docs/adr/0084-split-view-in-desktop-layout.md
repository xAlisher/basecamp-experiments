# ADR-084: Split view in the Desktop layout

**Status:** Accepted (experiment) · 2026-10-26 · Contributed by Vaclav Pavlin ([vpavlin/basecampos](https://github.com/vpavlin/basecampos), branch `basecamp-shell`). Applies to the Desktop layout (Basecamp Legacy); Basecampium is unchanged.

## Context
The Desktop layout has floating windows, maximise and tiling. Tiling puts every visible window in a column, so two apps side by side means minimising the rest first, and nothing remembers which apps a person uses together. On a tablet the 14 px title-bar circles were also hard to hit. Vaclav Pavlin built a split view on top of this repository's Desktop shell and published it as three commits; this ADR records porting it into the Basecamp source.

## Decision
- **A Split button on every window** (fourth, left of minimise) puts that window in the first half: left, or top when the free area is taller than wide. The halves use the maximised-window area, so the status bar and the dock stay clear.
- **The other half is a picker:** remembered partners first ("Usually with", launched if closed, the split completing when the window arrives), then the open windows. With exactly one other window and no history it joins at once. An app opened from the dock or a window restored while the picker waits takes the empty half; an app opened while split replaces the side in use.
- **A divider** resizes between 0.2 and 0.8 with snaps at 1/3, 1/2 and 2/3; double-click swaps the sides.
- **A split is one state among the others, not a mode on top of them.** Maximising, dragging, minimising or closing a side ends it (the remaining window is maximised); tiling and split exclude each other.
- **Pairs are remembered** in `<AppConfigLocation>/split-pairs.json`: per unordered pair the ratio, a use count and the last use. Rank is the count halving every week; the 30 best are kept.
- **Finger-sized hit areas:** the divider grabs 12 px over both windows' edges, the title-bar buttons take the full bar height.
- **Ported as contributed**, authorship kept (three commits), with two changes in a separate commit: the picker draws apps with the shell's app tile, and the snap and rank are factored out so they can be unit-tested.

## Consequences
+ Two apps side by side in one click, and the usual pair comes back first without searching.
+ Works in portrait and survives a rotation (the split is laid out again on every resize).
+ No new API for apps and nothing in Basecampium; the whole feature is in `DesktopArea` plus one connection in `MainContainer`.
- The pairs file lives outside the shell's settings and stores absolute icon paths; a moved profile loses the icons until a pair is used again.
- The windows behind a split stay where they were and show through the gap between the halves.
- Dragging and double-clicking the divider are tested with synthetic mouse events only; real touch needs a person at a touchscreen.

## Evidence
`src/DesktopArea.{h,cpp}` (`beginSplit`, `setSplit`, `layoutSplit`, `exitSplit`, `replaceActiveSide`, `recordSplit`, `partnersOf`, `snapSplitRatio`, `SplitDivider`, `SplitPicker`), `src/MainContainer.cpp` (`launchRequested`). Test `desktop_split_test` (18 cases). Basecamp source branch `keychain`, commits 4537408, b26a879, ca3aa83 (contributed), 13aa1e5 and 70c8b8d (port). Spec and headless results: [SPLIT-VIEW-SPEC](../SPLIT-VIEW-SPEC.md).
