// ==WindhawkMod==
// @id              macflow-dock
// @name            MacFlow Dock
// @description     Turns the Windows 11 taskbar into a floating, translucent, rounded dock, macOS-style
// @version         1.0.0
// @author          Leonardo Galli
// @github          https://github.com/Leo-Galli
// @homepage        https://github.com/Leo-Galli/macflow-windhawk
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -ldwmapi -lshell32 -luser32 -lole32 -loleaut32 -luuid -lgdi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# MacFlow Dock

Turns the Windows 11 taskbar into a floating, rounded, translucent dock with a
configurable margin from the screen edges, optional auto-hide, and an
optional hover highlight, for a macOS dock feel. Part of the MacFlow suite
(https://github.com/Leo-Galli/macflow-windhawk). Icons are never modified.

## Known limitations

- The floating-margin effect is only applied when the taskbar is docked to
  the **bottom** edge of the screen (the default and most common
  configuration). Left/right/top-docked taskbars are left untouched rather
  than risk an incorrectly computed inset on an orientation this mod has not
  been visually verified against.
- MacFlow Dock insets the taskbar window by a margin on all sides; it does
  not shrink the bar to exactly the width of its pinned/running content
  (true macOS-style content-hugging). That would require controlling the
  taskbar's internal WinUI layout sizing, the same surface used by
  **Taskbar height and icon size** - pair that mod with MacFlow Dock if you
  want a narrower bar in addition to the floating-margin look.
- Hover magnify (off by default) shows an accent-colored highlight behind
  the hovered icon; it does not resize the real taskbar icons themselves,
  which would require the private XAML diagnostics engine MacFlow
  intentionally does not reimplement.
- **Running-app indicator dots and a pinned/running separator were evaluated
  and are not implemented.** Windows 11 does not expose any public,
  documented way to tell whether a taskbar button represents a running app
  or a pinned-but-not-running one: the real signal
  (`ITaskListButton::get_IsRunning`) is a private WinRT property only
  reachable by symbol-hooking the taskbar's internal implementation, and UI
  Automation only exposes this as locale-dependent text buried in the
  button's accessible name (e.g. "App - 2 running windows"), which is not a
  reliable structured signal. Since MacFlow's architecture intentionally
  avoids private internal hooking (see the MacFlow repository's RESEARCH.md
  and DESIGN.md), this feature from the original design brief is dropped
  rather than shipped on an unverified heuristic.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- dockMarginPx: 8
  $name: Dock margin (px)
  $description: >-
    Distance between the floating dock and the screen edges. Only applied
    when the taskbar is docked to the bottom of the screen.
- dockAutoHide: false
  $name: Auto-hide
  $description: >-
    Automatically hide the dock until the mouse touches the screen edge,
    using the same mechanism as Windows' own taskbar auto-hide setting.
- dockCornerStyle: round
  $name: Corner style
  $options:
  - round: Round
  - roundsmall: Round (small)
  - square: Square (no rounding)
- dockBackdrop: mica
  $name: Backdrop material
  $options:
  - mica: Mica
  - acrylic: Acrylic
  - none: None (solid system color)
- dockMagnifyEnabled: false
  $name: Hover highlight (experimental, off by default for performance)
  $description: >-
    Shows an accent-colored highlight behind the taskbar icon under the
    mouse cursor.
- dockMagnifyScale: 130
  $name: Hover highlight scale (percent)
*/
// ==/WindhawkModSettings==

#include <dwmapi.h>
#include <shellapi.h>
#include <uiautomation.h>
#include <windhawk_utils.h>
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

void UpdateHoverHighlight(HWND trayHwnd, POINT screenPt);
bool g_comInitializedByUs = false;

struct Settings {
    int marginPx = 8;
    bool autoHide = false;
    DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
    DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_MAINWINDOW;
    bool magnifyEnabled = false;
    float magnifyScale = 1.3f;
};

Settings g_settings;
std::mutex g_trayWindowsMutex;
std::vector<HWND> g_trayWindows;
bool g_autoHideAppliedByUs = false;

IUIAutomation* g_uiAutomation = nullptr;
HWND g_highlightOverlay = nullptr;
bool g_highlightVisible = false;

using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t g_origCreateWindowExW;

constexpr WCHAR kPrimaryTrayClass[] = L"Shell_TrayWnd";
constexpr WCHAR kSecondaryTrayClass[] = L"Shell_SecondaryTrayWnd";
constexpr WCHAR kHighlightOverlayClass[] = L"MacFlowDockHighlightOverlay";

bool IsTrayClassName(LPCWSTR lpClassName) {
    if (reinterpret_cast<ULONG_PTR>(lpClassName) <= 0xFFFF) {
        return false;
    }
    return _wcsicmp(lpClassName, kPrimaryTrayClass) == 0 ||
           _wcsicmp(lpClassName, kSecondaryTrayClass) == 0;
}

void ApplyDockChrome(HWND hwnd) {
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                           &g_settings.cornerPreference,
                           sizeof(g_settings.cornerPreference));
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                           &g_settings.backdropType,
                           sizeof(g_settings.backdropType));
}

void SetAutoHideState(HWND trayHwnd, bool enable) {
    APPBARDATA abd{};
    abd.cbSize = sizeof(abd);
    abd.hWnd = trayHwnd;
    abd.lParam = enable ? ABS_AUTOHIDE : 0;
    SHAppBarMessage(ABM_SETSTATE, &abd);
}

LRESULT CALLBACK TraySubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam, DWORD_PTR /*dwRefData*/) {
    switch (uMsg) {
        case WM_WINDOWPOSCHANGING: {
            auto* pos = reinterpret_cast<WINDOWPOS*>(lParam);
            APPBARDATA abd{};
            abd.cbSize = sizeof(abd);
            abd.hWnd = hWnd;
            if (SHAppBarMessage(ABM_GETTASKBARPOS, &abd) &&
                abd.uEdge == ABE_BOTTOM && g_settings.marginPx > 0) {
                UINT dpi = GetDpiForWindow(hWnd);
                if (dpi == 0) {
                    dpi = 96;
                }
                int margin = MulDiv(g_settings.marginPx, static_cast<int>(dpi), 96);

                pos->x += margin;
                pos->cx = std::max(0, pos->cx - margin * 2);
                pos->y -= margin;
                pos->flags &= ~(SWP_NOMOVE | SWP_NOSIZE);
            }
            break;
        }
        case WM_WINDOWPOSCHANGED: {
            ApplyDockChrome(hWnd);
            break;
        }
        case WM_MOUSEMOVE: {
            if (g_settings.magnifyEnabled) {
                POINT screenPt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                ClientToScreen(hWnd, &screenPt);
                UpdateHoverHighlight(hWnd, screenPt);
            }
            break;
        }
        case WM_NCDESTROY: {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(hWnd,
                                                              TraySubclassProc);
            std::lock_guard<std::mutex> lock(g_trayWindowsMutex);
            g_trayWindows.erase(
                std::remove(g_trayWindows.begin(), g_trayWindows.end(), hWnd),
                g_trayWindows.end());
            break;
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void TrackTrayWindow(HWND hwnd) {
    if (WindhawkUtils::SetWindowSubclassFromAnyThread(hwnd, TraySubclassProc,
                                                       0)) {
        std::lock_guard<std::mutex> lock(g_trayWindowsMutex);
        g_trayWindows.push_back(hwnd);
    }
}

BOOL CALLBACK EnumTrayWindowsProc(HWND hwnd, LPARAM /*lParam*/) {
    WCHAR className[64];
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) > 0 &&
        IsTrayClassName(className)) {
        TrackTrayWindow(hwnd);
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
    if (hwnd && IsTrayClassName(lpClassName)) {
        TrackTrayWindow(hwnd);
    }
    return hwnd;
}

// --- Hover highlight overlay (experimental, off by default) ---

LRESULT CALLBACK HighlightOverlayProc(HWND hWnd, UINT uMsg, WPARAM wParam,
                                       LPARAM lParam) {
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);

        DWORD argb = 0;
        BOOL opaque = FALSE;
        COLORREF accent = RGB(0, 120, 215);
        if (SUCCEEDED(DwmGetColorizationColor(&argb, &opaque))) {
            accent = RGB((argb >> 16) & 0xFF, (argb >> 8) & 0xFF, argb & 0xFF);
        }

        HBRUSH brush = CreateSolidBrush(accent);
        HRGN region = CreateRoundRectRgn(rc.left, rc.top, rc.right + 1,
                                          rc.bottom + 1, 16, 16);
        FillRgn(hdc, region, brush);
        DeleteObject(region);
        DeleteObject(brush);

        EndPaint(hWnd, &ps);
        return 0;
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void EnsureHighlightOverlay() {
    if (g_highlightOverlay) {
        return;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = HighlightOverlayProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = kHighlightOverlayClass;
    RegisterClassExW(&wc);

    g_highlightOverlay = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kHighlightOverlayClass, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);

    if (g_highlightOverlay) {
        SetLayeredWindowAttributes(g_highlightOverlay, 0, 160, LWA_ALPHA);
    }
}

void UpdateHoverHighlight(HWND /*trayHwnd*/, POINT screenPt) {
    if (!g_settings.magnifyEnabled || !g_uiAutomation) {
        return;
    }

    EnsureHighlightOverlay();
    if (!g_highlightOverlay) {
        return;
    }

    IUIAutomationElement* element = nullptr;
    if (FAILED(g_uiAutomation->ElementFromPoint(screenPt, &element)) ||
        !element) {
        if (g_highlightVisible) {
            AnimateWindow(g_highlightOverlay, 180, AW_HIDE | AW_BLEND);
            g_highlightVisible = false;
        }
        return;
    }

    RECT bounds{};
    HRESULT hr = element->get_CurrentBoundingRectangle(&bounds);
    element->Release();

    if (FAILED(hr) || (bounds.right - bounds.left) <= 0 ||
        (bounds.bottom - bounds.top) <= 0) {
        if (g_highlightVisible) {
            AnimateWindow(g_highlightOverlay, 180, AW_HIDE | AW_BLEND);
            g_highlightVisible = false;
        }
        return;
    }

    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    int scaledWidth = static_cast<int>(width * g_settings.magnifyScale);
    int scaledHeight = static_cast<int>(height * g_settings.magnifyScale);
    int centerX = bounds.left + width / 2;
    int centerY = bounds.top + height / 2;

    SetWindowPos(g_highlightOverlay, HWND_TOPMOST,
                 centerX - scaledWidth / 2, centerY - scaledHeight / 2,
                 scaledWidth, scaledHeight, SWP_NOACTIVATE);
    InvalidateRect(g_highlightOverlay, nullptr, FALSE);

    if (!g_highlightVisible) {
        ShowWindow(g_highlightOverlay, SW_SHOWNOACTIVATE);
        AnimateWindow(g_highlightOverlay, 180, AW_ACTIVATE | AW_BLEND);
        g_highlightVisible = true;
    }
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
    g_settings.marginPx = Wh_GetIntSetting(L"dockMarginPx");
    g_settings.autoHide = Wh_GetIntSetting(L"dockAutoHide") != 0;

    PCWSTR cornerStyle = Wh_GetStringSetting(L"dockCornerStyle");
    g_settings.cornerPreference = ParseCornerStyle(cornerStyle);
    Wh_FreeStringSetting(cornerStyle);

    PCWSTR backdrop = Wh_GetStringSetting(L"dockBackdrop");
    g_settings.backdropType = ParseBackdrop(backdrop);
    Wh_FreeStringSetting(backdrop);

    g_settings.magnifyEnabled = Wh_GetIntSetting(L"dockMagnifyEnabled") != 0;
    int scalePercent = Wh_GetIntSetting(L"dockMagnifyScale");
    g_settings.magnifyScale = scalePercent > 0 ? scalePercent / 100.0f : 1.3f;
}

void ApplyAutoHideToAllTrays(bool enable) {
    std::lock_guard<std::mutex> lock(g_trayWindowsMutex);
    for (HWND hwnd : g_trayWindows) {
        if (IsWindow(hwnd)) {
            SetAutoHideState(hwnd, enable);
        }
    }
    g_autoHideAppliedByUs = enable;
}

void ForceTrayRelayout() {
    std::lock_guard<std::mutex> lock(g_trayWindowsMutex);
    for (HWND hwnd : g_trayWindows) {
        if (IsWindow(hwnd)) {
            SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                             SWP_FRAMECHANGED);
            ApplyDockChrome(hwnd);
        }
    }
}

}  // namespace

BOOL Wh_ModInit() {
    LoadSettings();

    HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_comInitializedByUs = (comInit == S_OK || comInit == S_FALSE);

    CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                      IID_IUIAutomation,
                      reinterpret_cast<void**>(&g_uiAutomation));

    Wh_SetFunctionHook(reinterpret_cast<void*>(CreateWindowExW),
                        reinterpret_cast<void*>(CreateWindowExW_Hook),
                        reinterpret_cast<void**>(&g_origCreateWindowExW));

    EnumWindows(EnumTrayWindowsProc, 0);

    if (g_settings.autoHide) {
        ApplyAutoHideToAllTrays(true);
    }

    ForceTrayRelayout();

    return TRUE;
}

void Wh_ModSettingsChanged() {
    bool wasAutoHide = g_autoHideAppliedByUs;
    LoadSettings();

    if (g_settings.autoHide != wasAutoHide) {
        ApplyAutoHideToAllTrays(g_settings.autoHide);
    }

    ForceTrayRelayout();
}

void Wh_ModUninit() {
    if (g_autoHideAppliedByUs) {
        ApplyAutoHideToAllTrays(false);
    }

    {
        std::lock_guard<std::mutex> lock(g_trayWindowsMutex);
        for (HWND hwnd : g_trayWindows) {
            if (IsWindow(hwnd)) {
                WindhawkUtils::RemoveWindowSubclassFromAnyThread(
                    hwnd, TraySubclassProc);

                DWM_WINDOW_CORNER_PREFERENCE defaultCorner = DWMWCP_DEFAULT;
                DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                                       &defaultCorner, sizeof(defaultCorner));

                DWM_SYSTEMBACKDROP_TYPE defaultBackdrop = DWMSBT_AUTO;
                DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                       &defaultBackdrop,
                                       sizeof(defaultBackdrop));

                SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                                 SWP_FRAMECHANGED);
            }
        }
        g_trayWindows.clear();
    }

    if (g_highlightOverlay) {
        DestroyWindow(g_highlightOverlay);
        g_highlightOverlay = nullptr;
    }
    UnregisterClassW(kHighlightOverlayClass, GetModuleHandleW(nullptr));

    if (g_uiAutomation) {
        g_uiAutomation->Release();
        g_uiAutomation = nullptr;
    }

    if (g_comInitializedByUs) {
        CoUninitialize();
        g_comInitializedByUs = false;
    }
}
