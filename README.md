# MacFlow

A small suite of [Windhawk](https://windhawk.net/) mods that give Windows 11
a clean, macOS-inspired look and feel - rounded corners, Mica/Acrylic
translucency, accent-colored borders, a floating dock - **without changing
any icons**.

> **Status: pre-release.** All four mods compile cleanly against the real
> Windhawk toolchain (see `docs/TESTING.md`), but hands-on hardware testing
> (visual verification, screenshots, stability passes, idle CPU/memory
> checks) has not been completed yet. No screenshot exists in this
> repository that was not captured from a real, running desktop - see
> `docs/screenshots/README.md`.

## What's in the suite

| Mod | What it does |
|---|---|
| `macflow-dock` | Turns the taskbar into a floating, rounded, translucent dock with a configurable margin, optional auto-hide, and an experimental hover highlight. |
| `macflow-notifications` | Rounds and tints the Notification Center and Quick Settings flyouts with an accent-colored border. |
| `macflow-start` | Rounds and tints the Start menu window with an accent-colored border. |
| `macflow-explorer` | Rounds File Explorer window corners, applies a Mica/Acrylic backdrop, and adds an accent-colored rounded selection outline. |

A fifth component described in the original design brief, **MacFlow Menu
Bar**, was evaluated and intentionally not built - see
[`docs/DESIGN.md`](docs/DESIGN.md) for why.

## Why these mods look the way they do

MacFlow is built entirely on **public, documented Windows APIs**: DWM window
attributes (rounded corners, Mica/Acrylic backdrop, accent border), UI
Automation, and standard Win32 window management. It does **not** reimplement
the private XAML-diagnostics engine that
[Windows 11 Taskbar Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp),
[Windows 11 Start Menu Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-start-menu-styler.wh.cpp),
[Windows 11 Notification Center Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp),
and
[Windows 11 File Explorer Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-file-explorer-styler.wh.cpp)
use for deep per-element XAML restyling. That engine is GPLv3-licensed,
shared by at most one consumer per process at a time, and tightly coupled to
the exact internal layout of each Windows build - reimplementing it would
mean license contamination, real crash risk from untested COM/WinRT code in
`explorer.exe`, and direct conflict with those mods if a user runs both.

MacFlow is designed to **complement, not duplicate** those mods. Where a
visual goal genuinely needs that private engine (e.g. per-tile Start menu
spacing, per-card Notification Center layout), MacFlow documents pairing
with the relevant mod's own settings instead of shipping a competing
implementation. Full reasoning, with sources, is in
[`docs/RESEARCH.md`](docs/RESEARCH.md) and
[`docs/DESIGN.md`](docs/DESIGN.md).

## Install

Once reviewed and merged upstream, each mod will be installable by searching
"MacFlow" inside the Windhawk app, or via its individual mod page (links will
be added here once published - see Known limits below for current status).

Until then, install from source:

1. Install [Windhawk](https://windhawk.net/) if you haven't already.
2. Open the Windhawk app, choose "+ New mod" (or "Edit mod" for the raw
   editor), and paste the contents of the `.wh.cpp` file you want from
   `mods/` in this repository.
3. Click "Compile", then enable the mod.

## Settings reference

### macflow-dock

| Setting | Default | Description |
|---|---|---|
| `dockMarginPx` | 8 | Margin between the floating dock and the screen edges (bottom-docked taskbar only). |
| `dockAutoHide` | false | Auto-hide the dock, using the same mechanism as Windows' own taskbar auto-hide. |
| `dockCornerStyle` | round | `round` / `roundsmall` / `square`. |
| `dockBackdrop` | mica | `mica` / `acrylic` / `none`. |
| `dockMagnifyEnabled` | false | Experimental hover highlight behind the icon under the cursor. Off by default for performance. |
| `dockMagnifyScale` | 130 | Hover highlight size, percent of the icon's bounds. |

### macflow-notifications / macflow-start

| Setting | Default | Description |
|---|---|---|
| `notif`/`start` `CornerStyle` | round | `round` / `roundsmall` / `square`. |
| `notif`/`start` `Backdrop` | acrylic (notifications) / mica (start) | `mica` / `acrylic` / `none`. |
| `notif`/`start` `AccentBorder` | true | Outline the flyout with the current Windows accent color. |

### macflow-explorer

| Setting | Default | Description |
|---|---|---|
| `explorerCornerStyle` | round | `round` / `roundsmall` / `square`. |
| `explorerBackdrop` | mica | `mica` / `acrylic` / `none`. |
| `explorerSidebarTranslucent` | true | Opts the navigation pane into Explorer-style control theming. Does not produce true translucency - see the mod's readme. |
| `explorerRoundedSelection` | true | Accent-colored rounded outline on top of the native selection highlight. |

All settings reload live via each mod's `Wh_ModSettingsChanged` handler - no
restart needed.

## Compatibility

- Windows 11, 23H2 and 24H2 (per the reference mods' own stated support -
  see `docs/RESEARCH.md`). Not tested on Windows 10 or Insider builds.
- Light and dark mode: implemented (theme-aware DWM attributes), not yet
  hands-on verified - see `docs/TESTING.md`.
- DPI scaling (100%-200%) and multi-monitor: implemented with DPI-aware
  margin/size calculations, not yet hands-on verified.
- x86-64 only, matching every reference mod researched for this project.

## Known limits

- **No reliable public signal for taskbar "running" state.** MacFlow Dock
  does not draw running-app indicator dots or a pinned/running separator
  (both described in the original design brief) because no public API
  exposes whether a taskbar button represents a running app vs. a pinned-only
  one - the real signal is a private WinRT property only reachable by
  hooking internal taskbar symbols, which this project's architecture
  intentionally avoids. Full research in `docs/DESIGN.md`.
- **No true sidebar translucency in File Explorer.** The classic
  `SysTreeView32` navigation pane does not reliably support see-through Mica
  without taking over Explorer's non-client frame handling, which this mod
  does not do. Pair with Windows 11 File Explorer Styler for that effect.
- **DWM corner presets, not an arbitrary 12px radius**, on window chrome
  (Notifications, Start, Explorer window frame). DWM only exposes
  round/round-small/square presets. MacFlow-drawn pixel content (the
  Explorer selection outline) does achieve a literal 12px radius.
- **Hover magnify is a highlight overlay, not real icon scaling.** True
  per-icon scaling would require the private XAML engine.
- **Notification Center vs. toast popups on Windows 11 23H2** share a
  generic window class with no public way to tell them apart; a height
  heuristic is used and documented in `docs/DESIGN.md`.
- No MacFlow Menu Bar - evaluated and dropped, see `docs/DESIGN.md`.

## Troubleshooting / how to fully revert

- Disable a mod from the Windhawk app at any time - each mod's
  `Wh_ModUninit` restores the DWM window attributes it changed and removes
  its window subclasses.
- If something looks wrong after a Windows update, check
  [this repository's issues](https://github.com/Leo-Galli/macflow-windhawk/issues)
  for a known compatibility break, and file a bug report with your Windows
  build number (see the issue template).
- To fully remove MacFlow, disable and delete each mod from the Windhawk app.
  No files are installed outside Windhawk's own mod storage.

## Credits

MacFlow was researched and designed around, and is meant to complement, the
mods built by **m417z** (https://github.com/m417z), all in the
[`ramensoftware/windhawk-mods`](https://github.com/ramensoftware/windhawk-mods/tree/main/mods)
repository:
[Windows 11 Taskbar Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-taskbar-styler.wh.cpp),
[Taskbar height and icon size](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-icon-size.wh.cpp),
[Windows 11 Start Menu Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-start-menu-styler.wh.cpp),
[Windows 11 Notification Center Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-notification-center-styler.wh.cpp),
[Taskbar Clock Customization](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/taskbar-clock-customization.wh.cpp),
[Better file sizes in Explorer details](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/explorer-details-better-file-sizes.wh.cpp),
and
[Windows 11 File Explorer Styler](https://github.com/ramensoftware/windhawk-mods/blob/main/mods/windows-11-file-explorer-styler.wh.cpp).
Those mods are GPLv3-licensed; no code from them is used in MacFlow (see
`docs/RESEARCH.md` Section 5 for the licensing analysis).

## License

MIT - see [`LICENSE`](LICENSE).

## Windhawk submission status

Not yet submitted. Submission PRs to
[`ramensoftware/windhawk-mods`](https://github.com/ramensoftware/windhawk-mods)
will be opened, and linked here, once Phase D hands-on testing (including
real screenshots) is complete - see `docs/TESTING.md` and `PROGRESS.md`.
