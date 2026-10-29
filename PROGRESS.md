# MacFlow Windhawk Suite — Progress Tracker

Working directory: `C:\Users\leoga\Desktop\PROGETTI\macflow-windhawk`

Resume rule: if this file exists, resume from the first unchecked item below.
Remove the `THE END` marker at the bottom while work is in progress; only append it back
once every item in every phase is checked and verified for real (no fabricated results).

## Known hard constraints (established 2026-10-29)

- No screenshot / screen-capture / GUI-automation tool is available to the assistant.
  Real before/after screenshots and visual verification (icons unchanged, animation feel,
  Task Manager idle monitoring) require the user's direct, hands-on participation.
- Windhawk IS installed locally (`C:\Program Files\Windhawk`), including a real mingw-w64
  compiler toolchain under `Compiler\bin`, so compilation and warning checks CAN be done
  for real by the assistant.
- `gh` CLI is authenticated as `Leo-Galli` with repo/workflow scopes, so real GitHub repo
  creation, pushes, and PR submission are technically possible once content is ready.
- Submission PRs to `ramensoftware/windhawk-mods` must only be opened once Phase D testing
  (including real screenshots) is genuinely complete — never claim tested status without
  real evidence.

## Phase A — Research
- [x] RESEARCH.md written: Windhawk mod format, API surface, contribution rules, PR template
- [x] Documented hooks/targets/limits for: Taskbar Styler, Taskbar height/icon size,
      Start Menu Styler, Notification Center Styler, Taskbar Clock Customization,
      Better file sizes in Explorer
- [x] Licenses of the above mods identified and compatibility/attribution requirements noted
      (all six are GPLv3 by m417z — MacFlow must not copy their code)
- [x] Confirmed no invented APIs — every function referenced exists in docs/headers/real mod source

## Phase B — Design
- [x] DESIGN.md written: visual system (Mica/acrylic, DWM rounded corners, shadows,
      Segoe UI Variable, accent color, 150-200ms composition easing)
- [x] Hard rule documented: no icon changes anywhere
- [x] Component designs written for: MacFlow Dock, MacFlow Menu Bar (DROPPED + documented why),
      MacFlow Notifications, MacFlow Start, MacFlow Explorer
- [x] Each component: hooks/APIs, settings, fallback behavior documented
- [x] Architecture decision: original public-API-only implementations (DWM attributes, UI
      Automation, Win32 subclassing) instead of reimplementing the GPLv3 XAML-diagnostics
      engine; deep per-element restyling documented as theme presets for the existing
      reference mods instead of duplicating them (user-confirmed 2026-10-29)

## Phase C — Implementation
- [x] macflow-dock.wh.cpp (running-dots/separator dropped, see DESIGN.md — no
      public API signal exists for taskbar button running state)
- [x] macflow-menubar.wh.cpp — DROPPED, documented in DESIGN.md
- [x] macflow-notifications.wh.cpp
- [x] macflow-start.wh.cpp
- [x] macflow-explorer.wh.cpp
- [x] All mods: full metadata blocks, macflow- id prefix, settings with live reload
- [x] All mods: guarded hooks, full Wh_ModUninit cleanup, no polling loops
- [x] All 4 mods compile cleanly (0 warnings, 0 errors) with the real Windhawk
      mingw/clang-20 toolchain at `C:\Program Files\Windhawk\Compiler`, using
      `-Wall -Wextra` and the same flags as `compile_flags.txt` (plus a
      locally-supplied `WH_MOD_ID` define, which the real Windhawk engine
      supplies automatically at actual build time — confirmed necessary
      because `windhawk_utils.h`'s `SetWindowSubclassFromAnyThread` macro-
      expands it).
- [x] **Real bug found via an actual in-app compile attempt (by the user) and
      fixed**: `macflow-start` (and latently `macflow-dock` and
      `macflow-notifications`) were missing `-lcomctl32`, causing a real
      linker failure (`undefined symbol: DefSubclassProc` /
      `SetWindowSubclass` / `RemoveWindowSubclass`). Fixed, then verified
      with a full `-shared` link (not just `-c`/syntax-only) against every
      real system library, using a local-only stub for the engine-internal
      `InternalWh_*` functions — see docs/TESTING.md for detail. All 4 mods
      now link cleanly.

## Phase D — Testing and screenshots (requires user participation — see constraints above)
- [x] Each mod compiles cleanly (real compiler, zero warnings) — verified by assistant
- [x] TESTING.md matrix written with REAL results only; untestable items explicitly marked
      "NOT TESTED" (assistant has no GUI/screenshot capability)
- [ ] **BLOCKED ON USER**: in-app compile check via Windhawk's own "Compile mod" step
- [ ] **BLOCKED ON USER**: Enable/disable/settings-change x20 stability pass per mod
- [ ] **BLOCKED ON USER**: Real before/after screenshots captured at 1920x1080, light+dark,
      in docs/screenshots/
- [ ] **BLOCKED ON USER**: Icon-unchanged comparison done on real screenshots
- [ ] **BLOCKED ON USER**: Idle CPU/memory check over 30 minutes

## Phase E — Public GitHub repo
- [x] mods/, docs/, README.md, LICENSE, CONTRIBUTING.md, CHANGELOG.md written locally
- [x] .github/ISSUE_TEMPLATE bug report added
- [x] CI workflow added: metadata validation (real, tested script,
      `scripts/validate_mods.py`), README link check (lycheeverse/lychee-action),
      best-effort syntax check against real mingw-w64 + the public Windhawk
      IDE headers (continue-on-error — full build needs the Windhawk app's
      private internal header, not publicly available, documented in ci.yml)
- [x] Repo github.com/Leo-Galli/macflow-windhawk created (public) and pushed
- [x] CI confirmed green on the actual repo (run 40902476578: all 3 jobs
      passed, including the syntax-check job genuinely succeeding, not just
      masked by continue-on-error)
- [ ] v1.0.0 release — BLOCKED on Phase D (do not tag/release untested code)

## Phase F — Submission to Windhawk
- [ ] Fork of ramensoftware/windhawk-mods created
- [ ] Mods added in required format under mods/
- [ ] PR(s) opened with real screenshots and real tested-builds info (blocked on Phase D)
- [ ] PR links recorded in PROGRESS.md and README.md

## Phase G — Final check
- [ ] All mods compile without warnings, load/unload cleanly, restore original look
- [ ] No icon changed (verified against real screenshots)
- [ ] No measurable idle CPU/memory leak (real observation)
- [ ] Repo public, CI green, release exists
- [ ] PRs open
