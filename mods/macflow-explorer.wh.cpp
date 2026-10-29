// ==WindhawkMod==
// @id              macflow-explorer
// @name            MacFlow Explorer
// @description     Rounded window corners, Mica/acrylic backdrop, and an accent-colored rounded selection highlight for File Explorer
// @version         1.0.0
// @author          Leonardo Galli
// @github          https://github.com/Leo-Galli
// @homepage        https://github.com/Leo-Galli/macflow-windhawk
// @include         explorer.exe
// @architecture    x86-64
// @compilerOptions -ldwmapi -lcomctl32 -luxtheme -luser32 -lgdi32
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# MacFlow Explorer

Gives File Explorer folder windows rounded corners, a Mica/Acrylic backdrop,
dark-mode-aware title bar, and an accent-colored rounded outline on the
selected item, for a cleaner macOS-influenced look. Part of the MacFlow suite
(https://github.com/Leo-Galli/macflow-windhawk). File and folder icons are
never modified.

## Known limitations

- **Sidebar translucency**: this mod opts the navigation pane into
  Explorer-style control theming (`SetWindowTheme`), but does not attempt a
  true see-through Mica effect behind the classic `SysTreeView32` control.
  That would require taking over File Explorer's non-client frame handling,
  which this mod does not do, to avoid shipping an unverified effect that
  could render as a flickering or solid black strip instead of translucency.
  For genuine sidebar translucency, use **Windows 11 File Explorer Styler**'s
  "Translucent Explorer11" theme alongside this mod.
- **Rounded selection** draws an accent-colored rounded outline on top of the
  existing native selection highlight; it does not replace the native
  highlight's shape.
- The WinUI3 ribbon command bar (Windows 11 24H2) is not restyled by this
  mod - that requires the private XAML diagnostics engine used by the
  styler-family mods, which MacFlow intentionally does not reimplement (see
  the MacFlow repository's RESEARCH.md and DESIGN.md for why).
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- explorerCornerStyle: round
  $name: Corner style
  $description: >-
    DWM only exposes a small/round/off preset, not an arbitrary pixel radius -
    this is the closest native approximation available for window chrome.
  $options:
  - round: Round
  - roundsmall: Round (small)
  - square: Square (no rounding)
- explorerBackdrop: mica
  $name: Backdrop material
  $options:
  - mica: Mica
  - acrylic: Acrylic
  - none: None (solid system color)
- explorerSidebarTranslucent: true
  $name: Theme the navigation pane as an Explorer control
  $description: >-
    Opts the navigation pane into standard Explorer-style control theming.
    Does not produce true see-through translucency - see the mod readme.
- explorerRoundedSelection: true
  $name: Rounded accent selection outline
  $description: >-
    Draws a 12px-radius accent-colored outline on top of the selected item in
    the navigation pane and folder view.
*/
// ==/WindhawkModSettings==

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <windhawk_utils.h>
#include <windows.h>

#include <algorithm>
#include <mutex>
#include <vector>

namespace {

struct Settings {
    DWM_WINDOW_CORNER_PREFERENCE cornerPreference = DWMWCP_ROUND;
    DWM_SYSTEMBACKDROP_TYPE backdropType = DWMSBT_MAINWINDOW;
    bool sidebarTheming = true;
    bool roundedSelection = true;
};

Settings g_settings;
std::mutex g_stateMutex;

struct ExplorerWindowState {
    HWND mainHwnd = nullptr;
    HWND treeView = nullptr;
    HWND listView = nullptr;
    bool childrenDiscovered = false;
};

std::vector<ExplorerWindowState> g_windows;
// Immediate parents of tracked controls, subclassed once each to observe
// WM_NOTIFY / NM_CUSTOMDRAW. A parent may host both the tree view and the
// list view, so this list is deduplicated independently of g_windows.
std::vector<HWND> g_subclassedNotifyParents;
// Flat set of controls eligible for the rounded selection overlay, checked
// by hwndFrom in the shared notify-parent subclass proc.
std::vector<HWND> g_watchedControls;

using CreateWindowExW_t = decltype(&CreateWindowExW);
CreateWindowExW_t g_origCreateWindowExW;

constexpr WCHAR kCabinetClass[] = L"CabinetWClass";
constexpr WCHAR kExploreClass[] = L"ExploreWClass";

bool IsExplorerTopLevelClass(LPCWSTR lpClassName) {
    if (reinterpret_cast<ULONG_PTR>(lpClassName) <= 0xFFFF) {
        return false;
    }
    return _wcsicmp(lpClassName, kCabinetClass) == 0 ||
           _wcsicmp(lpClassName, kExploreClass) == 0;
}

void ApplyWindowChrome(HWND hwnd) {
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                           &g_settings.cornerPreference,
                           sizeof(g_settings.cornerPreference));
    DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                           &g_settings.backdropType,
                           sizeof(g_settings.backdropType));

    BOOL isDarkMode = FALSE;
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                       L"Software\\Microsoft\\Windows\\CurrentVersion\\"
                       L"Themes\\Personalize",
                       0, KEY_READ, &key) == ERROR_SUCCESS) {
        DWORD value = 1;
        DWORD size = sizeof(value);
        if (RegQueryValueExW(key, L"AppsUseLightTheme", nullptr, nullptr,
                              reinterpret_cast<BYTE*>(&value),
                              &size) == ERROR_SUCCESS) {
            isDarkMode = (value == 0);
        }
        RegCloseKey(key);
    }
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &isDarkMode,
                           sizeof(isDarkMode));
}

COLORREF GetAccentColor() {
    DWORD argb = 0;
    BOOL opaque = FALSE;
    if (SUCCEEDED(DwmGetColorizationColor(&argb, &opaque))) {
        BYTE r = static_cast<BYTE>((argb >> 16) & 0xFF);
        BYTE g = static_cast<BYTE>((argb >> 8) & 0xFF);
        BYTE b = static_cast<BYTE>(argb & 0xFF);
        return RGB(r, g, b);
    }
    return RGB(0, 120, 215);
}

void DrawRoundedSelectionOutline(HDC hdc, const RECT& rc) {
    HPEN pen = CreatePen(PS_SOLID, 2, GetAccentColor());
    HPEN oldPen = static_cast<HPEN>(SelectObject(hdc, pen));
    HBRUSH oldBrush =
        static_cast<HBRUSH>(SelectObject(hdc, GetStockObject(NULL_BRUSH)));

    RoundRect(hdc, rc.left + 1, rc.top + 1, rc.right - 1, rc.bottom - 1, 12,
              12);

    SelectObject(hdc, oldBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(pen);
}

LRESULT CALLBACK NotifyParentSubclassProc(HWND hWnd, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam,
                                           DWORD_PTR /*dwRefData*/) {
    if (uMsg == WM_NOTIFY && g_settings.roundedSelection) {
        NMHDR* hdr = reinterpret_cast<NMHDR*>(lParam);
        if (hdr->code == NM_CUSTOMDRAW) {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            bool watched = std::find(g_watchedControls.begin(),
                                      g_watchedControls.end(),
                                      hdr->hwndFrom) != g_watchedControls.end();
            if (watched) {
                NMCUSTOMDRAW* cd = reinterpret_cast<NMCUSTOMDRAW*>(lParam);
                if (cd->dwDrawStage == CDDS_PREPAINT) {
                    return CDRF_NOTIFYITEMDRAW;
                }
                if (cd->dwDrawStage == CDDS_ITEMPREPAINT) {
                    LRESULT defaultResult =
                        DefSubclassProc(hWnd, uMsg, wParam, lParam);
                    if (cd->uItemState & CDIS_SELECTED) {
                        DrawRoundedSelectionOutline(cd->hdc, cd->rc);
                    }
                    return defaultResult != 0 ? defaultResult
                                               : CDRF_DODEFAULT;
                }
            }
        }
    }

    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

BOOL CALLBACK EnumChildProc(HWND hwnd, LPARAM lParam) {
    auto* state = reinterpret_cast<ExplorerWindowState*>(lParam);
    WCHAR className[64];
    if (GetClassNameW(hwnd, className, ARRAYSIZE(className)) == 0) {
        return TRUE;
    }

    if (!state->treeView && _wcsicmp(className, L"SysTreeView32") == 0) {
        state->treeView = hwnd;
    } else if (!state->listView &&
               _wcsicmp(className, L"SysListView32") == 0) {
        state->listView = hwnd;
    }

    return !(state->treeView && state->listView);
}

void WatchControlForRoundedSelection(HWND control) {
    if (!control) {
        return;
    }

    HWND parent = GetParent(control);
    if (!parent) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (std::find(g_watchedControls.begin(), g_watchedControls.end(),
                  control) == g_watchedControls.end()) {
        g_watchedControls.push_back(control);
    }

    if (std::find(g_subclassedNotifyParents.begin(),
                  g_subclassedNotifyParents.end(),
                  parent) == g_subclassedNotifyParents.end()) {
        if (WindhawkUtils::SetWindowSubclassFromAnyThread(
                parent, NotifyParentSubclassProc, 0)) {
            g_subclassedNotifyParents.push_back(parent);
        }
    }
}

void DiscoverChildren(ExplorerWindowState& state) {
    EnumChildWindows(state.mainHwnd, EnumChildProc,
                      reinterpret_cast<LPARAM>(&state));

    if (state.treeView && g_settings.sidebarTheming) {
        SetWindowTheme(state.treeView, L"Explorer", nullptr);
    }

    WatchControlForRoundedSelection(state.treeView);
    WatchControlForRoundedSelection(state.listView);

    state.childrenDiscovered = state.treeView && state.listView;
}

LRESULT CALLBACK ExplorerMainSubclassProc(HWND hWnd, UINT uMsg,
                                           WPARAM wParam, LPARAM lParam,
                                           DWORD_PTR /*dwRefData*/) {
    switch (uMsg) {
        case WM_WINDOWPOSCHANGED: {
            ApplyWindowChrome(hWnd);

            std::lock_guard<std::mutex> lock(g_stateMutex);
            for (auto& state : g_windows) {
                if (state.mainHwnd == hWnd && !state.childrenDiscovered) {
                    DiscoverChildren(state);
                    break;
                }
            }
            break;
        }
        case WM_NCDESTROY: {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(
                hWnd, ExplorerMainSubclassProc);
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_windows.erase(
                std::remove_if(g_windows.begin(), g_windows.end(),
                                [hWnd](const ExplorerWindowState& s) {
                                    return s.mainHwnd == hWnd;
                                }),
                g_windows.end());
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
    if (!hwnd || !IsExplorerTopLevelClass(lpClassName)) {
        return hwnd;
    }

    if (WindhawkUtils::SetWindowSubclassFromAnyThread(
            hwnd, ExplorerMainSubclassProc, 0)) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        ExplorerWindowState state;
        state.mainHwnd = hwnd;
        g_windows.push_back(state);
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
    PCWSTR cornerStyle = Wh_GetStringSetting(L"explorerCornerStyle");
    g_settings.cornerPreference = ParseCornerStyle(cornerStyle);
    Wh_FreeStringSetting(cornerStyle);

    PCWSTR backdrop = Wh_GetStringSetting(L"explorerBackdrop");
    g_settings.backdropType = ParseBackdrop(backdrop);
    Wh_FreeStringSetting(backdrop);

    g_settings.sidebarTheming =
        Wh_GetIntSetting(L"explorerSidebarTranslucent") != 0;
    g_settings.roundedSelection =
        Wh_GetIntSetting(L"explorerRoundedSelection") != 0;
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

    std::lock_guard<std::mutex> lock(g_stateMutex);
    for (const auto& state : g_windows) {
        if (IsWindow(state.mainHwnd)) {
            ApplyWindowChrome(state.mainHwnd);
        }
        if (state.treeView && IsWindow(state.treeView) &&
            g_settings.sidebarTheming) {
            SetWindowTheme(state.treeView, L"Explorer", nullptr);
        }
    }
}

void Wh_ModUninit() {
    std::lock_guard<std::mutex> lock(g_stateMutex);

    for (const auto& state : g_windows) {
        if (IsWindow(state.mainHwnd)) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(
                state.mainHwnd, ExplorerMainSubclassProc);

            DWM_WINDOW_CORNER_PREFERENCE defaultCorner = DWMWCP_DEFAULT;
            DwmSetWindowAttribute(state.mainHwnd,
                                   DWMWA_WINDOW_CORNER_PREFERENCE,
                                   &defaultCorner, sizeof(defaultCorner));

            DWM_SYSTEMBACKDROP_TYPE defaultBackdrop = DWMSBT_AUTO;
            DwmSetWindowAttribute(state.mainHwnd, DWMWA_SYSTEMBACKDROP_TYPE,
                                   &defaultBackdrop,
                                   sizeof(defaultBackdrop));
        }
    }
    g_windows.clear();

    for (HWND parent : g_subclassedNotifyParents) {
        if (IsWindow(parent)) {
            WindhawkUtils::RemoveWindowSubclassFromAnyThread(
                parent, NotifyParentSubclassProc);
        }
    }
    g_subclassedNotifyParents.clear();
    g_watchedControls.clear();
}
