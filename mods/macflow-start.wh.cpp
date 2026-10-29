// ==WindhawkMod==
// @id              macflow-start
// @name            MacFlow Start
// @description     Rounded, translucent, accent-bordered Start menu window chrome, macOS launchpad feel
// @version         1.0.0
// @author          Leonardo Galli
// @github          https://github.com/Leo-Galli
// @homepage        https://github.com/Leo-Galli/macflow-windhawk
// @include         StartMenuExperienceHost.exe
// @architecture    x86-64
// @compilerOptions -ldwmapi -luser32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# MacFlow Start

Gives the Windows 11 Start menu rounded corners, a translucent Mica/Acrylic
backdrop, and an accent-colored border, for a cleaner, macOS launchpad-like
feel. Part of the MacFlow suite
(https://github.com/Leo-Galli/macflow-windhawk).

This mod only applies window-level DWM attributes to the Start menu's
top-level window. It does not change the pinned/all-apps grid layout,
spacing, or icons - for centered launchpad-style layout and spacing control,
pair this mod with **Windows 11 Start Menu Styler** and configure its own
built-in theme/style settings directly. MacFlow does not ship a preset for
that mod in v1.0.0. Icons, pinned tiles, and the apps list are never
modified by this mod.

## Known limitation

The Start menu is hosted in a generic window class
(`Windows.UI.Core.CoreWindow`) that WinRT/WinUI surfaces share broadly. As a
defensive measure against a future Windows build hosting another small
`CoreWindow` surface in the same process, this mod only styles windows taller
than roughly 400 effective pixels, which comfortably covers the real Start
menu panel.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- startCornerStyle: round
  $name: Corner style
  $description: >-
    DWM only exposes a small/round/off preset, not an arbitrary pixel radius -
    this is the closest native approximation available for window chrome.
  $options:
  - round: Round
  - roundsmall: Round (small)
  - square: Square (no rounding)
- startBackdrop: mica
  $name: Backdrop material
  $options:
  - mica: Mica
  - acrylic: Acrylic
  - none: None (solid system color)
- startAccentBorder: true
  $name: Accent-colored border
  $description: >-
    Outline the Start menu with the current Windows accent color.
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
    DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_MAINWINDOW;
    bool accentBorder = true;
};

Settings g_settings;
std::mutex g_trackedWindowsMutex;
std::vector<HWND> g_trackedWindows;

using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t g_origCreateWindowExW;

constexpr WCHAR kCoreWindowClass[] = L"Windows.UI.Core.CoreWindow";
constexpr int kMinHeightDip = 400;

bool IsClassNameMatch(LPCWSTR lpClassName) {
    if (reinterpret_cast<ULONG_PTR>(lpClassName) <= 0xFFFF) {
        return false;
    }
    return _wcsicmp(lpClassName, kCoreWindowClass) == 0;
}

void ApplyStartVisuals(HWND hwnd) {
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

LRESULT CALLBACK StartSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                    LPARAM lParam, DWORD_PTR /*dwRefData*/) {
    switch (uMsg) {
        case WM_WINDOWPOSCHANGED: {
            RECT rect{};
            if (GetWindowRect(hWnd, &rect)) {
                int heightDip = EffectiveHeightDip(hWnd, rect.bottom - rect.top);
                if (heightDip >= kMinHeightDip) {
                    ApplyStartVisuals(hWnd);
                }
            }
            break;
        }
        case WM_NCDESTROY: {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd,
                                                              StartSubclassProc);
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

    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, StartSubclassProc,
                                                       0)) {
        std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
        g_trackedWindows.push_back(hwnd);
    }

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
    if (_wcsicmp(value, L"acrylic") == 0) {
        return DWMSBT_TRANSIENTWINDOW;
    }
    return DWMSBT_MAINWINDOW;
}

void LoadSettings() {
    PCWSTR cornerStyle = Wh_GetStringSetting(L"startCornerStyle");
    g_settings.cornerPreference = ParseCornerStyle(cornerStyle);
    Wh_FreeStringSetting(cornerStyle);

    PCWSTR backdrop = Wh_GetStringSetting(L"startBackdrop");
    g_settings.backdropType = ParseBackdrop(backdrop);
    Wh_FreeStringSetting(backdrop);

    g_settings.accentBorder = Wh_GetIntSetting(L"startAccentBorder") != 0;
}

}  // namespace

BOOL Wh_ModInit() {
    LoadSettings();

    Wh_SetFunctionHook(reinterpret_cast<void*>(CreateWindowExW),
                        reinterpret_cast<void*>(CreateWindowExW_Hook),
                        reinterpret_cast<void**>(&g_origCreateWindowExW));

    return TRUE;
}

void Wh_ModSettingsChanged() {
    LoadSettings();

    std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
    for (HWND hwnd : g_trackedWindows) {
        if (IsWindow(hwnd)) {
            ApplyStartVisuals(hwnd);
        }
    }
}

void Wh_ModUninit() {
    std::lock_guard<std::mutex> lock(g_trackedWindowsMutex);
    for (HWND hwnd : g_trackedWindows) {
        if (IsWindow(hwnd)) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hwnd,
                                                              StartSubclassProc);

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
