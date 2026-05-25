#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define NOMINMAX

#pragma execution_character_set("utf-8")

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <vector>
#include <string>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMSBT_TRANSIENTWINDOW
#define DWMSBT_TRANSIENTWINDOW 3
#endif
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

typedef enum _WINDOWCOMPOSITIONATTRIB {
    WCA_UNDEFINED = 0,
    WCA_ACCENT_POLICY = 19
} WINDOWCOMPOSITIONATTRIB;

typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_ENABLE_HOSTBACKDROP = 5
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

typedef struct _WINDOWCOMPOSITIONATTRIBDATA {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
pfnSetWindowCompositionAttribute g_pSetWindowCompositionAttribute = nullptr;
ULONG_PTR g_gdiplusToken = 0;

const COLORREF COLOR_BG_MAIN = RGB(0x20, 0x20, 0x20);
const COLORREF COLOR_BG_CARD = RGB(0x2C, 0x2C, 0x2C);
const COLORREF COLOR_BG_HOVER = RGB(0x38, 0x38, 0x38);
const COLORREF COLOR_TEXT_PRIMARY = RGB(0xFF, 0xFF, 0xFF);
const COLORREF COLOR_TEXT_SECOND = RGB(0xA0, 0xA0, 0xA0);
const COLORREF COLOR_ACCENT = RGB(0x60, 0xCD, 0xFF);
const COLORREF COLOR_ACCENT_HOVER = RGB(0x4F, 0xB4, 0xE6);
const COLORREF COLOR_BORDER = RGB(0x45, 0x45, 0x45);

HWND g_hMainWnd = nullptr;
HWND g_hSelectDlg = nullptr;
HWND g_hCloneWnd = nullptr;
HTHUMBNAIL g_hThumbnail = nullptr;
HWND g_hSourceWindow = nullptr;

bool g_isDragging = false;
POINT g_dragStart = { 0, 0 };
RECT g_wndRectStart = { 0, 0, 0, 0 };

struct Button {
    RECT rect = {0, 0, 0, 0};
    std::wstring text;
    bool hover = false;
    bool isAccent = true;
};

Button g_btnMainSelect;
Button g_btnDlgSelect;
Button g_btnDlgCancel;
RECT g_closeBtnRect = { 0, 0, 0, 0 };
bool g_closeBtnHover = false;
RECT g_titleBarRect = { 0, 0, 0, 0 };
RECT g_thumbRect = { 0, 0, 0, 0 };

RECT g_dlgCloseBtn = { 0, 0, 0, 0 };
bool g_dlgCloseBtnHover = false;
RECT g_setsCloseBtn = { 0, 0, 0, 0 };
bool g_setsCloseBtnHover = false;
bool g_sliderDrag = false;
int g_sliderDragId = 0;
bool g_setsOkBtnHover = false;
bool g_setsCancelBtnHover = false;

struct AppSettings {
    int slowResizeStep = 4;
    int fastResizeStep = 10;
    bool preserveAspectRatio = true;
    int maxCloneWidth = 800;
    int maxCloneHeight = 600;
};
AppSettings g_settings;

RECT g_settingsBtnRect = { 0, 0, 0, 0 };
bool g_settingsBtnHover = false;

HWND g_hSettingsDlg = nullptr;
HHOOK g_hMouseHook = nullptr;

HWND g_hListBox = nullptr;
std::vector<std::pair<HWND, std::wstring>> g_windowList;
double g_sourceAspectRatio = 4.0 / 3.0;
bool g_clickThrough = false;

void FillRectWithColor(HDC hdc, const RECT& rc, COLORREF color) {
    HBRUSH hBrush = CreateSolidBrush(color);
    FillRect(hdc, &rc, hBrush);
    DeleteObject(hBrush);
}

void FillRectWithAlpha(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha) {
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, alpha, 0 };
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);
    HBRUSH hBrush = CreateSolidBrush(color);
    RECT rcFill = { 0, 0, w, h };
    FillRect(hMemDC, &rcFill, hBrush);
    DeleteObject(hBrush);
    AlphaBlend(hdc, rc.left, rc.top, w, h, hMemDC, 0, 0, w, h, blend);
    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hMemDC);
}

void DrawRoundedRect(HDC hdc, const RECT& rc, COLORREF color, int radius = 8) {
    HBRUSH hBrush = CreateSolidBrush(color);
    HPEN hPen = CreatePen(PS_NULL, 0, 0);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    SetBkMode(hdc, TRANSPARENT);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
    DeleteObject(hBrush);
}

void DrawRoundedRectBorder(HDC hdc, const RECT& rc, COLORREF bgColor, COLORREF borderColor, int radius = 8, int borderWidth = 1) {
    DrawRoundedRect(hdc, rc, bgColor, radius);
    HPEN hPen = CreatePen(PS_SOLID, borderWidth, borderColor);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}

void DrawTextStyled(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, bool bold, int fontSize, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, BYTE fontQuality = CLEARTYPE_QUALITY) {
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, fontQuality, VARIABLE_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, color);
    DrawTextW(hdc, text.c_str(), -1, &rc, format);
    SelectObject(hdc, hOldFont);
    DeleteObject(hFont);
}

void DrawTextOnClearBg(HDC hdc, const std::wstring& text, RECT rc, COLORREF color,
                        bool bold, int fontSize, UINT format) {
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0) return;

    HDC hTempDC = CreateCompatibleDC(hdc);
    HBITMAP hTempBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hTempDC, hTempBmp);

    FillRectWithColor(hTempDC, {0, 0, w, h}, COLOR_BG_MAIN);

    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
                              FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                              CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
                              VARIABLE_PITCH, L"Segoe UI");
    HFONT hOldFont = (HFONT)SelectObject(hTempDC, hFont);
    SetBkMode(hTempDC, TRANSPARENT);
    SetTextColor(hTempDC, color);
    RECT tempRc = {0, 0, w, h};
    DrawTextW(hTempDC, text.c_str(), -1, &tempRc, format);
    SelectObject(hTempDC, hOldFont);
    DeleteObject(hFont);

    TransparentBlt(hdc, rc.left, rc.top, w, h, hTempDC, 0, 0, w, h, COLOR_BG_MAIN);

    SelectObject(hTempDC, hOldBmp);
    DeleteObject(hTempBmp);
    DeleteObject(hTempDC);
}

void DrawCloseButton(HDC hdc, const RECT& rc, bool hover) {
    if (hover) FillRectWithColor(hdc, rc, RGB(0xC4, 0x2B, 0x1C));
    HPEN hPen = CreatePen(PS_SOLID, 1, hover ? RGB(0xFF, 0xFF, 0xFF) : COLOR_TEXT_SECOND);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    SetBkMode(hdc, TRANSPARENT);
    int cx = (rc.left + rc.right) / 2;
    int cy = (rc.top + rc.bottom) / 2;
    int s = 6;
    MoveToEx(hdc, cx - s, cy - s, nullptr); LineTo(hdc, cx + s, cy + s);
    MoveToEx(hdc, cx + s, cy - s, nullptr); LineTo(hdc, cx - s, cy + s);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

void DrawButton(HDC hdc, const Button& btn) {
    COLORREF bg = btn.isAccent ? (btn.hover ? COLOR_ACCENT_HOVER : COLOR_ACCENT) : (btn.hover ? COLOR_BG_HOVER : COLOR_BG_CARD);
    DrawRoundedRect(hdc, btn.rect, bg, 6);
    if (!btn.isAccent) {
        HPEN hPen = CreatePen(PS_SOLID, 1, COLOR_BORDER);
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
        SetBkMode(hdc, TRANSPARENT);
        RoundRect(hdc, btn.rect.left, btn.rect.top, btn.rect.right, btn.rect.bottom, 6, 6);
        SelectObject(hdc, hOldPen);
        SelectObject(hdc, hOldBrush);
        DeleteObject(hPen);
    }
    COLORREF txt = btn.isAccent ? RGB(0,0,0) : COLOR_TEXT_PRIMARY;
    DrawTextStyled(hdc, btn.text, btn.rect, txt, btn.isAccent, 13, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

BOOL GetWindowTextSafe(HWND hwnd, LPWSTR lpString, int nMaxCount) {
    if (!IsWindow(hwnd)) return FALSE;
    DWORD_PTR dwResult = 0;
    LRESULT res = SendMessageTimeoutW(hwnd, WM_GETTEXT, (WPARAM)nMaxCount, (LPARAM)lpString, SMTO_ABORTIFHUNG | SMTO_BLOCK, 300, &dwResult);
    return (res != 0 && dwResult > 0);
}

void ApplyWin11Effects(HWND hWnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hWnd, (DWMWINDOWATTRIBUTE)DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    DWORD cornerPref = DWMWCP_ROUND;
    DwmSetWindowAttribute(hWnd, (DWMWINDOWATTRIBUTE)DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref, sizeof(cornerPref));
}

void ApplyAcrylic(HWND hWnd) {
    ACCENT_POLICY accent = { ACCENT_ENABLE_ACRYLICBLURBEHIND, 0, 0xCC202020, 0 };
    WINDOWCOMPOSITIONATTRIBDATA wca = { WCA_ACCENT_POLICY, &accent, sizeof(accent) };
    if (g_pSetWindowCompositionAttribute) {
        g_pSetWindowCompositionAttribute(hWnd, &wca);
    }
}

void CenterWindow(HWND hwnd, int w, int h) {
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    SetWindowPos(hwnd, nullptr, (screenW - w) / 2, (screenH - h) / 2, w, h, SWP_NOZORDER);
}

void UpdateThumbnail() {
    if (g_hThumbnail && g_hCloneWnd) {
        DWM_THUMBNAIL_PROPERTIES props = { 0 };
        props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_SOURCECLIENTAREAONLY;
        props.fVisible = TRUE;
        props.fSourceClientAreaOnly = TRUE;
        props.rcDestination = g_thumbRect;
        DwmUpdateThumbnailProperties(g_hThumbnail, &props);
    }
}

void GetSettingsPath(wchar_t* buf, size_t len) {
    GetModuleFileNameW(nullptr, buf, (DWORD)len);
    wchar_t* slash = wcsrchr(buf, L'\\');
    if (slash) wcscpy_s(slash + 1, len - (slash - buf + 1), L"settings.ini");
}

void LoadSettings() {
    wchar_t path[MAX_PATH];
    GetSettingsPath(path, MAX_PATH);
    g_settings.slowResizeStep = GetPrivateProfileIntW(L"Resize", L"SlowStep", 4, path);
    if (g_settings.slowResizeStep < 1 || g_settings.slowResizeStep > 10) g_settings.slowResizeStep = 4;
    g_settings.fastResizeStep = GetPrivateProfileIntW(L"Resize", L"FastStep", 10, path);
    if (g_settings.fastResizeStep < 1 || g_settings.fastResizeStep > 20) g_settings.fastResizeStep = 10;
    g_settings.preserveAspectRatio = GetPrivateProfileIntW(L"General", L"PreserveAspectRatio", 1, path) != 0;
    g_settings.maxCloneWidth = GetPrivateProfileIntW(L"General", L"MaxCloneWidth", 800, path);
    if (g_settings.maxCloneWidth < 200) g_settings.maxCloneWidth = 200;
    g_settings.maxCloneHeight = GetPrivateProfileIntW(L"General", L"MaxCloneHeight", 600, path);
    if (g_settings.maxCloneHeight < 150) g_settings.maxCloneHeight = 150;
}

void SaveSettings() {
    wchar_t path[MAX_PATH];
    GetSettingsPath(path, MAX_PATH);
    wchar_t buf[16];
    swprintf_s(buf, L"%d", g_settings.slowResizeStep);
    WritePrivateProfileStringW(L"Resize", L"SlowStep", buf, path);
    swprintf_s(buf, L"%d", g_settings.fastResizeStep);
    WritePrivateProfileStringW(L"Resize", L"FastStep", buf, path);
    swprintf_s(buf, L"%d", g_settings.preserveAspectRatio ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"PreserveAspectRatio", buf, path);
    swprintf_s(buf, L"%d", g_settings.maxCloneWidth);
    WritePrivateProfileStringW(L"General", L"MaxCloneWidth", buf, path);
    swprintf_s(buf, L"%d", g_settings.maxCloneHeight);
    WritePrivateProfileStringW(L"General", L"MaxCloneHeight", buf, path);
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && wParam == WM_MOUSEWHEEL) {
        MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
        bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000);
        if (alt) {
            bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000);
            int delta = GET_WHEEL_DELTA_WPARAM(p->mouseData);
            int dir = (delta > 0) ? 1 : -1;
            int step = (shift ? g_settings.fastResizeStep : g_settings.slowResizeStep) * 10;

            if (g_hCloneWnd && IsWindow(g_hCloneWnd)) {
                RECT rc;
                GetWindowRect(g_hCloneWnd, &rc);
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                int newW = w + dir * step; if (newW < 100) newW = 100;
                int newH;
                if (g_settings.preserveAspectRatio) {
                    newH = (int)(newW / g_sourceAspectRatio);
                } else {
                    newH = h + dir * step * 3 / 4;
                }
                if (newH < 60) newH = 60;
                int cx = (rc.left + rc.right) / 2;
                int cy = (rc.top + rc.bottom) / 2;
                SetWindowPos(g_hCloneWnd, nullptr, cx - newW / 2, cy - newH / 2, newW, newH, SWP_NOZORDER);
            }
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void InstallMouseHook() {
    if (!g_hMouseHook) {
        g_hMouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, GetModuleHandleW(nullptr), 0);
    }
}

void UninstallMouseHook() {
    if (g_hMouseHook) {
        UnhookWindowsHookEx(g_hMouseHook);
        g_hMouseHook = nullptr;
    }
}

void RenderMainWindow(HWND hWnd) {
    RECT rc; GetClientRect(hWnd, &rc);
    int w = rc.right, h = rc.bottom;
    if (w <= 0 || h <= 0) return;
    
    HDC hdcScreen = GetDC(NULL);
    
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    
    void* bits;
    HBITMAP hBmp = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HDC hMemDC = CreateCompatibleDC(hdcScreen);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemDC, hBmp);
    memset(bits, 0, (size_t)w * h * 4);
    
    FillRectWithColor(hMemDC, rc, COLOR_BG_MAIN);
    g_titleBarRect = { 0, 0, w, 40 };
    FillRectWithColor(hMemDC, g_titleBarRect, COLOR_BG_CARD);
    DrawTextStyled(hMemDC, L"OnTop Windows", { 16, 0, w - 110, 40 }, COLOR_TEXT_PRIMARY, false, 14);
    g_settingsBtnRect = { w - 86, 4, w - 50, 36 };
    if (g_settingsBtnHover) FillRectWithColor(hMemDC, g_settingsBtnRect, COLOR_BG_HOVER);
    DrawTextStyled(hMemDC, L"\u2699", g_settingsBtnRect, g_settingsBtnHover ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECOND, false, 18, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    g_closeBtnRect = { w - 46, 0, w, 40 };
    DrawCloseButton(hMemDC, g_closeBtnRect, g_closeBtnHover);
    int btnW = 200, btnH = 44;
    g_btnMainSelect = { { (w - btnW)/2, (h - btnH)/2, (w + btnW)/2, (h + btnH)/2 }, L"\u2795  Выбрать окно", false, true };
    DrawButton(hMemDC, g_btnMainSelect);
    
    DWORD* pixel = (DWORD*)bits;
    BYTE bgR[4], bgG[4], bgB[4];
    DWORD bgA[3];
    bgR[0] = GetRValue(COLOR_BG_MAIN); bgG[0] = GetGValue(COLOR_BG_MAIN); bgB[0] = GetBValue(COLOR_BG_MAIN); bgA[0] = 0x01;
    bgR[1] = GetRValue(COLOR_BG_CARD); bgG[1] = GetGValue(COLOR_BG_CARD); bgB[1] = GetBValue(COLOR_BG_CARD); bgA[1] = 0x01;
    bgR[2] = GetRValue(COLOR_BG_HOVER); bgG[2] = GetGValue(COLOR_BG_HOVER); bgB[2] = GetBValue(COLOR_BG_HOVER); bgA[2] = 0x01;
    for (int i = 0; i < w * h; i++) {
        DWORD c = pixel[i];
        BYTE b = (BYTE)c, g = (BYTE)(c >> 8), r = (BYTE)(c >> 16);
        DWORD A = 0xFF;
        for (int j = 0; j < 3; j++) {
            if (r == bgR[j] && g == bgG[j] && b == bgB[j]) { A = bgA[j]; break; }
        }
        if (A < 0xFF) {
            pixel[i] = (A << 24) | ((DWORD)r * A / 255 << 16) | ((DWORD)g * A / 255 << 8) | ((DWORD)b * A / 255);
        } else {
            pixel[i] = c | 0xFF000000;
        }
    }
    
    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {w, h};
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hWnd, hdcScreen, nullptr, &sizeWnd, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);
    
    SelectObject(hMemDC, hOld); DeleteObject(hBmp); DeleteDC(hMemDC);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST: return HTCLIENT;
    case WM_CREATE:
        g_hMainWnd = hWnd;
        ApplyWin11Effects(hWnd);
        ApplyAcrylic(hWnd);
        CenterWindow(hWnd, 400, 300);
        LoadSettings();
        InstallMouseHook();
        RenderMainWindow(hWnd);
        break;

    case WM_ERASEBKGND: return 1;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_closeBtnRect, pt)) { DestroyWindow(hWnd); return 0; }
        if (PtInRect(&g_settingsBtnRect, pt)) {
            extern void ShowSettingsDialog();
            ShowSettingsDialog();
            return 0;
        }
        if (PtInRect(&g_btnMainSelect.rect, pt)) {
            extern void ShowSelectWindowDialog();
            ShowSelectWindowDialog();
            return 0;
        }
        if (pt.y < 40) {
            SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool nh = PtInRect(&g_btnMainSelect.rect, pt);
        if (g_btnMainSelect.hover != nh) { g_btnMainSelect.hover = nh; RenderMainWindow(hWnd); }
        bool ns = PtInRect(&g_settingsBtnRect, pt);
        if (g_settingsBtnHover != ns) { g_settingsBtnHover = ns; RenderMainWindow(hWnd); }
        bool nc = PtInRect(&g_closeBtnRect, pt);
        if (g_closeBtnHover != nc) { g_closeBtnHover = nc; RenderMainWindow(hWnd); }
        break;
    }
    case WM_LBUTTONUP: break;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
        SetCursor(LoadCursorW(nullptr, (PtInRect(&g_btnMainSelect.rect, pt) || PtInRect(&g_settingsBtnRect, pt) || PtInRect(&g_closeBtnRect, pt)) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_DESTROY:
        if (!g_hCloneWnd) UninstallMouseHook();
        if (!g_hCloneWnd) PostQuitMessage(0);
        g_hMainWnd = nullptr;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

BOOL IsWindowSelectable(HWND hwnd) {
    if (!IsWindowVisible(hwnd)) return FALSE;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return FALSE;
    if (IsIconic(hwnd)) return FALSE;

    BOOL cloaked = FALSE;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, (DWMWINDOWATTRIBUTE)DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))) {
        if (cloaked) return FALSE;
    }

    LONG exStyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
    if (exStyle & WS_EX_TOOLWINDOW) return FALSE;

    wchar_t className[128] = { 0 };
    GetClassNameW(hwnd, className, 127);
    if (wcsstr(className, L"Windows.UI.Core.CoreWindow") ||
        wcsstr(className, L"ApplicationFrameWindow") ||
        wcsstr(className, L"Windows.UI.Composition") ||
        wcsstr(className, L"Shell_TrayWnd") ||
        wcsstr(className, L"Shell_SecondaryTrayWnd") ||
        wcsstr(className, L"NotifyIconOverflowWindow") ||
        wcsstr(className, L"IME") ||
        wcsstr(className, L"MSCTF")) {
        return FALSE;
    }

    wchar_t title[512] = { 0 };
    if (!GetWindowTextSafe(hwnd, title, 511)) return FALSE;
    if (wcslen(title) == 0) return FALSE;

    if (hwnd == g_hMainWnd || hwnd == g_hSelectDlg || hwnd == g_hCloneWnd) return FALSE;

    return TRUE;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    if (IsWindowSelectable(hwnd)) {
        wchar_t title[512] = { 0 };
        GetWindowTextSafe(hwnd, title, 511);
        g_windowList.push_back({ hwnd, std::wstring(title) });
    }
    return TRUE;
}

LRESULT CALLBACK SelectWinProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    MARGINS margins = { -1, -1, -1, -1 };
    switch (msg) {
    case WM_CREATE: {
        g_hSelectDlg = hDlg;
        ApplyWin11Effects(hDlg);
        CenterWindow(hDlg, 400, 450);
        g_hListBox = CreateWindowW(L"LISTBOX", 0,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            16, 50, 368, 310, hDlg, (HMENU)100, ((LPCREATESTRUCT)lParam)->hInstance, 0);
            
        CreateWindowW(L"BUTTON", L"Выбрать", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 16, 375, 178, 40, hDlg, (HMENU)101, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        CreateWindowW(L"BUTTON", L"Отмена", WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 206, 375, 178, 40, hDlg, (HMENU)102, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        
        g_windowList.clear();
        EnumWindows(EnumWindowsProc, 0);
        SendMessageW(g_hListBox, LB_SETITEMHEIGHT, 0, 26);
        for (const auto& w : g_windowList) SendMessageW(g_hListBox, LB_ADDSTRING, 0, (LPARAM)w.second.c_str());
        if (!g_windowList.empty()) SendMessageW(g_hListBox, LB_SETCURSEL, 0, 0);
        break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);
        RECT rc; GetClientRect(hDlg, &rc);
        int w = rc.right, h = rc.bottom;

        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

        FillRectWithColor(hMemDC, rc, COLOR_BG_MAIN);

        RECT titleRc = { 0, 0, w, 40 };
        FillRectWithColor(hMemDC, titleRc, COLOR_BG_CARD);
        DrawTextStyled(hMemDC, L"\u25C9  Выберите окно для клонирования", { 16, 0, w - 56, 40 }, COLOR_TEXT_PRIMARY, true, 14);

        g_dlgCloseBtn = { w - 46, 0, w, 40 };
        DrawCloseButton(hMemDC, g_dlgCloseBtn, g_dlgCloseBtnHover);

        RECT sepRc = { 0, 40, w, 41 };
        FillRectWithColor(hMemDC, sepRc, COLOR_BORDER);

        DrawRoundedRectBorder(hMemDC, { 1, 1, w - 1, h - 1 }, COLOR_BG_MAIN, COLOR_BORDER, 8, 1);

        BitBlt(hdc, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
        SelectObject(hMemDC, hOldBmp);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);
        EndPaint(hDlg, &ps);
        return 0;
    }
    case WM_COMMAND: {
        if (LOWORD(wParam) == 101 || (LOWORD(wParam) == 100 && HIWORD(wParam) == LBN_DBLCLK)) {
            int idx = (int)SendMessageW(g_hListBox, LB_GETCURSEL, 0, 0);
            if (idx >= 0 && idx < (int)g_windowList.size()) {
                g_hSourceWindow = g_windowList[idx].first;
                extern void CreateCloneWindow();
                CreateCloneWindow();
                DestroyWindow(g_hSelectDlg);
                if (g_hMainWnd) DestroyWindow(g_hMainWnd);
            }
        }
        else if (LOWORD(wParam) == 102 || LOWORD(wParam) == IDCANCEL) {
            DestroyWindow(hDlg);
        }
        return TRUE;
    }
    case WM_CLOSE: DestroyWindow(hDlg); return TRUE;
    case WM_CTLCOLORLISTBOX: SetBkMode((HDC)wParam, TRANSPARENT); return (LRESULT)GetStockObject(NULL_BRUSH);
    case WM_DRAWITEM: {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if (lpdis->CtlID == 101 || lpdis->CtlID == 102) {
            bool isAccent = (lpdis->CtlID == 101);
            bool isHover = (lpdis->itemState & ODS_SELECTED) || (lpdis->itemState & ODS_HOTLIGHT);
            COLORREF bg = isAccent
                ? (isHover ? RGB(0x4F, 0xCD, 0xFF) : RGB(0x60, 0xCD, 0xFF))
                : (isHover ? RGB(0x3A, 0x3A, 0x3A) : COLOR_BG_CARD);
            RECT rc = lpdis->rcItem; InflateRect(&rc, -1, -1);
            DrawRoundedRect(lpdis->hDC, rc, bg, 6);
            if (isAccent) {
                HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0x50, 0xB8, 0xE8));
                HPEN hOld = (HPEN)SelectObject(lpdis->hDC, hPen);
                HBRUSH hBrOld = (HBRUSH)SelectObject(lpdis->hDC, GetStockObject(NULL_BRUSH));
                SetBkMode(lpdis->hDC, TRANSPARENT);
                RoundRect(lpdis->hDC, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
                SelectObject(lpdis->hDC, hOld); SelectObject(lpdis->hDC, hBrOld); DeleteObject(hPen);
            } else {
                HPEN hPen = CreatePen(PS_SOLID, 1, isHover ? RGB(0x66, 0x66, 0x66) : COLOR_BORDER);
                HPEN hOld = (HPEN)SelectObject(lpdis->hDC, hPen);
                HBRUSH hBrOld = (HBRUSH)SelectObject(lpdis->hDC, GetStockObject(NULL_BRUSH));
                SetBkMode(lpdis->hDC, TRANSPARENT);
                RoundRect(lpdis->hDC, rc.left, rc.top, rc.right, rc.bottom, 6, 6);
                SelectObject(lpdis->hDC, hOld); SelectObject(lpdis->hDC, hBrOld); DeleteObject(hPen);
            }
            wchar_t txt[256] = {0}; GetWindowTextW(lpdis->hwndItem, txt, 256);
            DrawTextStyled(lpdis->hDC, std::wstring(txt), lpdis->rcItem, isAccent ? RGB(0,0,0) : COLOR_TEXT_PRIMARY, isAccent, 14, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            return TRUE;
        }
        if (lpdis->CtlID == 100 && lpdis->itemID != -1 && lpdis->itemID < (UINT)g_windowList.size()) {
            RECT rcItem = lpdis->rcItem; rcItem.left += 4; rcItem.right -= 4; rcItem.top += 2; rcItem.bottom -= 2;
            bool sel = (lpdis->itemState & ODS_SELECTED);
            if (sel) {
                DrawRoundedRect(lpdis->hDC, rcItem, COLOR_ACCENT, 4);
            } else {
                FillRectWithColor(lpdis->hDC, rcItem, COLOR_BG_MAIN);
            }
            HICON hIcon = (HICON)SendMessageW(g_windowList[lpdis->itemID].first, WM_GETICON, ICON_SMALL, 0);
            if (!hIcon) hIcon = (HICON)GetClassLongPtrW(g_windowList[lpdis->itemID].first, GCLP_HICONSM);
            if (!hIcon) hIcon = (HICON)GetClassLongPtrW(g_windowList[lpdis->itemID].first, GCLP_HICON);
            if (hIcon) {
                DrawIconEx(lpdis->hDC, rcItem.left + 6, (rcItem.top + rcItem.bottom - 16) / 2, hIcon, 16, 16, 0, nullptr, DI_NORMAL);
            }
            wchar_t txt[512] = {0}; SendMessageW(g_hListBox, LB_GETTEXT, lpdis->itemID, (LPARAM)txt);
            DrawTextStyled(lpdis->hDC, std::wstring(txt), { rcItem.left + 28, rcItem.top, rcItem.right, rcItem.bottom }, sel ? RGB(0,0,0) : COLOR_TEXT_PRIMARY, false, 13);
            return TRUE;
        }
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_dlgCloseBtn, pt)) { DestroyWindow(hDlg); return 0; }
        if (pt.y < 40) {
            SendMessage(hDlg, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool nc = PtInRect(&g_dlgCloseBtn, pt);
        if (g_dlgCloseBtnHover != nc) { g_dlgCloseBtnHover = nc; InvalidateRect(hDlg, nullptr, FALSE); }
        break;
    }
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hDlg, &pt);
        SetCursor(LoadCursorW(nullptr, PtInRect(&g_dlgCloseBtn, pt) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_LBUTTONUP: break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void ShowSelectWindowDialog() {
    if (g_hSelectDlg && IsWindow(g_hSelectDlg)) { SetForegroundWindow(g_hSelectDlg); return; }
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(g_hMainWnd, GWLP_HINSTANCE);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = SelectWinProc; wc.hInstance = hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL; wc.lpszClassName = L"SelectWinClass";
    RegisterClassExW(&wc);
    HWND hDlg = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"SelectWinClass", L"Выбор окна", WS_POPUP, 0, 0, 400, 450, g_hMainWnd, nullptr, hInst, nullptr);
    if (hDlg) {
        SetLayeredWindowAttributes(hDlg, 0, 190, LWA_ALPHA);
        UpdateWindow(hDlg);
        ShowWindow(hDlg, SW_SHOW);
    }
}

#define ID_SLOW_SLIDER  201
#define ID_FAST_SLIDER  202
#define ID_OK_BTN       203
#define ID_CANCEL_BTN   204

void DrawSliderTrack(HDC hdc, int trackL, int trackR, int cy, int trackH, int thumbX, int thumbR) {
    RECT trackRc = {trackL, cy - trackH/2, trackR, cy + trackH/2};
    FillRectWithColor(hdc, trackRc, COLOR_BG_HOVER);
    if (thumbX > trackL) {
        RECT fillRc = {trackL, cy - trackH/2, thumbX, cy + trackH/2};
        FillRectWithColor(hdc, fillRc, COLOR_ACCENT);
    }
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0x7A, 0x7A, 0x7A));
    HBRUSH hBr = CreateSolidBrush(RGB(0xEE, 0xEE, 0xEE));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBr = (HBRUSH)SelectObject(hdc, hBr);
    Ellipse(hdc, thumbX - thumbR, cy - thumbR, thumbX + thumbR + 1, cy + thumbR + 1);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBr);
    DeleteObject(hPen);
    DeleteObject(hBr);
}

int SliderXFromValue(int val, int minV, int maxV, int trackL, int trackR) {
    return trackL + (val - minV) * (trackR - trackL) / (maxV - minV);
}

int SliderValueFromX(int x, int minV, int maxV, int trackL, int trackR) {
    if (x < trackL) return minV;
    if (x > trackR) return maxV;
    return minV + (x - trackL) * (maxV - minV) / (trackR - trackL);
}

LRESULT CALLBACK SettingsWinProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    const int TRACK_L = 20, TRACK_R = 230, THUMB_R = 7;
    const int SLOW_CY = 72, FAST_CY = 118;
    switch (msg) {
    case WM_CREATE: {
        g_hSettingsDlg = hDlg;
        ApplyWin11Effects(hDlg);
        ACCENT_POLICY accent = { ACCENT_ENABLE_ACRYLICBLURBEHIND, 0, 0xCC202020, 0 };
        WINDOWCOMPOSITIONATTRIBDATA wca = { WCA_ACCENT_POLICY, &accent, sizeof(accent) };
        if (g_pSetWindowCompositionAttribute) {
            g_pSetWindowCompositionAttribute(hDlg, &wca);
        }
        break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hDlg, &ps);
        RECT rc; GetClientRect(hDlg, &rc);
        int w = rc.right, h = rc.bottom;

        HDC hdcScreen = GetDC(NULL);
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = w;
        bi.bmiHeader.biHeight = -h;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits;
        HBITMAP hBmp = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        HDC hMemDC = CreateCompatibleDC(hdcScreen);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);
        memset(bits, 0, (size_t)w * h * 4);

        FillRectWithColor(hMemDC, rc, COLOR_BG_MAIN);

        RECT titleRc = { 0, 0, w, 40 };
        FillRectWithColor(hMemDC, titleRc, COLOR_BG_CARD);
        DrawTextStyled(hMemDC, L"\u2699  Настройки", { 16, 0, w - 56, 40 }, COLOR_TEXT_PRIMARY, true, 15, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ANTIALIASED_QUALITY);

        g_setsCloseBtn = { w - 46, 0, w, 40 };
        DrawCloseButton(hMemDC, g_setsCloseBtn, g_setsCloseBtnHover);

        RECT sepRc = { 0, 40, w, 41 };
        FillRectWithColor(hMemDC, sepRc, COLOR_BORDER);

        DrawTextStyled(hMemDC, L"Медленный ресайз — Alt + Колёсико мыши", { 24, 48, TRACK_R, 68 }, COLOR_TEXT_PRIMARY, false, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ANTIALIASED_QUALITY);
        DrawTextStyled(hMemDC, L"Быстрый ресайз — Alt+Shift + Колёсико мыши", { 24, 96, TRACK_R, 116 }, COLOR_TEXT_PRIMARY, false, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ANTIALIASED_QUALITY);

        wchar_t val[16];

        int tx = SliderXFromValue(g_settings.slowResizeStep, 1, 10, TRACK_L, TRACK_R);
        DrawSliderTrack(hMemDC, TRACK_L, TRACK_R, SLOW_CY, 4, tx, THUMB_R);
        swprintf_s(val, L"%d×", g_settings.slowResizeStep);
        DrawTextStyled(hMemDC, val, { TRACK_R + 8, SLOW_CY - 10, 370, SLOW_CY + 10 }, COLOR_ACCENT, true, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ANTIALIASED_QUALITY);

        tx = SliderXFromValue(g_settings.fastResizeStep, 1, 20, TRACK_L, TRACK_R);
        DrawSliderTrack(hMemDC, TRACK_L, TRACK_R, FAST_CY, 4, tx, THUMB_R);
        swprintf_s(val, L"%d×", g_settings.fastResizeStep);
        DrawTextStyled(hMemDC, val, { TRACK_R + 8, FAST_CY - 10, 370, FAST_CY + 10 }, COLOR_ACCENT, true, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE, ANTIALIASED_QUALITY);

        RECT okRc = { 186, 164, 262, 194 };
        RECT cancelRc = { 276, 164, 352, 194 };
        for (int bi = 0; bi < 2; bi++) {
            bool isOk = (bi == 0);
            RECT brc = isOk ? okRc : cancelRc;
            bool hover = isOk ? g_setsOkBtnHover : g_setsCancelBtnHover;
            COLORREF bg = isOk ? (hover ? RGB(0x4F,0xCD,0xFF) : RGB(0x60,0xCD,0xFF))
                               : (hover ? RGB(0x3A,0x3A,0x3A) : COLOR_BG_CARD);
            RECT brcInner = brc;
            InflateRect(&brcInner, -1, -1);
            if (isOk) {
                DrawRoundedRectBorder(hMemDC, brcInner, bg, RGB(0x50,0xB8,0xE8), 6, 1);
            } else {
                DrawRoundedRectBorder(hMemDC, brcInner, bg, hover ? RGB(0x66,0x66,0x66) : COLOR_BORDER, 6, 1);
            }
            DrawTextStyled(hMemDC, isOk ? L"OK" : L"Отмена", brc, isOk ? RGB(0,0,0) : COLOR_TEXT_PRIMARY, isOk, 14, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        DWORD* pixel = (DWORD*)bits;
        BYTE bgR[4], bgG[4], bgB[4]; DWORD bgA[3];
        bgR[0] = GetRValue(COLOR_BG_MAIN); bgG[0] = GetGValue(COLOR_BG_MAIN); bgB[0] = GetBValue(COLOR_BG_MAIN); bgA[0] = 0x01;
        bgR[1] = GetRValue(COLOR_BG_CARD); bgG[1] = GetGValue(COLOR_BG_CARD); bgB[1] = GetBValue(COLOR_BG_CARD); bgA[1] = 0x30;
        bgR[2] = GetRValue(COLOR_BG_HOVER); bgG[2] = GetGValue(COLOR_BG_HOVER); bgB[2] = GetBValue(COLOR_BG_HOVER); bgA[2] = 0x30;
        for (int i = 0; i < w * h; i++) {
            DWORD c = pixel[i];
            BYTE b = (BYTE)c, g = (BYTE)(c >> 8), r = (BYTE)(c >> 16);
            DWORD A = 0xFF;
            for (int j = 0; j < 3; j++) {
                if (r == bgR[j] && g == bgG[j] && b == bgB[j]) { A = bgA[j]; break; }
            }
            if (A < 0xFF) {
                pixel[i] = (A << 24) | ((DWORD)r * A / 255 << 16) | ((DWORD)g * A / 255 << 8) | ((DWORD)b * A / 255);
            } else {
                pixel[i] = c | 0xFF000000;
            }
        }

        POINT ptSrc = {0, 0};
        SIZE sizeWnd = {w, h};
        BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
        UpdateLayeredWindow(hDlg, hdcScreen, nullptr, &sizeWnd, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

        SelectObject(hMemDC, hOldBmp);
        DeleteObject(hBmp);
        DeleteDC(hMemDC);
        ReleaseDC(NULL, hdcScreen);
        EndPaint(hDlg, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_setsCloseBtn, pt)) { DestroyWindow(hDlg); return 0; }
        RECT okRc = { 186, 164, 262, 194 };
        RECT cancelRc = { 276, 164, 352, 194 };
        if (PtInRect(&okRc, pt)) { SaveSettings(); DestroyWindow(hDlg); return 0; }
        if (PtInRect(&cancelRc, pt)) { LoadSettings(); DestroyWindow(hDlg); return 0; }
        int tx = SliderXFromValue(g_settings.slowResizeStep, 1, 10, TRACK_L, TRACK_R);
        if (abs(pt.x - tx) <= THUMB_R + 4 && abs(pt.y - SLOW_CY) <= THUMB_R + 4) {
            g_sliderDrag = true; g_sliderDragId = ID_SLOW_SLIDER;
            SetCapture(hDlg);
            return 0;
        }
        tx = SliderXFromValue(g_settings.fastResizeStep, 1, 20, TRACK_L, TRACK_R);
        if (abs(pt.x - tx) <= THUMB_R + 4 && abs(pt.y - FAST_CY) <= THUMB_R + 4) {
            g_sliderDrag = true; g_sliderDragId = ID_FAST_SLIDER;
            SetCapture(hDlg);
            return 0;
        }
        if (pt.y >= SLOW_CY - 14 && pt.y <= SLOW_CY + 14 && pt.x >= TRACK_L && pt.x <= TRACK_R) {
            g_settings.slowResizeStep = SliderValueFromX(pt.x, 1, 10, TRACK_L, TRACK_R);
            InvalidateRect(hDlg, nullptr, FALSE);
            return 0;
        }
        if (pt.y >= FAST_CY - 14 && pt.y <= FAST_CY + 14 && pt.x >= TRACK_L && pt.x <= TRACK_R) {
            g_settings.fastResizeStep = SliderValueFromX(pt.x, 1, 20, TRACK_L, TRACK_R);
            InvalidateRect(hDlg, nullptr, FALSE);
            return 0;
        }
        if (pt.y < 40) {
            SendMessage(hDlg, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool nc = PtInRect(&g_setsCloseBtn, pt);
        if (g_setsCloseBtnHover != nc) { g_setsCloseBtnHover = nc; InvalidateRect(hDlg, nullptr, FALSE); }
        RECT okRc = { 186, 164, 262, 194 };
        RECT cancelRc = { 276, 164, 352, 194 };
        bool oh = PtInRect(&okRc, pt);
        if (g_setsOkBtnHover != oh) { g_setsOkBtnHover = oh; InvalidateRect(hDlg, nullptr, FALSE); }
        bool ch = PtInRect(&cancelRc, pt);
        if (g_setsCancelBtnHover != ch) { g_setsCancelBtnHover = ch; InvalidateRect(hDlg, nullptr, FALSE); }
        if (g_sliderDrag) {
            int* pVal = (g_sliderDragId == ID_SLOW_SLIDER) ? &g_settings.slowResizeStep : &g_settings.fastResizeStep;
            int minV = (g_sliderDragId == ID_SLOW_SLIDER) ? 1 : 1;
            int maxV = (g_sliderDragId == ID_SLOW_SLIDER) ? 10 : 20;
            int newVal = SliderValueFromX(pt.x, minV, maxV, TRACK_L, TRACK_R);
            if (*pVal != newVal) { *pVal = newVal; InvalidateRect(hDlg, nullptr, FALSE); }
        }
        break;
    }
    case WM_LBUTTONUP: {
        if (g_sliderDrag) {
            g_sliderDrag = false; g_sliderDragId = 0;
            ReleaseCapture();
            return 0;
        }
        break;
    }
    case WM_NCHITTEST: return HTCLIENT;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hDlg, &pt);
        RECT okRc = { 186, 164, 262, 194 };
        RECT cancelRc = { 276, 164, 352, 194 };
        bool overBtn = PtInRect(&g_setsCloseBtn, pt) || PtInRect(&okRc, pt) || PtInRect(&cancelRc, pt);
        SetCursor(LoadCursorW(nullptr, overBtn ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_CLOSE: DestroyWindow(hDlg); return TRUE;
    case WM_DESTROY: g_hSettingsDlg = nullptr; break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void ShowSettingsDialog() {
    if (g_hSettingsDlg && IsWindow(g_hSettingsDlg)) { SetForegroundWindow(g_hSettingsDlg); return; }
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_hMainWnd, GWLP_HINSTANCE);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = SettingsWinProc; wc.hInstance = hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL; wc.lpszClassName = L"SettingsWinClass";
    RegisterClassExW(&wc);
    HWND hDlg = CreateWindowExW(WS_EX_LAYERED, L"SettingsWinClass", L"\u2699 Настройки", WS_POPUP, 0, 0, 380, 210, g_hMainWnd, nullptr, hInst, nullptr);
    if (hDlg) {
        CenterWindow(hDlg, 380, 210);
        ShowWindow(hDlg, SW_SHOW);
        InvalidateRect(hDlg, nullptr, TRUE);
        UpdateWindow(hDlg);
    }
}

LRESULT CALLBACK CloneWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    MARGINS margins = { -1, -1, -1, -1 };
    switch (msg) {
case WM_CREATE: {
        g_hCloneWnd = hWnd;
        ApplyWin11Effects(hWnd);
        RECT srcRc;
        GetWindowRect(g_hSourceWindow, &srcRc);
        int srcW = srcRc.right - srcRc.left;
        int srcH = srcRc.bottom - srcRc.top;
        if (srcW > 0 && srcH > 0) {
            g_sourceAspectRatio = (double)srcW / srcH;
        } else {
            g_sourceAspectRatio = 4.0 / 3.0;
        }
        int cloneW = g_settings.maxCloneWidth;
        int cloneH = g_settings.maxCloneHeight;
        if (g_settings.preserveAspectRatio && srcW > 0 && srcH > 0) {
            double aspect = (double)srcW / srcH;
            if (srcW > srcH) {
                cloneW = (srcW < g_settings.maxCloneWidth) ? srcW : g_settings.maxCloneWidth;
                cloneH = (int)(cloneW / aspect);
                if (cloneH < 100) cloneH = 100;
            } else {
                cloneH = (srcH < g_settings.maxCloneHeight) ? srcH : g_settings.maxCloneHeight;
                cloneW = (int)(cloneH * aspect);
                if (cloneW < 150) cloneW = 150;
            }
        }
        CenterWindow(hWnd, cloneW, cloneH);
        RegisterHotKey(hWnd, 1, MOD_ALT | MOD_SHIFT, 'O');
        HRESULT hr = DwmRegisterThumbnail(hWnd, g_hSourceWindow, &g_hThumbnail);
        break;
    }
    case WM_HOTKEY: {
        if (wParam == 1) {
            g_clickThrough = !g_clickThrough;
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        break;
    }
    case WM_NCHITTEST: {
        if (g_clickThrough) return HTTRANSPARENT;
        break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int w = rc.right, h = rc.bottom;
        
        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);
        
        FillRectWithAlpha(hMemDC, rc, COLOR_BG_MAIN, 160);
        
        g_titleBarRect = { 0, 0, w, 36 };
        FillRectWithAlpha(hMemDC, g_titleBarRect, RGB(0x15, 0x15, 0x15), 200);
        
        std::wstring titleStr = L"OnTop Windows";
        if (g_clickThrough) titleStr += L" \x25CB";
        DrawTextStyled(hMemDC, titleStr, { 12, 0, w - 50, 36 }, g_clickThrough ? COLOR_TEXT_SECOND : COLOR_TEXT_PRIMARY, false, 14);
        
        g_closeBtnRect = { w - 36, 0, w, 36 };
        DrawCloseButton(hMemDC, g_closeBtnRect, g_closeBtnHover);
        
        g_thumbRect = { 0, 36, w, h };
        
        BitBlt(hdc, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
        SelectObject(hMemDC, hOldBmp);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_SIZE: {
        RECT rc; GetClientRect(hWnd, &rc);
        g_thumbRect = { 0, 36, rc.right, rc.bottom };
        UpdateThumbnail();
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_closeBtnRect, pt)) { DestroyWindow(hWnd); return 0; }
        if (pt.y < 36) {
            SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool nc = PtInRect(&g_closeBtnRect, pt);
        if (g_closeBtnHover != nc) { g_closeBtnHover = nc; InvalidateRect(hWnd, nullptr, FALSE); }
        break;
    }
    case WM_LBUTTONUP: break;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
        SetCursor(LoadCursorW(nullptr, PtInRect(&g_closeBtnRect, pt) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_DESTROY:
        UnregisterHotKey(hWnd, 1);
        if (g_hThumbnail) DwmUnregisterThumbnail(g_hThumbnail);
        g_hThumbnail = nullptr;
        g_hCloneWnd = nullptr;
        PostQuitMessage(0);
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CreateCloneWindow() {
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(g_hMainWnd, GWLP_HINSTANCE);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = CloneWndProc; wc.hInstance = hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL; wc.lpszClassName = L"CloneWndClass";
    RegisterClassExW(&wc);
    CreateWindowExW(WS_EX_TOPMOST, L"CloneWndClass", L"OnTop Windows", WS_POPUP | WS_VISIBLE, 0, 0, 800, 600, nullptr, nullptr, hInst, nullptr);
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
    SetProcessDPIAware();
    InitCommonControls();
    GdiplusStartupInput gsi;
    GdiplusStartup(&g_gdiplusToken, &gsi, nullptr);
    
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) g_pSetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)GetProcAddress(hUser32, "SetWindowCompositionAttribute");
    
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"OnTopWindowsMainClass";
    RegisterClassExW(&wc);
    
    HWND hMainWnd = CreateWindowExW(WS_EX_LAYERED, L"OnTopWindowsMainClass", L"OnTop Windows", WS_POPUP | WS_VISIBLE, 0, 0, 400, 300, nullptr, nullptr, hInstance, nullptr);
    
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
