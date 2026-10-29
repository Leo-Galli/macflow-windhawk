# Changelog

All notable changes to MacFlow are documented in this file.

## [Unreleased]

### Fixed

- `macflow-dock`, `macflow-notifications`, `macflow-start`: added missing
  `-lcomctl32` to `@compilerOptions`. Without it, the real Windhawk engine's
  link step failed with `undefined symbol: DefSubclassProc` /
  `SetWindowSubclass` / `RemoveWindowSubclass` (found via an actual in-app
  compile attempt). `macflow-explorer` was unaffected, it already linked
  comctl32.
- `macflow-dock`: fixed incorrect multi-monitor behavior. Taskbar-edge
  detection relied on `SHAppBarMessage(ABM_GETTASKBARPOS)`, which only
  reports the primary monitor's taskbar; replaced with
  `MonitorFromWindow`/`GetMonitorInfo` against each window's own monitor.
  Also fixed a compounding margin bug where the inset transform could reapply
  on top of itself during pure z-order/activation window messages, causing
  the dock to visually collapse on one monitor (found via real hands-on
  testing on a dual-monitor machine).
- `macflow-notifications`, `macflow-start`, `macflow-explorer`: fixed these
  mods having no visible effect when their target window already existed
  before the mod was enabled (the common case) - added an `EnumWindows`
  initial scan at startup, matching the approach `macflow-dock` already used.
- `macflow-explorer`: fixed a real deadlock (`WM_WINDOWPOSCHANGED` locking a
  mutex, then calling into a function that locked the same non-recursive
  mutex again) on the first layout of any Explorer window, found while
  fixing the issue above. Split into two separate mutexes.

Initial implementation of the MacFlow suite. Not yet tagged as a release:
hands-on hardware testing (see `docs/TESTING.md`) is still pending before a
v1.0.0 release is cut.

### Added

- `macflow-dock` (1.0.0): floating, translucent, rounded taskbar dock with
  configurable margin, optional auto-hide, and an experimental hover
  highlight.
- `macflow-notifications` (1.0.0): rounded, translucent, accent-bordered
  Notification Center and Quick Settings window chrome.
- `macflow-start` (1.0.0): rounded, translucent, accent-bordered Start menu
  window chrome.
- `macflow-explorer` (1.0.0): rounded corners, Mica/Acrylic backdrop, and an
  accent-colored rounded selection outline for File Explorer windows.
- `docs/RESEARCH.md`, `docs/DESIGN.md`, `docs/TESTING.md`: research,
  architecture decisions, and the current (mostly pending) testing matrix.

### Not included (evaluated and dropped, with reasoning in `docs/DESIGN.md`)

- MacFlow Menu Bar - no safe, non-duplicate implementation surface exists.
- Running-app indicator dots and a pinned/running separator in
  `macflow-dock` - no public API exposes a reliable running-vs-pinned signal.
