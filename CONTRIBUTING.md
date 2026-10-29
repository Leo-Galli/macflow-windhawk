# Contributing to MacFlow

Thanks for your interest in MacFlow. This repository hosts the source for a
small suite of [Windhawk](https://windhawk.net/) mods that give Windows 11 a
macOS-inspired look without changing any icons.

## Before you start

- Read `docs/RESEARCH.md` and `docs/DESIGN.md` first. They explain why each
  mod is built the way it is, including the architecture decision to use
  only public, documented Windows APIs (DWM window attributes, UI
  Automation, Win32 window subclassing) rather than the private
  XAML-diagnostics engine used by the Taskbar/Start Menu/Notification
  Center/File Explorer "Styler" mods. Changes that would require that
  private engine are out of scope for this repository - raise an issue
  first if you think an exception is warranted.
- Every mod is a single `.wh.cpp` file under `mods/`, following the official
  [Windhawk mod source format](https://github.com/ramensoftware/windhawk/wiki/Creating-a-new-mod).
- Mod IDs use the `macflow-` prefix and, once published, cannot be renamed.

## Code requirements

- C++17 or newer, matching the Windhawk engine's compiler (clang, mingw-w64
  target, currently C++23 mode - see `docs/RESEARCH.md` Section 1).
- No invented APIs: every Windhawk or Win32 function used must exist in the
  real headers shipped with the Windhawk compiler
  (`C:\Program Files\Windhawk\Compiler\include` on a Windhawk install) or in
  official Microsoft documentation. If you're not sure a function exists,
  check the header before sending a PR.
- No icon changes, ever. This is a hard rule for the whole suite.
- Every hook must be guarded and fail safe - a missing window, control, or
  symbol should log via `Wh_Log` and skip that feature, not crash
  `explorer.exe` or any other target process.
- `Wh_ModUninit` must fully undo whatever the mod changed.
- No polling loops. Use window messages, `SetWinEventHook`, UI Automation
  events, or DWM/registry change notifications instead.
- English only, no emoji, and comments should explain non-obvious *why*, not
  restate *what* the code already says.

## Testing

Before submitting a change:

1. Compile the mod (the Windhawk app's own "Compile mod" step, or the
   command-line compiler at `C:\Program Files\Windhawk\Compiler\Compiler\bin\clang++.exe`
   using the flags in `compile_flags.txt`) with zero warnings.
2. Enable/disable the mod and change its settings repeatedly; confirm no
   crash and that disabling restores the original look.
3. Test in both light and dark mode if your change touches theming.
4. Update `docs/TESTING.md` with real results. Do not write a result you
   have not personally observed.

## Licensing

This repository is MIT-licensed (see `LICENSE`). Do not submit code copied
from GPLv3 mods (including the Taskbar/Start Menu/Notification Center/File
Explorer "Styler" mods referenced in `docs/RESEARCH.md`) - GPLv3 is a strong
copyleft license incompatible with MIT distribution. If you believe a
GPLv3-licensed technique is genuinely necessary, open an issue to discuss
licensing implications before writing code.

## Submitting changes

Open a pull request against `main`. Describe what changed, why, and what you
tested. For new settings, include the exact `==WindhawkModSettings==` block
you added.
