#include "main.h"

pfnSetWindowCompositionAttribute g_pSetWindowCompositionAttribute = nullptr;
ULONG_PTR g_gdiplusToken = 0;

HWND g_hMainWnd = nullptr;
bool g_isDragging = false;
POINT g_dragStart = { 0, 0 };
RECT g_wndRectStart = { 0, 0, 0, 0 };

Button g_btnMainSelect;
RECT g_closeBtnRect = { 0, 0, 0, 0 };
bool g_closeBtnHover = false;
RECT g_titleBarRect = { 0, 0, 0, 0 };
RECT g_thumbRect = { 0, 0, 0, 0 };
RECT g_settingsBtnRect = { 0, 0, 0, 0 };
bool g_settingsBtnHover = false;
HHOOK g_hMouseHook = nullptr;

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

static bool CheckBindingMods(const KeyBinding& bind) {
    if (bind.modifiers == 0) return false;
    UINT mods = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
    if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
    return mods == bind.modifiers;
}

static bool CheckBindingModsAndEvent(const KeyBinding& bind, UINT wParam, int wheelDir = 0) {
    if (bind.modifiers == 0) return false;
    UINT mods = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
    if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
    if (mods != bind.modifiers) return false;
    if (bind.IsMouseBased()) return wParam == WM_MOUSEWHEEL;
    if (bind.vk == MOUSE_BIND_MBUTTON) return wParam == WM_MBUTTONDOWN;
    if (bind.vk == MOUSE_BIND_WHEELUP) return wParam == WM_MOUSEWHEEL && wheelDir > 0;
    if (bind.vk == MOUSE_BIND_WHEELDOWN) return wParam == WM_MOUSEWHEEL && wheelDir < 0;
    return false;
}

LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        if (wParam == WM_MBUTTONDOWN) {
            if (CheckBindingModsAndEvent(g_bindClickThrough, wParam) && g_hCloneWnd && IsWindow(g_hCloneWnd)) {
                g_clickThrough = !g_clickThrough;
                InvalidateRect(g_hCloneWnd, nullptr, TRUE);
                return 1;
            }
            bool slow = CheckBindingModsAndEvent(g_bindResizeSlow, wParam);
            bool fast = CheckBindingModsAndEvent(g_bindResizeFast, wParam);
            if (slow || fast) return 1;
        }
        if (wParam == WM_MOUSEWHEEL) {
            MSLLHOOKSTRUCT* p = (MSLLHOOKSTRUCT*)lParam;
            int delta = GET_WHEEL_DELTA_WPARAM(p->mouseData);
            int dir = (delta > 0) ? 1 : -1;

            bool slow = CheckBindingModsAndEvent(g_bindResizeSlow, wParam, dir);
            bool fast = CheckBindingModsAndEvent(g_bindResizeFast, wParam, dir);
            bool ct = CheckBindingModsAndEvent(g_bindClickThrough, wParam, dir);

            if (ct && g_hCloneWnd && IsWindow(g_hCloneWnd)) {
                g_clickThrough = !g_clickThrough;
                InvalidateRect(g_hCloneWnd, nullptr, TRUE);
                return 1;
            }

            if (slow || fast) {
                if (g_hCloneWnd && IsWindow(g_hCloneWnd)) {
                    int step = (fast ? g_settings.fastResizeStep : g_settings.slowResizeStep) * 10;
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
    DrawTextStyled(hMemDC, L"OnTop Windows", { 16, 0, w - 110, 40 }, COLOR_TEXT_PRIMARY, false, 13);
    g_settingsBtnRect = { w - 86, 4, w - 50, 36 };
    if (g_settingsBtnHover) FillRectWithColor(hMemDC, g_settingsBtnRect, COLOR_BG_HOVER);
    DrawTextStyled(hMemDC, L"\u2699", g_settingsBtnRect, g_settingsBtnHover ? COLOR_TEXT_PRIMARY : COLOR_TEXT_SECOND, false, 13, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
        BYTE a = (BYTE)(c >> 24);
        DWORD A = 0xFF;
        bool isBg = false;
        for (int j = 0; j < 3; j++) {
            if (r == bgR[j] && g == bgG[j] && b == bgB[j]) { A = bgA[j]; isBg = true; break; }
        }
        if (isBg && A < 0xFF) {
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
        SaveSettings();
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
            ShowSettingsDialog();
            return 0;
        }
        if (PtInRect(&g_btnMainSelect.rect, pt)) {
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
