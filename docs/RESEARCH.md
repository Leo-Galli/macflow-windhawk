# MacFlow — Phase A Research: Windhawk Mod Platform

Research date: 2026-10-29. Scope: Windhawk mod source format, submission rules, and
the actual source of six existing `ramensoftware/windhawk-mods` mods relevant to a
macOS-style visual overhaul of the Windows 11 shell (no icon changes). All claims
below are sourced; anything not directly verifiable is flagged explicitly rather
than guessed.

All six mod files were downloaded verbatim via `gh api
repos/ramensoftware/windhawk-mods/contents/mods/<file>` (GitHub Contents API,
`main` branch) on 2026-10-29. Header files were downloaded from
`.vscode/windhawk_headers_1.7.3/` in the same repo — the exact headers the repo's
VS Code workspace (and therefore the editor's IntelliSense) targets for Windhawk
engine **v1.7.3**.

---

## 1. Mod source format (verified)

Source: GitHub wiki
[`ramensoftware/windhawk/wiki/Creating-a-new-mod`](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod)
and [`Development-tips`](https://github.com/ramensoftware/windhawk/wiki/Development-tips),
cross-checked against the repo's own validator,
[`.github/pr_validation.py`](https://github.com/ramensoftware/windhawk-mods/blob/main/.github/pr_validation.py).

A mod is a single `.wh.cpp` file with a metadata block:

```cpp
// ==WindhawkMod==
// @id              <mod-id>
// @name            <display name>
// @description     <short description>
// @version         <version>
// @author          <author>
// @github          <github profile URL>
// @twitter         <twitter profile URL>       (optional)
// @homepage        <website URL>               (optional)
// @donateUrl       <sponsor URL>                (optional)
// @license         <SPDX identifier>            (optional, default MIT)
// @include         <process name / pattern>
// @exclude         <process name / pattern>    (optional, repeatable)
// @architecture    x86-64 | x86 | arm64         (optional, repeatable)
// @compilerOptions <extra clang/linker flags>   (optional)
// ==/WindhawkMod==
```

Confirmed metadata field categories from `pr_validation.py` (`MOD_METADATA_PARAMS`):
- **Single-value**: `id`, `version`, `github`, `twitter`, `homepage`,
  `compilerOptions`, `license`, `donateUrl`.
- **Single-value, localizable**: `name`, `description`, `author`.
- **Multi-value** (repeatable): `include`, `exclude`, `architecture`.

**`@id` validation rule** (from `pr_validation.py`, function `validate_id`):
regex `([0-9a-z]+-)*[0-9a-z]+` (lowercase letters, digits, dashes only), length
6–48 characters, must equal the filename minus `.wh.cpp`, and **cannot be
changed** once a mod exists in the repo.

**`@version` validation rule** (`validate_version`): regex `([0-9]+\.)*[0-9]+`
(digits and dots only); a resubmitted/updated mod's version must be strictly
greater than the previously published one.

**`@license` validation rule** (`validate_license`): must be a valid
[SPDX license identifier](https://spdx.org/licenses/) — the validator fetches
`https://spdx.org/licenses/licenses.json` and checks membership. Per the repo
README: *"Mods which don't specify a license are submitted under the MIT
license. It's the author's responsibility to specify the appropriate license
for third-party code."*

A `// ==WindhawkModReadme== /* ... */ // ==/WindhawkModReadme==` Markdown block
provides the catalog description, and an optional
`// ==WindhawkModSettings== /* YAML */ // ==/WindhawkModSettings==` block
declares user-configurable settings (YAML-like syntax, each item has a default
value and optional `$name`/`$description`).

Compiler: per the wiki, *"As of version 1.7, Windhawk uses Clang 20
(mingw-w64 toolchain) and compiles the mods in C++23 mode."*

### Mod lifecycle callbacks (implemented BY the mod, called BY the engine)

Source: `.github/pr_validation.py`, dict `CALLBACK_SIGNATURES` (the repo's own
ground truth for valid signatures — a mod with a mismatched signature fails CI),
corroborated by the wiki.

```c
BOOL Wh_ModInit();                           // required; return FALSE aborts load
void Wh_ModAfterInit();                      // optional
void Wh_ModBeforeUninit();                   // optional, hooks still active
void Wh_ModUninit();                         // optional, hooks removed
void Wh_ModSettingsChanged();                // optional, OR:
BOOL Wh_ModSettingsChanged(BOOL* bReload);   // optional, alternate signature
```

There are also tool-mod-only entry points (mods that run inside Windhawk's own
process rather than being injected elsewhere):
`BOOL WhTool_ModInit()`, `void WhTool_ModUninit()`,
`void WhTool_ModSettingsChanged()`, `void WhTool_ModEntryPoint()`. Tool mods'
`@include` must be exactly one of a small fixed set of Windhawk's own process
names (`windhawk.exe`, `windhawk-mod.exe`, etc.) per
`TOOL_MOD_ALLOWED_INCLUDES` in the same file — not relevant to MacFlow's shell
styling goals but noted for completeness.

---

## 2. Core Windhawk API surface (verified against real headers)

Source files, downloaded verbatim from the mods repo's own pinned copy of the
engine headers:
[`.vscode/windhawk_headers_1.7.3/windhawk_api.h`](https://github.com/ramensoftware/windhawk-mods/blob/main/.vscode/windhawk_headers_1.7.3/windhawk_api.h)
and
[`.vscode/windhawk_headers_1.7.3/windhawk_utils.h`](https://github.com/ramensoftware/windhawk-mods/blob/main/.vscode/windhawk_headers_1.7.3/windhawk_utils.h).
These are the actual headers mods `#include <windhawk_utils.h>` against; nothing
below is inferred.

### `windhawk_api.h` (functions available to every mod, `WH_MOD`-gated)

```c
void Wh_Log(PCWSTR message, ...);

int    Wh_GetIntValue(PCWSTR valueName, int defaultValue);
BOOL   Wh_SetIntValue(PCWSTR valueName, int value);
size_t Wh_GetStringValue(PCWSTR valueName, PWSTR stringBuffer, size_t bufferChars);
BOOL   Wh_SetStringValue(PCWSTR valueName, PCWSTR value);
size_t Wh_GetBinaryValue(PCWSTR valueName, void* buffer, size_t bufferSize);
BOOL   Wh_SetBinaryValue(PCWSTR valueName, const void* buffer, size_t bufferSize);
BOOL   Wh_DeleteValue(PCWSTR valueName);                      // since v1.5
size_t Wh_GetModStoragePath(PWSTR pathBuffer, size_t bufferChars);

int    Wh_GetIntSetting(PCWSTR valueName, ...);               // printf-style valueName
PCWSTR Wh_GetStringSetting(PCWSTR valueName, ...);            // must free with below
void   Wh_FreeStringSetting(PCWSTR string);

BOOL Wh_SetFunctionHook(void* targetFunction, void* hookFunction, void** originalFunction);
BOOL Wh_RemoveFunctionHook(void* targetFunction);             // since v1.0
BOOL Wh_ApplyHookOperations();                                // since v1.0, "very slow"

HANDLE Wh_FindFirstSymbol(HMODULE hModule, const WH_FIND_SYMBOL_OPTIONS* options, WH_FIND_SYMBOL* findData);
BOOL   Wh_FindNextSymbol(HANDLE symSearch, WH_FIND_SYMBOL* findData);
void   Wh_FindCloseSymbol(HANDLE symSearch);

BOOL Wh_Disasm(void* address, WH_DISASM_RESULT* result);      // since v1.2

const WH_URL_CONTENT* Wh_GetUrlContent(PCWSTR url, const WH_GET_URL_CONTENT_OPTIONS* options); // since v1.5
void Wh_FreeUrlContent(const WH_URL_CONTENT* content);        // since v1.5
```

Key documented semantics (from the header's own doc-comments):
- `Wh_SetFunctionHook` "Can't be called after `Wh_ModBeforeUninit` returns."
- `Wh_GetStringSetting` result "When no longer needed, free the memory with
  `Wh_FreeStringSetting`."
- `Wh_ApplyHookOperations`: "Note: This function is very slow, avoid using it
  if possible. Ideally, all hooks should be set in `Wh_ModInit`."

### `windhawk_utils.h` (`namespace WindhawkUtils`, opt-in convenience layer)

```cpp
struct SYMBOL_HOOK {
    template <typename Prototype>
    SYMBOL_HOOK(std::initializer_list<std::wstring_view> symbols,
                Prototype** originalFunction,
                std::type_identity_t<Prototype*> hookFunction = nullptr,
                bool optional = false);
    // + an overload taking `nullptr_t` in place of originalFunction (since v1.7.1)
    SYMBOL_HOOK() = default;                       // since v1.4.1
};

bool HookSymbols(HMODULE module,
                  const SYMBOL_HOOK* symbolHooks,
                  size_t symbolHooksCount,
                  const WH_HOOK_SYMBOLS_OPTIONS* options = nullptr);
// since v1.3; options param since v1.5. Caches symbol results to avoid
// re-enumerating the module's symbols on every call.

using WH_SUBCLASSPROC = LRESULT(CALLBACK*)(HWND, UINT, WPARAM, LPARAM, DWORD_PTR);
BOOL SetWindowSubclassFromAnyThread(HWND hWnd, WH_SUBCLASSPROC pfnSubclass, DWORD_PTR dwRefData); // since v1.3
void RemoveWindowSubclassFromAnyThread(HWND hWnd, WH_SUBCLASSPROC pfnSubclass);

template <typename Prototype>
BOOL SetFunctionHook(Prototype* targetFunction, Prototype* hookFunction, Prototype** originalFunction); // since v1.5, type-safe wrapper over Wh_SetFunctionHook

class StringSetting {   // since v1.4, RAII wrapper; auto-calls Wh_FreeStringSetting
  public:
    static StringSetting make(PCWSTR valueName, Args... args);
    operator PCWSTR() const;
};
```

`SYMBOL_HOOK` matters enormously for MacFlow: it takes a **list** of symbol name
candidates (demangled C++ names) per hook, the first one found is used, and
entries can be marked `optional = true` so a missing symbol on some Windows
build doesn't abort the whole batch. `HookSymbols` pulls public symbol PDBs
(from Microsoft's symbol server by default, or a custom `symbolServer` /
`onlineCacheUrl`) to resolve addresses by demangled name — this is how mods hook
internal, unexported, unversioned functions inside `explorer.exe`,
`twinui.pcshell.dll`, `windows.storage.dll`, etc. **This is also the mechanism
most exposed to Windows-build churn** — see Section 4.

Not found in either header, and not used by any of the six mods inspected:
any `Wh_ModInit`/`Wh_ModUninit`-style declarations (those are callbacks the mod
itself defines, not API the engine exports) — matches expectation from Section 1.

---

## 3. Submission / contribution rules (`ramensoftware/windhawk-mods`)

Source: repo
[`README.md`](https://github.com/ramensoftware/windhawk-mods/blob/main/README.md)
and
[`.github/pull_request_template.md`](https://github.com/ramensoftware/windhawk-mods/blob/main/.github/pull_request_template.md).
There is **no separate `CONTRIBUTING.md`** in this repo — the README itself
contains the contribution rules, and mod-authoring is instead documented in the
`ramensoftware/windhawk` wiki (linked from the README).

- New mods: one PR = one new file, path **must** be `mods/<mod-id>.wh.cpp`.
- `@github` metadata must match the PR author's own GitHub profile (enforced by
  CI label/metadata checks referenced in `pr_validation.py`, e.g.
  `DISALLOWED_AUTHORS`, `ALLOWED_AUTHOR_NAME_CHANGES`).
- `@twitter`, if present, requires a human reviewer to add a
  `twitter-verified` label after confirming the account really belongs to the
  same author (`TWITTER_VERIFIED_LABEL` constant).
- Mod **updates**: only the original submitter (matched by `@github`) can
  submit a new version of an existing mod id; `@id` must be unchanged,
  `@version` must increase. If you're not the original author, the README
  instructs you to either send your changes to that author or submit your
  idea as a brand-new mod.
- No license specified → MIT by default; otherwise must be a valid SPDX id.
- PR template requires: a changelog block (for updates) between
  `<!-- changelog:start -->` / `<!-- changelog:end -->` markers, and for new
  mods, a checkbox disclosure of whether the mod was written unassisted, with
  AI assistance, or primarily by an AI tool (Claude/ChatGPT/Gemini/other) —
  explicitly stated to *not* affect acceptance, only reviewer context.
- CI (`pr_validation.py`) is strict and mechanical: validates id/version/license
  format as above, URL syntax, callback signatures against
  `CALLBACK_SIGNATURES`, disallows redefining `Wh_`/`WH_` symbols, and more
  (full ruleset not exhaustively covered here — see the script for the
  complete list if a MacFlow mod fails CI).

**Practical implication for MacFlow**: if MacFlow intends to upstream any mod
into `ramensoftware/windhawk-mods` itself (vs. distributing a private repo/JSON
feed), one GitHub identity must own each mod id going forward, mod ids are
permanent, and every third-party code snippet reused must carry a
license compatible with (or itself) a valid SPDX identifier that the author is
willing to declare.

---

## 4. The six existing mods (real source inspected)

All six are authored by **m417z** (github.com/m417z), and all six declare the
**same license statement** in a code comment immediately below the metadata
block (not in `@license` metadata itself, interestingly — see "License notes"
below):

> "Source code is published under The GNU General Public License v3.0."

Each also says bug reports go to
`https://github.com/ramensoftware/windhawk-mods/issues`, and development/PRs
happen in the author's own staging repo `https://github.com/m417z/my-windhawk-mods`.

Exact filenames confirmed via `gh api repos/ramensoftware/windhawk-mods/contents/mods`:

| MacFlow interest | Actual filename | `@id` |
|---|---|---|
| Taskbar Styler | `windows-11-taskbar-styler.wh.cpp` | `windows-11-taskbar-styler` |
| Taskbar height/icon size | `taskbar-icon-size.wh.cpp` | `taskbar-icon-size` |
| Start Menu Styler | `windows-11-start-menu-styler.wh.cpp` | `windows-11-start-menu-styler` |
| Notification Center Styler | `windows-11-notification-center-styler.wh.cpp` | `windows-11-notification-center-styler` |
| Taskbar Clock Customization | `taskbar-clock-customization.wh.cpp` | `taskbar-clock-customization` |
| Better file sizes | `explorer-details-better-file-sizes.wh.cpp` | `explorer-details-better-file-sizes` |

Note the id/filename for "Taskbar height and icon size" is `taskbar-icon-size`,
not the more literal name one might guess — confirmed from the file's own
`@id`/`@name` header, not assumed.

### 4.1 Windows 11 Taskbar Styler (`windows-11-taskbar-styler.wh.cpp`, v1.10)

- `@include explorer.exe`; `@architecture x86-64` (64-bit only);
  `@compilerOptions -lcomctl32 -lgdi32 -lole32 -loleaut32 -lruntimeobject -lshlwapi`.
- **Technique**: hooks `InitializeXamlDiagnosticsEx` (resolved via
  `GetProcAddress` on the WinUI/XAML module) to self-inject a XAML diagnostics
  endpoint into `explorer.exe`, then drives the taskbar's live WinUI visual
  tree through the `IXamlDiagnosticsTestHooks` COM interface (a private
  interface — "`xamlom.h` does not declare it", per the mod's own comment) and
  `IVisualTreeService3`/`IVisualTreeServiceCallback2` to read/mutate XAML
  elements and resource dictionaries at runtime. This is a **general-purpose
  XAML Diagnostics hijack**, not a documented public Windows API.
  Code citation (verbatim comment in source, lines ~10949–10957):
  > "The XAML composition diagnostics rebuild a process-wide visual tree walker
  > without any locking whenever a DirectComposition visual is added, so any UI
  > thread which adds one corrupts the heap while another thread is in the same
  > code... composition diagnostics are kept from being created at all:
  > `XamlDiagnostics::CreateCompVisualDiag` skips them when the
  > `HKLM\Software\Microsoft\XAML\Debug\DisableCompositionDiag` value is 1."
  This documents a real, load-bearing registry-key workaround for a
  Microsoft-internal race condition.
- Also hooks `InitializeXamlDiagnosticsEx` defensively to detect/alert on
  *other* consumers of XAML diagnostics in the same process (handling modes
  `kAlert`/`kBlock`/`kAllow`), because only one XAML diagnostics consumer can
  safely run per process at a time — **this is the single biggest
  cross-mod-compatibility hazard for a suite like MacFlow** (see Section 5).
- Styling model: a custom CSS-like "target selector" language
  (`Class#Name[Property=Value] > Child@VisualState`) mapped onto the real XAML
  element tree, plus a `WindhawkBlur` synthetic XAML element exposing
  `BlurAmount`/`TintColor`/`TintOpacity`/etc. for acrylic/mica-like blur
  effects — directly relevant to a "macOS vibrancy" look.
- 40+ community themes are bundled/linked from a sibling repo
  [`windows-11-taskbar-styling-guide`](https://github.com/ramensoftware/windows-11-taskbar-styling-guide).
- **Version sensitivity**: no `WinVersion` enum in this file the way the clock
  mod has one, but the open-issues list (below) shows it repeatedly breaks on
  new Insider/production builds because it depends on the current WinUI
  resource-dictionary structure, which Microsoft changes without notice.
- **Known open issues** (via `gh issue list --label mod/windows-11-taskbar-styler`,
  fetched 2026-10-29, https://github.com/ramensoftware/windhawk-mods/issues):
  - #6546 "Fails to render the taskbar correctly after updating Windows Insider"
  - #5273 "Since Build 29648, sometimes styling silently fails"
  - #4250 "Styles not applying on OS build 26100 (LTSC 24H2)"
  - #3590 "Windows 11 Taskbar Styler loses acrylic/blur effect when Start11
    v2.0.3 is loaded" (third-party taskbar tool conflict)
  - #3622 click-hitbox offset bug when styling `Grid#SystemTrayFrameGrid`

### 4.2 Taskbar height and icon size (`taskbar-icon-size.wh.cpp`, v1.3.10)

- `@include explorer.exe`; `@architecture x86-64`;
  `@compilerOptions -lole32 -loleaut32 -lruntimeobject -lshcore -lversion`.
- README states explicitly: **"Only Windows 11 is supported. For older Windows
  versions check out 7+ Taskbar Tweaker."**
- **Technique**: `WindhawkUtils::SYMBOL_HOOK` / `HookSymbols` against
  demangled C++ symbols inside the taskbar's WinRT/WinUI implementation, e.g.
  (verbatim from source):
  ```
  public: __cdecl winrt::impl::consume_WindowsUdk_UI_Shell_ITaskbarSettings<...>::Size(void) const
  private: double __cdecl winrt::SystemTray::implementation::SystemTrayController::GetFrameSize(enum winrt::WindowsUdk::UI::Shell::TaskbarSize)
  private: void __cdecl winrt::SystemTray::implementation::SystemTrayController::UpdateFrameSize(void)
  ```
  Several of these hooks are marked `optional = true` with comments like
  *"Missing in older Windows 11 versions"* / *"From Windows 11 version 22H2,
  inlined sometimes"* — i.e., the mod author has already hit, and coded around,
  cross-build symbol churn. This is direct evidence of the symbol-availability
  risk described generically in Section 2.
- No `WinVersion`/build-number gating logic like the clock mod; it instead
  treats each hook target as independently optional and falls back gracefully
  per-symbol.
- **Known open issues**:
  - #6717 "Custom taskbar height NOT work (Win11 26300.9818)"
  - #6238 "Bottom right icons get clipped since 2026-10 windows update"
  - #5422 "stopped working after updating from version 1.3.10" (i.e. even a
    mod update didn't survive a later OS update)
  - #4313 ExplorerPatcher interaction bug

### 4.3 Windows 11 Start Menu Styler (`windows-11-start-menu-styler.wh.cpp`, v1.7)

- `@include StartMenuExperienceHost.exe`, `SearchHost.exe`, `SearchApp.exe`;
  `@architecture x86-64`;
  `@compilerOptions -lcomctl32 -lole32 -loleaut32 -lruntimeobject -lversion`.
- **Technique**: the same XAML-Diagnostics/`IVisualTreeService3`/
  `VisualTreeWatcher` machinery as the Taskbar Styler (19 occurrences of
  `VisualTreeWatcher`/`IVisualTreeServiceCallback2`/
  `IID_IXamlDiagnosticsTestHooks` found in the file) — strongly suggesting a
  shared internal library/copy-paste base across all three "Styler" mods by
  the same author.
  Same selector-language styling model (`Class#Name[...]`) as the taskbar
  styler.
  The README candidly notes there's no way to statically decode the Start
  menu's compiled XAML resource (`.xbf`) files for recent Windows builds, so
  the author publishes a manually-extracted `StartResources.xbf` as a
  convenience, and recommends the companion `UWPSpy` tool
  (https://ramensoftware.com/uwpspy) for live inspection.
- **Known open issues**:
  - #4256 "The mod is broken / not displaying correctly"
  - #2700 styles not restored correctly when `RequestedTheme` changes
    (light/dark theme switching is a likely MacFlow interaction point)

### 4.4 Windows 11 Notification Center Styler (`windows-11-notification-center-styler.wh.cpp`, v1.8)

- `@include ShellExperienceHost.exe`, `ShellHost.exe`; `@architecture x86-64`;
  `@compilerOptions -lcomctl32 -lole32 -loleaut32 -lruntimeobject -lshlwapi`.
- Same XAML-diagnostics engine again (27 occurrences of the same
  `VisualTreeWatcher`/`IXamlDiagnosticsTestHooks` symbols) and the same
  selector/style DSL, confirming a single shared styling core reused across
  Taskbar/Start Menu/Notification Center stylers.
- Fewer open issues reported (#6506 card-flicker on clear, #1999 media
  transport controls visual bug) — suggests this surface is comparatively more
  stable than the taskbar styler, likely because the Notification/Action
  Center XAML tree changes less often than the taskbar's.

### 4.5 Taskbar Clock Customization (`taskbar-clock-customization.wh.cpp`, v1.8.1)

- `@include explorer.exe`; `@architecture x86-64`;
  `@compilerOptions -ldxgi -lole32 -loleaut32 -lpdh -lpowrprof -lruntimeobject -lshlwapi -lversion -lwininet`.
- README: **"Only Windows 10 64-bit and Windows 11 are supported."**
- **Technique**: `WindhawkUtils::HookSymbols` against `explorer.exe`'s own
  symbols plus a system-tray-module hook path, *not* XAML diagnostics. It also
  has an explicit, enumerated version-detection scheme — the clearest
  build-targeting logic of the six mods:
  ```cpp
  enum class WinVersion { Unsupported, Win10, Win11, Win11_22H2, Win11_24H2 };

  WinVersion GetExplorerVersion() {
      // reads explorer.exe's own VS_FIXEDFILEINFO (file version resource)
      // major==10, then by `build` (HIWORD of the low DWORD):
      //   build < 22000        -> Win10
      //   build <= 22000       -> Win11        (21H2/original Win11)
      //   build < 26100        -> Win11_22H2   (covers 22H2 and 23H2)
      //   else                 -> Win11_24H2
  }
  ```
  i.e. the mod distinguishes exactly 4 buckets (Win10 / Win11-RTM / Win11
  22H2-23H2 / Win11 24H2+) by reading the **file version of `explorer.exe`
  itself** via `GetModuleVersionInfo`/`VS_FIXEDFILEINFO` — not via
  `RtlGetVersion` or the Windows version-lie-prone `GetVersionEx` family. This
  is the only one of the six mods with this level of explicit version bucketing
  visible in a grep for build numbers.
- Also has first-class **ExplorerPatcher compatibility code**
  (`HookExplorerPatcherSymbols`, `IsExplorerPatcherModule`,
  `HandleLoadedExplorerPatcher`): when ExplorerPatcher's taskbar module is
  detected loaded in `explorer.exe`, the mod re-hooks a parallel set of
  ExplorerPatcher-exported mangled symbols (e.g.
  `?UpdateTextStringsIfNecessary@ClockButton@@AEAAIPEA_N@Z`) and treats the
  effective taskbar as `WinVersion::Win10` regardless of actual OS version,
  because ExplorerPatcher reimplements a Windows-10-style taskbar on Windows
  11. **This is direct, load-bearing evidence that ExplorerPatcher compatibility
  is treated as a first-class concern by the most popular mod author**, which
  MacFlow should budget for if it wants broad compatibility.
  Per-settings docs: "Text styles" (custom clock font/color) are gated to
  *"Windows 11 version 22H2 and newer"* only.
- **Known open issues** are mostly feature requests (GPU/weather/plugin asks),
  not breakage — this mod appears to be the most stable of the six, likely
  because it hooks explorer.exe's own clock-formatting symbols rather than
  reaching into the fast-moving system tray/taskbar WinUI tree.

### 4.6 Better file sizes in Explorer details (`explorer-details-better-file-sizes.wh.cpp`, v1.6)

- `@include *` (all processes) with a long `@exclude` list (`conhost.exe`,
  `Plex*.exe`, `backgroundTaskHost.exe`, `LockApp.exe`, `SearchHost.exe`,
  `ShellExperienceHost.exe`, `StartMenuExperienceHost.exe`,
  `msedgewebview2.exe`, `windhawk.exe`, `windhawk-ui.exe`, `*\UI\VSCodium.exe`).
  `@compilerOptions -lole32 -loleaut32 -lpropsys`. No `@architecture`
  restriction stated (unlike the other five, which are all x86-64-only).
- **Technique**: entirely different from the Styler family — no XAML
  diagnostics at all. It uses `WindhawkUtils::HookSymbols` against
  **`windows.storage.dll`**, hooking `CFSFolder` COM-ish virtual methods by
  their demangled, calling-convention-qualified names, e.g. (verbatim,
  with explicit 32-/64-bit calling-convention branches):
  ```cpp
  #ifdef _WIN64
  LR"(public: virtual long __cdecl CFSFolder::GetDetailsEx(struct _ITEMID_CHILD const __unaligned *,struct _tagpropertykey const *,struct tagVARIANT *))"
  #else
  LR"(public: virtual long __stdcall CFSFolder::GetDetailsEx(struct _ITEMID_CHILD const *,struct _tagpropertykey const *,struct tagVARIANT *))"
  #endif
  ```
  Note the `__cdecl` vs `__stdcall` and pointer-qualifier (`__unaligned`)
  differences between the 64-bit and 32-bit symbol names for the *same*
  logical function — concrete proof that `SYMBOL_HOOK` entries are brittle not
  just across Windows versions but across architecture/bitness too.
  It optionally integrates with the third-party **"Everything" search engine**
  (voidtools) via its SDK for fast folder-size queries, with a documented
  fallback manual-calculation mode (optionally gated behind holding Shift,
  since synchronous folder-size calculation is slow).
- README explicitly calls out an OS-version-dependent Explorer UI change it
  works around: *"Starting with update KB5101684, Windows 11 is gradually
  switching to MB/GB for large files"* — i.e. Microsoft itself is in the
  process of shipping (a subset of) this mod's behavior natively, a real
  "shrinking relevance" / future-conflict risk worth tracking.
- **Known open issues**:
  - #6558 "slows down File Explorer rename feature" (perf regression report)
  - #3404 "Failed hooking Windows Storage symbols, in win10 22h2 19045.6466"
    (direct symbol-hooking failure on a specific Windows 10 build)

---

## 5. License summary — what MacFlow can/cannot reuse

| Mod | Declared license (in-file comment) | `@license` metadata field | Source |
|---|---|---|---|
| windows-11-taskbar-styler | GNU GPL v3.0 | not set in metadata block (plain-text notice below it) | mod source, line ~24-26 of each file |
| taskbar-icon-size | GNU GPL v3.0 | not set | same pattern |
| windows-11-start-menu-styler | GNU GPL v3.0 | not set | same pattern |
| windows-11-notification-center-styler | GNU GPL v3.0 | not set | same pattern |
| taskbar-clock-customization | GNU GPL v3.0 | not set | same pattern |
| explorer-details-better-file-sizes | GNU GPL v3.0 | not set | same pattern |

**Important nuance**: none of the six files actually populate the `@license`
metadata key inside the `==WindhawkMod==` block (the field the repo's own
validator checks against SPDX and which, if empty, defaults new submissions to
MIT per the README). Instead, all six declare GPLv3 via a **plain code comment
immediately following the metadata block** ("Source code is published under
The GNU General Public License v3.0."). This is a manually-asserted license
notice, not a machine-checked metadata field — so:

1. **Legally**, these six mods are GPLv3-licensed works by m417z. GPLv3 is a
   strong copyleft license. If MacFlow copies or adapts **any non-trivial code**
   from these six files (e.g. the shared XAML-diagnostics/`VisualTreeWatcher`
   engine, the `CFSFolder` symbol-hook list, the `WinVersion`/ExplorerPatcher
   detection logic), the resulting MacFlow mod **must also be released under
   GPLv3** (or a GPLv3-compatible/later license, if explicitly permitted) and
   must carry attribution to the original author and a copy of/link to the
   GPLv3 text. This is a real constraint, not a formality — GPLv3's copyleft
   is viral to derivative source code, and `windhawk-mods`' own submission
   rules (Section 3) additionally make the *original author* (by GitHub
   identity) the only party who can publish updates to a mod with that id —
   so MacFlow cannot "fork and re-publish under the same id" even if the
   license permitted it.
2. If MacFlow instead writes **original, independent implementations** that
   happen to target the same `@include` processes and even the same private
   WinRT classes/symbols (because those are just facts about Windows' own
   internals, not expressive code), that is almost certainly fine, but anyone
   on the MacFlow team who has read this GPLv3 source should be careful that
   new MacFlow code is an independent expression, not a close paraphrase —
   ordinary clean-room / avoid-copy-paste discipline.
3. MacFlow's own new mods, if ever submitted upstream to
   `ramensoftware/windhawk-mods`, need a real `@license` metadata value (a
   valid SPDX id) or they default to MIT. MacFlow should pick one license and
   apply it consistently; mixing MIT (MacFlow's own code) with
   GPLv3-contaminated code in the same file is not legally sound.

I could not verify a repo-wide default license for `ramensoftware/windhawk-mods`
itself (i.e. a root `LICENSE` file covering the repo's own tooling, as opposed
to individual mods) — I did not fetch one and do not assert its contents here.

---

## 6. Compatibility-risk takeaways for MacFlow's design

- **Styling backbone is fragile and shared.** All three "Styler" mods
  (Taskbar, Start Menu, Notification Center) hijack `InitializeXamlDiagnosticsEx`
  and ride `IVisualTreeService3`/`IXamlDiagnosticsTestHooks` — private, Microsoft
  XAML-debugging COM interfaces not declared in any public SDK header
  (confirmed by the mods' own comments: *"xamlom.h does not declare it"*). Only
  one XAML-diagnostics consumer is safe per process at a time, which is why the
  taskbar styler itself ships `kAlert`/`kBlock`/`kAllow` handling for *other*
  tools doing the same thing — a MacFlow suite combining several of these
  mod-style components in one process needs the same single-consumer
  discipline, or it will silently corrupt or fight itself.
- **Symbol-hook brittleness across builds is the norm, not the exception.**
  Concrete, cited evidence: `taskbar-icon-size.wh.cpp` marks multiple
  `SYMBOL_HOOK` entries `optional` with comments like "Missing in older Windows
  11 versions" / "inlined sometimes"; `explorer-details-better-file-sizes.wh.cpp`
  has a live, open GitHub issue (#3404) of a hooking failure on a specific
  Windows 10 build; the taskbar styler has three open issues this year alone
  about styling silently breaking on new Insider/production builds (26100,
  29648, and general Insider updates). `WindhawkUtils::HookSymbols` works by
  downloading/matching **demangled C++ symbol names** from Microsoft's symbol
  server — these names are not an API contract and change across both Windows
  builds **and** 32-bit/64-bit compilations (the `__cdecl` vs `__stdcall`,
  `__unaligned`-qualifier difference shown verbatim in Section 4.6).

## 7. Addendum — a seventh relevant mod found on the test machine

While inspecting the local Windhawk installation (`C:\ProgramData\Windhawk\ModsSource\`)
for Phase C/D prep, four mods were already present locally, one of which is
outside the original six-mod research scope and directly overlaps MacFlow
Explorer's design space: **Windows 11 File Explorer Styler**
(`windows-11-file-explorer-styler.wh.cpp`, v1.7, author m417z, same GPLv3
notice, `@include explorer.exe`, `@compilerOptions -lcomctl32 -ld2d1 -ldwmapi
-lgdi32 -lmsimg32 -lole32 -loleaut32 -lruntimeobject -lshlwapi -luxtheme`).
Its own readme explicitly cross-links the Taskbar/Start Menu/Notification
Center stylers, confirming it is the fourth member of the same
theme-driven styler family documented in Section 4 (same author, same
"themes contributed by others" model). It was not independently fully
read line-by-line (budget), but its metadata and linked `-ld2d1`/`-ldwmapi`
compiler options are consistent with the same class of technique as the other
stylers plus Direct2D/DWM for its own rendering. MacFlow's design (see
`DESIGN.md`) treats this as an additional "depend on, don't duplicate"
target for MacFlow Explorer's deep per-element theming, alongside the three
mods already covered above.

**Most important single compatibility risk for MacFlow:** any mod that styles
the taskbar/Start menu/notification center via the XAML-diagnostics +
symbol-hook approach used by all five Explorer-targeting mods here is
inherently coupled to the exact internal layout of Microsoft's closed-source
WinUI shell code for the installed Windows 11 build, and Microsoft has broken
it repeatedly and without warning across 23H2→24H2 and even inside 24H2
Insider flights (cited issues #6546, #5273, #4250, #6717, #6238). MacFlow
should budget ongoing maintenance for per-build breakage, ship conservative
`optional`-style fallbacks for every hook, and explicitly test/guard against
ExplorerPatcher co-installation, since the most mature of these mods
(taskbar-clock-customization) already treats ExplorerPatcher as a
first-class, separately-hooked code path rather than an edge case.
