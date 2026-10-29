# MacFlow — Phase B Design

Decision basis: see `RESEARCH.md` Sections 2, 4, 5, 6. All three "Styler" mods
(Taskbar, Start Menu, Notification Center) restyle WinUI surfaces by hijacking
`InitializeXamlDiagnosticsEx` and private `IVisualTreeService3`/
`IXamlDiagnosticsTestHooks` COM interfaces not declared in any public SDK
header. Those six reference mods are GPLv3. Only one XAML-diagnostics consumer
is safe per process.

**Design decision (confirmed 2026-10-29):** MacFlow mods are original,
independent implementations built exclusively on **public, documented Windows
APIs**: DWM window attributes (`dwmapi.h`), UI Automation (`IUIAutomation`),
and standard Win32 window management/subclassing (`user32.h`,
`WindhawkUtils::SetWindowSubclassFromAnyThread`). MacFlow does not hijack XAML
diagnostics and does not reuse any GPLv3 code from the reference mods. Where a
visual goal genuinely requires per-element XAML tree edits (something only the
private diagnostics engine can do), MacFlow documents depending on the
relevant existing mod and ships a curated MacFlow theme/settings preset for it
instead of reimplementing it. This avoids license contamination, avoids
crash risk from untested from-scratch COM/WinRT code in `explorer.exe`, and
avoids the single-XAML-diagnostics-consumer conflict when a user also runs
Taskbar Styler / Start Menu Styler / Notification Center Styler.

## Visual system (shared across all MacFlow mods)

- **Theme-follow**: read `HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`
  (standard, publicly documented registry value also read by Explorer itself)
  to pick light/dark; re-check on `WM_SETTINGCHANGE` / `WM_THEMECHANGED`.
- **Translucency**: `DwmSetWindowAttribute` with `DWMWA_SYSTEMBACKDROP_TYPE`
  (`DWMSBT_MAINWINDOW` for Mica, `DWMSBT_TRANSIENTWINDOW` for acrylic-like
  flyouts) — public API, `dwmapi.h`, Windows 11 (22000+).
- **Rounded corners**: `DwmSetWindowAttribute` with `DWMWA_WINDOW_CORNER_PREFERENCE`.
  Documented limitation: DWM only exposes the preset values `DWMWCP_DEFAULT`,
  `DWMWCP_ROUND`, `DWMWCP_ROUNDSMALL`, `DWMWCP_DONOTROUND` — there is no public
  API for an arbitrary pixel radius. MacFlow uses `DWMWCP_ROUND` as the closest
  native approximation of "12px rounded corners" and documents this as a
  platform limitation rather than claiming a literal 12px value everywhere.
  Where a mod draws its own overlay chrome (dock background, indicator dots),
  MacFlow draws that geometry itself with a true 12px corner radius via GDI+
  region/path, since it fully owns that pixel content.
- **Accent color**: read the user's Windows accent color via
  `DwmGetColorizationColor` (public, `dwmapi.h`) and apply via
  `DWMWA_BORDER_COLOR` / `DWMWA_CAPTION_COLOR` where applicable, and as the
  tint for MacFlow-drawn overlay elements (indicator dots, separators).
- **Shadows**: rely on the DWM-composited window shadow that already applies
  to any top-level window with `WS_EX_*`/class-style drop shadow
  (`CS_DROPSHADOW` or the standard DWM non-client shadow for windows with a
  sizeable/popup frame) — no custom shadow renderer; this is standard and
  requires no private API.
- **Typography**: Segoe UI Variable (ships with Windows 11; MacFlow never
  bundles or substitutes a font). Any MacFlow-drawn text uses whatever the
  OS reports as the current UI font (`SystemParametersInfo` /
  `GetThemeSysFont`) rather than hardcoding a font name, so it still follows
  Windows if the display language changes the default UI font.
- **Animation**: MacFlow-drawn overlay elements (the Dock's hover-highlight)
  use the public Win32 `AnimateWindow` API with `AW_BLEND` for a 150-200ms
  OS-driven cross-fade — no custom `Sleep`/timer-loop animation. The
  Notification Center, Start menu, and Explorer window chrome (corner
  radius, backdrop material, accent border) are plain DWM window attributes
  applied once when a window is created; their open/close motion is the
  OS's own native flyout animation, unmodified by MacFlow. A full
  DirectComposition/`Windows.UI.Composition` visual-tree animation pipeline
  was considered for continuous scale effects but intentionally not used:
  that wiring could not be verified correct without interactive runtime
  testing, and an unverified COM visual tree in `explorer.exe` is a stability
  risk MacFlow chooses not to take for a cosmetic effect.
- **No icon changes, ever**: MacFlow never touches `SHGetFileInfo`,
  `IExtractIcon`, icon caches, or any per-item icon resource. Enforced as a
  review rule for every mod (see CONTRIBUTING.md).

## Components

### 1. MacFlow Dock (`macflow-dock.wh.cpp`)

Target: `explorer.exe` (the primary taskbar window, class `Shell_TrayWnd`
plus `Shell_SecondaryTrayWnd` per monitor — both are real, long-stable public
window class names used by every taskbar tool, not private).

- **Floating dock**: subclass the tray window
  (`WindhawkUtils::SetWindowSubclassFromAnyThread`) and intercept
  `WM_WINDOWPOSCHANGING` to inset the window's rect by a configurable margin
  from the screen edges (left, right, and bottom), producing a floating,
  framed appearance with equal side margins. Apply
  `DWMWA_WINDOW_CORNER_PREFERENCE` / `DWMWA_SYSTEMBACKDROP_TYPE` to the
  inset window for the rounded-translucent look. Documented limitation:
  shrinking the bar to exactly the width of its pinned/running content (true
  macOS-style content-hugging) would require controlling the taskbar's
  internal WinUI layout sizing, which is the same symbol-hook surface used by
  *Taskbar height and icon size* (`RESEARCH.md` 4.2) — MacFlow Dock does not
  attempt this and instead documents pairing with that mod for users who want
  a narrower bar in addition to the floating-margin look.
- **Auto-hide**: optional; uses the existing public auto-hide mechanism
  (`ABM_SETSTATE` / `ABS_AUTOHIDE` via `SHAppBarMessage`, the same documented
  API Windows' own taskbar auto-hide setting uses) rather than a custom
  show/hide loop.
- **Hover magnify**: off by default (perf). When enabled, uses UI Automation
  (`IUIAutomation`) point hit-testing (`ElementFromPoint`) on the existing
  subclass's `WM_MOUSEMOVE` to find the hovered taskbar button's
  `BoundingRectangle`, and shows a MacFlow-drawn accent-colored highlight
  overlay sized by `dockMagnifyScale` behind that button, faded in/out with
  `AnimateWindow(..., 150-200, AW_BLEND)` (a standard public Win32 OS-driven
  cross-fade, not a custom timer loop). Documented limitation: MacFlow does
  not resize the real WinUI taskbar icons themselves (that would require the
  private XAML tree, ruled out by the architecture decision above) and does
  not attempt a continuously-interpolated scale transform via DirectComposition
  visual trees — that would need deeper, harder-to-verify COM wiring than this
  effect's value justifies. The magnify effect is an honest, bounded highlight
  overlay, not pixel-identical native icon scaling.
- **Running-app indicator dots and pinned/running separator — EVALUATED,
  NOT IMPLEMENTED.** Verified via research (real source of
  `taskbar-collapse-to-running.wh.cpp` and
  `taskbar-recent-focus-highlight.wh.cpp` in `ramensoftware/windhawk-mods`):
  the real "is this app running" signal is the private WinRT property
  `ITaskListButton::get_IsRunning`, reachable only by symbol-hooking
  `winrt::Taskbar::implementation::TaskListButton`'s internal implementation
  — exactly the private-internals hooking this architecture decision rules
  out. UI Automation's public automation peer for taskbar buttons
  (`Taskbar.TaskListButtonAutomationPeer`) does not expose running state as a
  structured property at all; it is only present as locale-dependent text
  embedded in the accessible name (e.g. "App - 2 running windows"), which is
  not a reliable basis for a visual indicator. No separator boundary signal
  exists either, for the same reason. MacFlow therefore does not ship these
  two features from the original design brief — documented here and in the
  mod's own readme rather than shipped on an unverified heuristic.
- **Settings**: `dockMarginPx` (int, default 8), `dockAutoHide` (bool, default
  false), `dockMagnifyEnabled` (bool, default false), `dockMagnifyScale`
  (percent, default 130), `dockCornerStyle` (enum: round/roundsmall/square,
  default round), `dockBackdrop` (enum: mica/acrylic/none, default mica).
- **Fallback**: if `Shell_TrayWnd`/`Shell_SecondaryTrayWnd` is not found, or
  UI Automation enumeration of taskbar buttons fails (e.g. a future Windows
  build renames the automation tree), the mod logs via `Wh_Log` and leaves
  the taskbar window geometry untouched rather than guessing — it never
  forces a resize without a successfully resolved window handle.
- **Depends on / complements**: for deep per-element taskbar visual edits
  beyond geometry/overlay (e.g. recoloring the native app-list background
  brush), MacFlow documents in its README that users who also run
  *Windows 11 Taskbar Styler* can configure that mod's own styling settings
  directly for further customization. MacFlow does not ship a preset file
  for that mod in v1.0.0 (authoring one correctly would require deep,
  independently-verified knowledge of that mod's selector/style DSL this
  project does not yet have — see RESEARCH.md 4.1). MacFlow Dock does not
  require that mod and does not duplicate its function.

### 2. MacFlow Menu Bar — DROPPED

Evaluated and not implemented. Reasoning:

- Windows 11 has no secondary top-of-screen shell surface that can be safely
  repurposed into a persistent "menu bar" without either (a) drawing a brand
  new always-on-top overlay window that is not actually part of the shell —
  which would just be a separate background program masquerading as a
  Windhawk mod, explicitly disallowed by the brief — or (b) restyling the
  existing tray/clock region, which is already the documented job of
  *Taskbar Clock Customization* and *Windows 11 Taskbar Styler*
  (`RESEARCH.md` 4.1, 4.5). Doing it again would duplicate, not complement,
  those mods.
- A technically sound, non-duplicate scope for this slot would be: nothing
  new. MacFlow instead documents, in the README, a recommended companion
  configuration of *Taskbar Clock Customization* for a slim top-bar feel
  (small height, centered clock, accent color), rather than shipping a
  `macflow-menubar.wh.cpp` that does the same thing under a different name.

### 3. MacFlow Notifications (`macflow-notifications.wh.cpp`)

Target: `ShellExperienceHost.exe` and `ShellHost.exe` (the Notification
Center / Quick Settings flyout hosts — same processes as the reference
Notification Center Styler, confirmed in `RESEARCH.md` 4.4; MacFlow only
applies window-level attributes to their top-level flyout window, it does not
touch their internal XAML tree).

- Original scope, independent of the XAML-diagnostics engine: on window
  creation, hook `CreateWindowExW` (public, `user32.h`) and filter by class
  name. **Real, verified class names** (confirmed from the live source of
  `windows-11-notification-center-styler.wh.cpp` and
  `refined-quick-settings-and-sound.wh.cpp`, both present as reference mods):
  on Windows 11 24H2+, both Notification Center and Quick Settings are
  hosted in `ShellHost.exe` under the dedicated class `ControlCenterWindow`
  — this is exactly MacFlow's intended target pair, styled identically, so
  no further disambiguation is needed on 24H2+. On 23H2 and earlier, both
  surfaces are hosted in `ShellExperienceHost.exe` under the generic class
  `Windows.UI.Core.CoreWindow`, which is also shared by **toast notification
  popups** in the same process — there is no public class-name-only way to
  tell them apart. MacFlow's 23H2 fallback adds a height heuristic (only
  style windows taller than ~400 effective pixels, DPI-scaled via
  `GetDpiForWindow`): Notification Center/Quick Settings panels are
  reliably much taller than a toast card. This is a documented heuristic,
  not a guarantee — an unusually tall stacked-toast case could theoretically
  false-positive, and this is called out explicitly in TESTING.md and the
  mod's readme rather than presented as a solved distinction. Once a target
  window is confirmed, apply: `DWMWA_WINDOW_CORNER_PREFERENCE` (round),
  `DWMWA_SYSTEMBACKDROP_TYPE` (acrylic/transient), and an accent-colored
  `DWMWA_BORDER_COLOR`.
- Spacing/padding between individual notification/quick-settings cards is an
  internal XAML layout property MacFlow does not edit directly (would require
  the private diagnostics engine). Documented as a limitation; MacFlow
  recommends pairing with *Windows 11 Notification Center Styler*'s own
  settings for that level of control, rather than shipping a MacFlow-authored
  preset it has not independently verified against that mod's styling DSL.
- **Settings**: `notifCornerStyle` (enum, default round), `notifBackdrop`
  (enum: mica/acrylic/none, default acrylic), `notifAccentBorder` (bool,
  default true).
- **Fallback**: if the flyout window class name is not found on a given
  build, the hook is a no-op (guarded `if (!hwnd) return;` pattern) — no
  crash, no forced styling.

### 4. MacFlow Start (`macflow-start.wh.cpp`)

Target: `StartMenuExperienceHost.exe`.

- Same window-level technique as Notifications: hook `CreateWindowExW`
  filtered to class `Windows.UI.Core.CoreWindow` within
  `StartMenuExperienceHost.exe` (confirmed from the live source of
  `windows-11-start-menu-styler.wh.cpp`; no dedicated class exists for Start
  even on 24H2 — it remains `CoreWindow`-hosted). Unlike the Notification
  Center case, `StartMenuExperienceHost.exe` does not also host toast
  popups, so the ambiguity risk is lower, but the same DPI-scaled minimum
  size heuristic is applied defensively in case a future Windows build hosts
  another `CoreWindow` surface (e.g. a tooltip or small flyout) in the same
  process. Apply DWM corner preference, backdrop type, and accent border to
  the matched window once created.
- "Centered launchpad-like panel, larger spacing" at the per-tile layout
  level is internal WinUI XAML layout, not editable without the private
  diagnostics engine used by *Windows 11 Start Menu Styler*
  (`RESEARCH.md` 4.3). MacFlow recommends pairing with that mod's own
  settings for layout control, rather than shipping a MacFlow-authored preset
  it has not independently verified. Icons are never touched (same icons,
  same grid, same pinned tiles — only window chrome: corners, backdrop,
  border).
- **Settings**: `startCornerStyle` (enum, default round), `startBackdrop`
  (enum: mica/acrylic/none, default mica), `startAccentBorder` (bool, default
  true).
- **Fallback**: same guarded no-op pattern as Notifications.

### 5. MacFlow Explorer (`macflow-explorer.wh.cpp`)

Target: `explorer.exe` (File Explorer folder windows, class
`CabinetWClass`/`ExploreWClass` — standard, long-stable public window
classes).

- File Explorer's navigation pane (`SysTreeView32`, a standard Win32 common
  control) and folder view are **not** part of the private XAML diagnostics
  surface — they're classic Win32/Shell, so meaningful original restyling is
  possible here without depending on any reference mod:
  - DWM window attributes on the Explorer top-level window: rounded corners,
    Mica/acrylic backdrop, accent border — consistent title bar styling.
  - Sidebar translucency: applies `SetWindowTheme(hwndTreeView, L"Explorer",
    nullptr)` on the navigation pane (the standard, long-documented technique
    apps use to opt a `SysTreeView32` into Explorer-style theming). A true
    see-through Mica bleed into the classic `SysTreeView32` client area (via
    `DwmExtendFrameIntoClientArea` plus skipping `WM_ERASEBKGND`) was
    evaluated and intentionally **not** shipped in v1.0.0: that technique
    normally also requires custom non-client frame handling
    (`WM_NCCALCSIZE`) to work correctly, which this mod does not take over
    from Explorer's own window procedure, and shipping an unverified partial
    implementation risked a visibly broken (flickering or solid black) strip
    instead of translucency. This is a documented, honest limitation, not a
    silent gap: users who want real sidebar translucency should use
    **Windows 11 File Explorer Styler**'s "Translucent Explorer11" theme,
    which operates at the WinUI/Direct2D layer where this is proven to work.
  - Rounded selection highlight: custom-draw (`NM_CUSTOMDRAW`) draws a
    12px-radius accent-colored rounded outline on top of the existing native
    selection highlight for the focused/selected item, rather than replacing
    the native highlight outright (suppressing all default item drawing to
    redraw it from scratch was evaluated as higher-risk for no clear visual
    benefit). This is an additive accent, not a native highlight
    replacement — documented as such.
  - Toolbar/header spacing: evaluated, not shipped in v1.0.0. The WinUI3
    ribbon command bar (24H2) is internal XAML layout, out of scope for the
    same reason as Start/Notifications; the classic `ToolbarWindow32`
    padding adjustment (`TB_SETPADDING`) was considered for older-style
    toolbars but dropped to keep this mod's settings surface matching what
    is documented and tested, rather than shipping an extra unconfigurable
    behind-the-scenes tweak. No reference mod exists for this specific
    surface per `RESEARCH.md` Phase A scope.
- **No icon changes**: file/folder icons are drawn by the shell's own
  icon-handler pipeline (`IExtractIcon`/`SHGetFileInfo`) and MacFlow's
  custom-draw hook only modifies the selection background/border, never the
  icon bitmap region.
- **Depends on / complements**: a fourth reference mod in the same GPLv3
  styler family, *Windows 11 File Explorer Styler* (`RESEARCH.md` Section 7),
  already covers deep per-element Explorer theming via the same private
  engine. MacFlow recommends that mod's own built-in themes for users who
  want that level of control, instead of duplicating it.
- **Settings**: `explorerCornerStyle` (enum, default round), `explorerBackdrop`
  (enum: mica/acrylic/none, default mica), `explorerSidebarTranslucent`
  (bool, default true), `explorerRoundedSelection` (bool, default true).
- **Fallback**: every `FindWindowEx`/control lookup is guarded; if the
  navigation pane or toolbar control isn't found by class name (e.g. a
  future Explorer rebuild changes the control tree), that specific visual
  touch is skipped without affecting the rest of the window or crashing.

## Settings architecture

Each mod has its own `==WindhawkModSettings==` block (per-component, as
required), but all five mods share a conceptual settings vocabulary — corner
style, backdrop type, accent toggle — implemented consistently (not via a
shared library, since each `.wh.cpp` is a standalone compiled unit per the
platform's own format) so a future "MacFlow meta-settings" convenience script
is at least mechanically possible without the mods needing a redesign. Live
reload is handled via `Wh_ModSettingsChanged`, re-reading settings and
re-applying DWM attributes to currently tracked windows immediately (no
restart required).

## Performance and stability baseline (applies to all 5 mods)

- Every hook installed in `Wh_ModInit`; no `Wh_ApplyHookOperations` (documented
  in `windhawk_api.h` as "very slow").
- UI Automation event handlers (`IUIAutomationEventHandler`) are used instead
  of polling; subscriptions are released in `Wh_ModUninit`.
- Overlay windows created by Dock are destroyed in `Wh_ModUninit`; DWM
  attributes applied to Explorer/Start/Notification windows are not
  reversible in-place for already-closed windows (those windows close/reopen
  naturally with the user's next interaction), but the mod stops applying
  them to any newly created window immediately once unloaded, matching the
  "restore the original look" requirement as literally as the platform allows
  for transient flyout windows.
- Every COM/UIA call is wrapped and checked for `FAILED(hr)`/`nullptr` before
  use; no call result is assumed to succeed.
