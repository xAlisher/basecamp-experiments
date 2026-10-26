# Testing Basecamp shell test build 3

Build: [basecamp-shell-3](https://github.com/xAlisher/basecamp-experiments/releases/tag/basecamp-shell-3). Linux x86_64. A prototype, not an official Logos release.

New since [build 2](TESTING-SHELL-2.md): split view and a resizable Basecamp overlay in the desktop layout, faster search, and activity that keeps balances when an app floods. Build 2's Keychain, widgets and keyboard steps still apply.

## Run

1. Download `basecamp-shell-test-x86_64.AppImage` and `basecamp-shell-test-starter.tar.gz` into one folder.
2. `tar xzf basecamp-shell-test-starter.tar.gz`
3. `./basecamp-shell-test/run.sh`

It uses its own profile in `~/basecamp-shell-test`; your regular Basecamp is untouched. A profile from build 1 or 2 is kept and its demo apps are updated.

Sections 1 and 2 use the desktop layout: Settings > Customisation > Shell, choose **Basecamp Legacy**, then **Restart now**.

## 1. Split view

Contributed by Vaclav Pavlin ([basecampOS](https://github.com/vpavlin/basecampos)). Spec: [SPLIT-VIEW-SPEC](SPLIT-VIEW-SPEC.md), decision: [ADR-084](adr/0084-split-view-in-desktop-layout.md).

1. Open Stub Wallet and Stub Chat from the dock.
2. In Stub Wallet's title bar, tap the split button (left of minimise).
3. The other half shows **Split with…**. Pick Stub Chat.

Expected: the two windows share the screen side by side (top and bottom on a portrait screen).

| Do | Expected |
|---|---|
| Drag the divider | It snaps to 1/3, 1/2 or 2/3 |
| Double-click the divider | The halves swap sides |
| Maximise or drag a window's title bar | The split ends |
| Turn on tiling | The split ends |
| Split again with the picker open, then open an app from the dock | That app fills the empty half |
| Close Stub Chat, restart, split Stub Wallet again | **Usually with** lists Stub Chat first; picking it opens it at the size you left |

## 2. Resizable Basecamp overlay

1. Press Ctrl+K (or the λ button) to open the overlay.
2. Drag its left or right edge outwards.

Expected: the panel stays centred and grows; the widget grid gains a column about every 400 px (2 columns at the default width, 4 at about 1600 px on a 1920 screen).

| Do | Expected |
|---|---|
| Double-click an edge | The panel goes to the widest; double-click again for the default |
| Restart Basecamp | The width you chose is kept |
| Make the window smaller | The panel shrinks to fit, and comes back when the window grows |

## 3. Faster search

Spec: [SHELL-OBJECTS-SPEC §14](SHELL-OBJECTS-SPEC.md), decision: [ADR-082](adr/0082-search-role-keeps-a-copy.md).

1. Open each Stub app once, and in each tap **Flood 1000**.
2. On home, type `a`, then `ada`, then clear the field.

Expected: results follow each keystroke without a visible pause. The feed fills in a few frames.

## 4. Balances survive a flood

Spec: [SHELL-OBJECTS-SPEC §15](SHELL-OBJECTS-SPEC.md), decision: [ADR-083](adr/0083-current-state-kept-over-the-cap.md).

1. In Stub Wallet, tap **Flood past the cap**.

Expected: the stub reports "keeps 500 of 500, 9 of them current state". On home, the Wallet widget still shows every account and asset with its balance, and Transactions shows the newest events.

## Known

- The window behind a split shows through the gap between the halves.
- In the widgets view, a broad search is still slower than in the feed ([#64](https://github.com/xAlisher/basecamp-experiments/issues/64)).
- After a flood the Communities widget can show no communities ([#66](https://github.com/xAlisher/basecamp-experiments/issues/66)).

## Not yet checked by a person

Dragging the split divider with a real mouse, and both the divider and the overlay edges with a finger. Reports welcome: https://github.com/xAlisher/basecamp-experiments/issues
