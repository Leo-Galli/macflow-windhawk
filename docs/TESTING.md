# MacFlow — Phase D Testing

Testing date started: 2026-10-29.

## What could actually be verified by the assistant, and how

The assistant building this suite has no screen-capture, GUI-automation, or
input-control capability (no way to see a rendered window, click a button in
the Windhawk UI, or take a screenshot). The following was verified for real,
using the real Windhawk compiler toolchain already installed at
`C:\Program Files\Windhawk\Compiler` (mingw-w64 clang 20, the same toolchain
version the Windhawk engine itself uses per `RESEARCH.md` Section 1):

| Check | Method | Result |
|---|---|---|
| Compiles with zero errors | `clang++ -x c++ -std=c++23 -target x86_64-w64-mingw32` with the same defines as the repo's own `compile_flags.txt`, plus `-DWH_MOD_ID` (supplied automatically by the real engine at actual build time; required locally because `windhawk_utils.h`'s `SetWindowSubclassFromAnyThread` macro-expands it) | PASS, all 4 mods |
| Compiles with zero warnings | Same, with `-Wall -Wextra` added | PASS, all 4 mods |
| Every referenced API exists | Verified against the real headers shipped in `C:\Program Files\Windhawk\Compiler\include` (`windhawk_api.h`, `windhawk_utils.h`, `dwmapi.h`, `uxtheme.h`, `commctrl.h`, `uiautomationclient.h`, `shellapi.h`, `wingdi.h`, `winuser.h`) before being used in code — see `RESEARCH.md` and the design decisions in `DESIGN.md` | PASS |
| Full link against the Windhawk engine's `Wh_*` exports | Not performed — the engine's real link step was not reproduced locally (the exact import library / symbol-resolution mechanism the engine uses was not available to invoke outside the app itself) | **NOT VERIFIED** — this is checked when the mod is loaded via the real Windhawk app, see below |

## What requires a human at the keyboard (not yet done)

Everything below requires installing each mod through the real Windhawk
application's "Edit mod" / "Compile mod" flow, enabling it, and visually
observing the running Windows 11 desktop. This has **not been done yet** and
no result below is invented. Windhawk is confirmed installed and already has
four third-party mods present
(`C:\ProgramData\Windhawk\ModsSource\windows-11-taskbar-styler.wh.cpp`,
`windows-11-start-menu-styler.wh.cpp`,
`windows-11-notification-center-styler.wh.cpp`,
`windows-11-file-explorer-styler.wh.cpp`), so a real install/enable/disable
cycle is possible on this machine once a person drives the GUI.

### Compile-in-app matrix

| Mod | Compiles in Windhawk's own "Compile mod" step | Notes |
|---|---|---|
| macflow-dock | NOT TESTED | |
| macflow-notifications | NOT TESTED | |
| macflow-start | NOT TESTED | |
| macflow-explorer | NOT TESTED | |

### Build / theme / DPI / monitor matrix

| Condition | macflow-dock | macflow-notifications | macflow-start | macflow-explorer |
|---|---|---|---|---|
| Windows 11 23H2 | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| Windows 11 24H2 | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| Light theme | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| Dark theme | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| 100% DPI | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| 125% DPI | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| 150% DPI | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| 200% DPI | NOT TESTED | NOT TESTED | NOT TESTED | NOT TESTED |
| Multiple monitors | NOT TESTED | N/A (single flyout) | N/A (single flyout) | NOT TESTED |
| Taskbar on non-primary monitor | NOT TESTED | N/A | N/A | N/A |

### Stability pass (enable/disable/settings-change x20)

| Mod | Result |
|---|---|
| macflow-dock | NOT TESTED |
| macflow-notifications | NOT TESTED |
| macflow-start | NOT TESTED |
| macflow-explorer | NOT TESTED |

### Idle CPU / memory (30 minutes, Task Manager / Process Explorer)

NOT TESTED.

### Icon-unchanged verification

NOT TESTED (requires real before/after screenshots - see `docs/screenshots/`,
currently empty).

## How to run this testing (for whoever has hands on the keyboard)

1. Open the Windhawk app (already installed).
2. For each file in `mods/`: New mod -> paste the source (or point Windhawk
   at the file) -> Compile. Record any compiler error/warning the app's own
   compiler reports (it should match "zero warnings" above, since the same
   toolchain is used, but the app's exact invocation may differ and should
   be the authority).
3. Enable the mod, observe the live effect against the matrix above, toggle
   dark/light mode (Settings > Personalization > Colors), and change DPI
   scaling (Settings > Display) to fill in each cell honestly.
4. Toggle enable/disable and change a setting, 20 times in a row, for each
   mod; note any crash, hang, or visual artifact that doesn't clear.
5. Leave each mod enabled for 30 minutes at idle, compare Task Manager's
   `explorer.exe` CPU/memory before enabling vs. after the 30 minutes.
6. Take real screenshots (1920x1080, light and dark) before/after each mod is
   enabled, save them into `docs/screenshots/`, and compare the icon regions
   pixel-for-pixel to confirm no icon changed.
7. Update the tables above with the real results, replacing "NOT TESTED".
