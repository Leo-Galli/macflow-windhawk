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
