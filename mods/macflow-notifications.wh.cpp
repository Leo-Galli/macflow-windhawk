// ==WindhawkMod==
// @id              macflow-notifications
// @name            MacFlow Notifications
// @description     Rounded, translucent, accent-bordered Notification Center and Quick Settings, macOS Control Center style
// @version         1.0.0
// @author          Leonardo Galli
// @github          https://github.com/Leo-Galli
// @homepage        https://github.com/Leo-Galli/macflow-windhawk
// @include         ShellExperienceHost.exe
// @include         ShellHost.exe
// @architecture    x86-64
// @compilerOptions -ldwmapi -lcomctl32 -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# MacFlow Notifications

Gives the Windows 11 Notification Center and Quick Settings flyouts rounded
corners, a translucent Mica/Acrylic backdrop, and an accent-colored border,
for a macOS Control Center feel. Part of the MacFlow suite
(https://github.com/Leo-Galli/macflow-windhawk).

This mod only applies window-level DWM attributes (corner radius, backdrop
material, border color) to the flyout's top-level window. It does not edit
the internal layout, spacing, or icons of individual notification/quick
settings cards - for that level of control, pair this mod with **Windows 11
Notification Center Styler** and configure its own built-in theme/style
settings directly. MacFlow does not ship a preset for that mod in v1.0.0.

## Known limitation

On Windows 11 23H2 and earlier, the Notification Center / Quick Settings
flyout and toast notification popups share the same generic window class
(`Windows.UI.Core.CoreWindow`) inside `ShellExperienceHost.exe`, with no
public, documented way to tell them apart by class name alone. This mod
applies a window-height heuristic (only windows taller than roughly 400
effective pixels are treated as Notification Center / Quick Settings) to
avoid styling toast popups, which are reliably much shorter. In rare cases
(e.g. an unusually tall stack of toasts) this heuristic could misfire; disable
the mod if you notice toast popups being styled unexpectedly. On Windows 11
24H2 and later, both surfaces use a dedicated window class
(`ControlCenterWindow` in `ShellHost.exe`) that is not shared with toasts, so
this heuristic is not needed there.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- notifCornerStyle: round
  $name: Corner style
  $description: >-
    DWM only exposes a small/round/off preset, not an arbitrary pixel radius -
    this is the closest native approximation available for window chrome.
  $options:
  - round: Round
  - roundsmall: Round (small)
  - square: Square (no rounding)
- notifBackdrop: acrylic
  $name: Backdrop material
  $options:
  - mica: Mica
  - acrylic: Acrylic
  - none: None (solid system color)
- notifAccentBorder: true
  $name: Accent-colored border
  $description: >-
    Outline the flyout with the current Windows accent color.
*/
// ==/WindhawkModSettings==

#include <dwmapi.h>
#include <windhawk_utils.h>
#include <windows.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

struct Settings {
    DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
    DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_TRANSIENTWINDOW;
    bool accentBorder = true;
};

Settings g_settings;
std::mutex g_trackedWindowsMutex;
std::vector<HWND> g_trackedWindows;

using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t g_origCreateWindowExW;

constexpr WCHAR kCoreWindowClass[] = L"Windows.UI.Core.CoreWindow";
constexpr WCHAR kControlCenterClass[] = L"ControlCenterWindow";

// Windows.UI.Core.CoreWindow is shared with toast popups on builds before
// 24H2; ControlCenterWindow (24H2+) is not shared with toasts, but the
// height heuristic is applied for both for simplicity and defense in depth.
constexpr int kMinHeightDip = 400;

bool IsClassNameMatch(LPCWSTR lpClassName) {
    // A class name passed as an atom (not a string pointer) has its upper 16
    // bits zero; never dereference it as a string in that case.
    if (reinterpret_cast<ULONG_PTR>(lpClassName) <= 0xFFFF) {
        return false;
    }
    return _wcsicmp(lpClassName, kCoreWindowClass) == 0 ||
           _wcsicmp(lpClassName, kControlCenterClass) == 0;
}

void ApplyFlyoutVisuals(HWND hwnd) {
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                           &g_settings.cornerPreference,
                           sizeof(g_settings.cornerPreference));
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                           &g_settings.backdropType,
                           sizeof(g_settings.backdropType));

    if (g_settings.accentBorder) {
        DWORD argbColorization = 0;
        BOOL opaqueBlend = FALSE;
        if (SUCCEEDED(
                DwmGetColorizationColor(&argbColorization, &opaqueBlend))) {
            BYTE r = static_cast<BYTE>((argbColorization >> 16) & 0xFF);
            BYTE g = static_cast<BYTE>((argbColorization >> 8) & 0xFF);
            BYTE b = static_cast<BYTE>(argbColorization & 0xFF);
            COLORREF accent = RGB(r, g, b);
            DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &accent,
                                   sizeof(accent));
        }
    } else {
        COLORREF def = DWMWA_COLOR_DEFAULT;
        DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &def, sizeof(def));
    }
}

int EffectiveHeightDip(HWND hwnd, int pixelHeight) {
    UINT dpi = GetDpiForWindow(hwnd);
    if (dpi == 0) {
        dpi = 96;
    }
    return MulDiv(pixelHeight, 96, static_cast<int>(dpi));
}

LRESULT CALLBACK FlyoutSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                     LPARAM lParam, DWORD_PTR /*dwRefData*/) {
    switch (uMsg) {
        case WM_WINDOWPOSCHANGED: {
            RECT rect{};
            if (GetWindowRect(hWnd, &rect)) {
                int heightDip = EffectiveHeightDip(hWnd, rect.bottom - rect.top);
                if (heightDip >= kMinHeightDip) {
                    ApplyFlyoutVisuals(hWnd);
                }
            }
            break;
        }
        case WM_NCDESTROY: {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd,
                                                              FlyoutSubclassProc);
            std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
            g_trackedWindows.erase(
                std::remove(g_trackedWindows.begin(), g_trackedWindows.end(),
                            hWnd),
                g_trackedWindows.end());
            break;
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void TrackWindow(HWND hwnd) {
    {
        std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
        if (std::find(g_trackedWindows.begin(), g_trackedWindows.end(),
                      hwnd) != g_trackedWindows.end()) {
            return;
        }
    }

    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, FlyoutSubclassProc,
                                                       0)) {
        {
            std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
            g_trackedWindows.push_back(hwnd);
        }
        // The window may already be shown and sized by the time the mod is
        // enabled (e.g. a Notification Center/Quick Settings host window
        // created before this mod loaded) - evaluate it immediately instead
        // of waiting for a future WM_WINDOWPOSCHANGED that may never come.
        RECT rect{};
        if (GetWindowRect(hwnd, &rect)) {
            int heightDip = EffectiveHeightDip(hwnd, rect.bottom - rect.top);
            if (heightDip >= kMinHeightDip) {
                ApplyFlyoutVisuals(hwnd);
            }
        }
    }
}

BOOL CALLBACK EnumExistingWindowsProc(HWND hwnd, LPARAM /*lParam*/) {
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    if (processId != GetCurrentProcessId()) {
        return TRUE;
    }

    WCHAR className[64];
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) > 0 &&
        IsClassNameMatch(className)) {
        TrackWindow(hwnd);
    }

    return TRUE;
}

HWND WINAPI CreateWindowExW_Hook(DWORD dwExStyle, LPCWSTR lpClassName,
                                  LPCWSTR lpWindowName, DWORD dwStyle, int X,
                                  int Y, int nWidth, int nHeight,
                                  HWND hWndParent, HMENU hMenu,
                                  HINSTANCE hInstance, LPVOID lpParam) {
    HWND hwnd = g_origCreateWindowExW(dwExStyle, lpClassName, lpWindowName,
                                       dwStyle, X, Y, nWidth, nHeight,
                                       hWndParent, hMenu, hInstance, lpParam);
    if (!hwnd || !IsClassNameMatch(lpClassName)) {
        return hwnd;
    }

    TrackWindow(hwnd);

    return hwnd;
}

DWM_WINDOW_CORNER_PREFERENCE ParseCornerStyle(PCWSTR value) {
    if (_wcsicmp(value, L"square") == 0) {
        return DWMWCP_DONOTROUND;
    }
    if (_wcsicmp(value, L"roundsmall") == 0) {
        return DWMWCP_ROUNDSMALL;
    }
    return DWMWCP_ROUND;
}

DWM_SYSTEMBACKDROP_TYPE ParseBackdrop(PCWSTR value) {
    if (_wcsicmp(value, L"none") == 0) {
        return DWMSBT_NONE;
    }
    if (_wcsicmp(value, L"mica") == 0) {
        return DWMSBT_MAINWINDOW;
    }
    return DWMSBT_TRANSIENTWINDOW;
}

void LoadSettings() {
    PCWSTR cornerStyle = Wh_GetStringSetting(L"notifCornerStyle");
    g_settings.cornerPreference = ParseCornerStyle(cornerStyle);
    Wh_FreeStringSetting(cornerStyle);

    PCWSTR backdrop = Wh_GetStringSetting(L"notifBackdrop");
    g_settings.backdropType = ParseBackdrop(backdrop);
    Wh_FreeStringSetting(backdrop);

    g_settings.accentBorder = Wh_GetIntSetting(L"notifAccentBorder") != 0;
}

}  // namespace

BOOL Wh_ModInit() {
    LoadSettings();

    Wh_SetFunctionHook(reinterpret_cast<void*>(CreateWindowExW),
                        reinterpret_cast<void*>(CreateWindowExW_Hook),
                        reinterpret_cast<void**>(&g_origCreateWindowExW));

    // The flyout host window is typically created once, well before this
    // mod is enabled, and then only shown/hidden - the CreateWindowExW hook
    // alone would never see it. Scan for it once at startup too.
    EnumWindows(EnumExistingWindowsProc, 0);

    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();

    std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
    for (HWND hwnd : g_trackedWindows) {
        if (IsWindow(hwnd)) {
            ApplyFlyoutVisuals(hwnd);
        }
    }
}

void Wh_ModUninit() {
    std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
    for (HWND hwnd : g_trackedWindows) {
        if (IsWindow(hwnd)) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(
                hwnd, FlyoutSubclassProc);

            DWM_WINDOW_CORNER_PREFERENCE defaultCorner = DWMWCP_DEFAULT;
            DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                   &defaultCorner, sizeof(defaultCorner));

            DWM_SYSTEMBACKDROP_TYPE defaultBackdrop = DWMSBT_AUTO;
            DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                   &defaultBackdrop, sizeof(defaultBackdrop));

            COLORREF defaultBorder = DWMWA_COLOR_DEFAULT;
            DwmSetWindowAttribute(hwnd, DWMWA_BORDER_COLOR, &defaultBorder,
                                   sizeof(defaultBorder));
        }
    }
    g_trackedWindows.clear();
}
