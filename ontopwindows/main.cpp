#include "main.h"

pfnSetWindowCompositionAttribute g_pSetWindowCompositionAttribute = nullptr;
ULONG_PTR g_gdiplusToken = 0;

HWND g_hMainWnd = nullptr;
bool g_isDragging = false;
POINT g_dragStart = { 0, 0 };
RECT g_wndRectStart = { 0, 0, 0, 0 };

Button g_btnMainSelect;
Button g_btnMainMenu;
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
    if (bind.vk == MOUSE_BIND_MBUTTON) return wParam == WM_MBUTTONDOWN;
    if (bind.vk == MOUSE_BIND_WHEELUP) return wParam == WM_MOUSEWHEEL && wheelDir > 0;
    if (bind.vk == MOUSE_BIND_WHEELDOWN) return wParam == WM_MOUSEWHEEL && wheelDir < 0;
    if (bind.IsMouseBased()) return wParam == WM_MOUSEWHEEL;
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
    int selectTop = (h - 40 - btnH * 2 - 8) / 2 + 40;
    g_btnMainSelect = { { (w - btnW)/2, selectTop, (w + btnW)/2, selectTop + btnH }, L"\u2795  Выбрать окно", false, true };
    DrawButton(hMemDC, g_btnMainSelect);
    int menuTop = selectTop + btnH + 8;
    g_btnMainMenu = { { (w - btnW)/2, menuTop, (w + btnW)/2, menuTop + btnH }, L"\u2630  Меню", false, false };
    DrawButton(hMemDC, g_btnMainMenu);

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
        CenterWindow(hWnd, 400, 330);
        LoadSettings();
        SaveSettings();
        InstallMouseHook();
        RenderMainWindow(hWnd);
        if (g_settings.autoUpdate) {
            PostMessage(hWnd, WM_AUTO_UPDATE, 0, 0);
        }
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
        if (PtInRect(&g_btnMainMenu.rect, pt)) {
            ClientToScreen(hWnd, &pt);
            ShowMainContextMenu(hWnd, pt.x, pt.y);
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
        bool nm = PtInRect(&g_btnMainMenu.rect, pt);
        if (g_btnMainMenu.hover != nm) { g_btnMainMenu.hover = nm; RenderMainWindow(hWnd); }
        bool ns = PtInRect(&g_settingsBtnRect, pt);
        if (g_settingsBtnHover != ns) { g_settingsBtnHover = ns; RenderMainWindow(hWnd); }
        bool nc = PtInRect(&g_closeBtnRect, pt);
        if (g_closeBtnHover != nc) { g_closeBtnHover = nc; RenderMainWindow(hWnd); }
        break;
    }
    case WM_LBUTTONUP: break;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
        SetCursor(LoadCursorW(nullptr, (PtInRect(&g_btnMainSelect.rect, pt) || PtInRect(&g_btnMainMenu.rect, pt) || PtInRect(&g_settingsBtnRect, pt) || PtInRect(&g_closeBtnRect, pt)) ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_AUTO_UPDATE:
        CheckForUpdates(hWnd, true);
        break;
    case WM_DESTROY:
        if (!g_hCloneWnd) UninstallMouseHook();
        if (!g_hCloneWnd) PostQuitMessage(0);
        g_hMainWnd = nullptr;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static HWND g_hMainMenuWnd = nullptr;
static int g_mainMenuResult = 0;
static bool g_mainMenuDestroying = false;

struct MainMenuItem {
    std::wstring text;
    int id;
    RECT rect;
    bool hover;
};

static std::vector<MainMenuItem> g_mainMenuItems;

static LRESULT CALLBACK MainMenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_NCHITTEST:
        return HTCLIENT;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int w = rc.right, h = rc.bottom;

        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

        FillRectWithColor(hMemDC, rc, COLOR_BG_CARD);
        DrawRoundedRectBorder(hMemDC, rc, COLOR_BG_CARD, COLOR_BORDER, 0, 1);

        for (auto& item : g_mainMenuItems) {
            FillRectWithColor(hMemDC, item.rect, item.hover ? COLOR_BG_HOVER : COLOR_BG_CARD);
            RECT textRc = item.rect;
            textRc.left += 12;
            DrawTextStyled(hMemDC, item.text, textRc, COLOR_TEXT_PRIMARY, false, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }

        BitBlt(hdc, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
        SelectObject(hMemDC, hOldBmp);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool needRepaint = false;
        for (auto& item : g_mainMenuItems) {
            bool hover = PtInRect(&item.rect, pt);
            if (item.hover != hover) { item.hover = hover; needRepaint = true; }
        }
        if (needRepaint) InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        g_mainMenuResult = 0;
        for (auto& item : g_mainMenuItems) {
            if (PtInRect(&item.rect, pt)) { g_mainMenuResult = item.id; break; }
        }
        g_mainMenuDestroying = true;
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_mainMenuResult = 0;
            g_mainMenuDestroying = true;
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_mainMenuDestroying) {
            g_mainMenuResult = 0;
            g_mainMenuDestroying = true;
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_DESTROY:
        g_hMainMenuWnd = nullptr;
        g_mainMenuItems.clear();
        g_mainMenuDestroying = false;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ShowMainContextMenu(HWND hWnd, int screenX, int screenY) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    if (!GetClassInfoExW(hInst, L"MainMenuClass", &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = MainMenuWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"MainMenuClass";
        RegisterClassExW(&wc);
    }

    int itemH = 30;
    int pad = 1;
    int menuW = 220;
    int n = 2;
    int menuH = n * itemH + pad * 2;

    g_mainMenuResult = 0;
    g_mainMenuDestroying = false;
    g_mainMenuItems.clear();
    g_mainMenuItems.push_back({ L"🔄  Проверить обновления", ID_CHECK_UPDATES, { pad, pad, menuW - pad, pad + itemH }, false });
    g_mainMenuItems.push_back({ L"ℹ  О программе", ID_ABOUT, { pad, pad + itemH, menuW - pad, pad + 2 * itemH }, false });

    g_hMainMenuWnd = CreateWindowExW(WS_EX_TOPMOST, L"MainMenuClass", L"", WS_POPUP, screenX, screenY, menuW, menuH, nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hMainMenuWnd, SW_SHOW);
    SetForegroundWindow(g_hMainMenuWnd);

    MSG msg;
    while (g_hMainMenuWnd && IsWindow(g_hMainMenuWnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    switch (g_mainMenuResult) {
    case ID_CHECK_UPDATES:
        CheckForUpdates(hWnd);
        break;
    case ID_ABOUT:
        ShowAboutDialog(hWnd);
        break;
    }
}

static std::wstring GetCurrentVersionString() {
    return APP_VERSION;
}

static int CompareVersions(const std::wstring& a, const std::wstring& b) {
    std::wstring sa = (a.front() == L'v') ? a.substr(1) : a;
    std::wstring sb = (b.front() == L'v') ? b.substr(1) : b;
    int va[4] = {0}, vb[4] = {0};
    int na = 0, nb = 0;
    size_t start = 0, end;
    do {
        end = sa.find(L'.', start);
        if (end == std::wstring::npos) { va[na++] = _wtoi(sa.substr(start).c_str()); break; }
        va[na++] = _wtoi(sa.substr(start, end - start).c_str());
        start = end + 1;
    } while (na < 4);
    start = 0;
    do {
        end = sb.find(L'.', start);
        if (end == std::wstring::npos) { vb[nb++] = _wtoi(sb.substr(start).c_str()); break; }
        vb[nb++] = _wtoi(sb.substr(start, end - start).c_str());
        start = end + 1;
    } while (nb < 4);
    for (int i = 0; i < std::min(na, nb); i++) {
        if (va[i] < vb[i]) return -1;
        if (va[i] > vb[i]) return 1;
    }
    return (na < nb) ? -1 : (na > nb) ? 1 : 0;
}

static std::wstring HttpGetJson(const std::wstring& host, const std::wstring& path) {
    HINTERNET hSession = WinHttpOpen(L"OnTopWindows/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) return L"";

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) { WinHttpCloseHandle(hSession); return L""; }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(), NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return L""; }

    WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, NULL, 0, 0, 0);
    WinHttpReceiveResponse(hRequest, NULL);

    std::string result;
    char buffer[4096];
    DWORD bytesRead;
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        result.append(buffer, bytesRead);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    int len = MultiByteToWideChar(CP_UTF8, 0, result.c_str(), (int)result.size(), nullptr, 0);
    if (len <= 0) return L"";
    std::wstring wresult(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, result.c_str(), (int)result.size(), &wresult[0], len);
    return wresult;
}

void CheckForUpdates(HWND hWnd, bool silent) {
    std::wstring json = HttpGetJson(L"api.github.com", L"/repos/giahoki/OnTop-Windows/releases/latest");
    if (json.empty()) {
        if (!silent)
            MessageBoxW(hWnd, L"Не удалось проверить обновления.\nПроверьте подключение к интернету.", L"Ошибка", MB_OK | MB_ICONWARNING);
        return;
    }

    auto pos = json.find(L"\"tag_name\":\"");
    if (pos == std::wstring::npos) {
        if (!silent)
            MessageBoxW(hWnd, L"Не удалось получить информацию о версии.", L"Ошибка", MB_OK | MB_ICONWARNING);
        return;
    }
    pos += 12;
    auto end = json.find(L"\"", pos);
    if (end == std::wstring::npos) {
        if (!silent)
            MessageBoxW(hWnd, L"Не удалось получить информацию о версии.", L"Ошибка", MB_OK | MB_ICONWARNING);
        return;
    }
    std::wstring latestTag = json.substr(pos, end - pos);
    std::wstring currentVer = GetCurrentVersionString();

    int cmp = CompareVersions(currentVer, latestTag);
    if (cmp < 0) {
        std::wstring msg = L"Доступна новая версия: " + latestTag + L"\n\nТекущая версия: " + currentVer + L"\n\nХотите скачать обновление?";
        if (MessageBoxW(hWnd, msg.c_str(), L"Обновление", MB_YESNO | MB_ICONINFORMATION) == IDYES) {
            ShellExecuteW(hWnd, L"open", L"https://github.com/giahoki/OnTop-Windows/releases/latest", nullptr, nullptr, SW_SHOW);
        }
    } else if (!silent) {
        std::wstring msg = L"У вас актуальная версия: " + currentVer;
        MessageBoxW(hWnd, msg.c_str(), L"Обновление", MB_OK | MB_ICONINFORMATION);
    }
}

static HWND g_hAboutDlg = nullptr;
static RECT g_aboutCloseRc = { 0, 0, 0, 0 };
static RECT g_aboutLinkRc = { 0, 0, 0, 0 };
static RECT g_aboutTgLinkRc = { 0, 0, 0, 0 };
static bool g_aboutCloseHover = false;
static bool g_aboutLinkHover = false;
static bool g_aboutTgLinkHover = false;

static void RenderAboutDialog(HWND hDlg) {
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
    DrawTextStyled(hMemDC, L"\u2139  О программе", { 16, 0, w - 56, 40 }, COLOR_TEXT_PRIMARY, false, 13);

    g_aboutCloseRc = { w - 46, 0, w, 40 };
    DrawCloseButton(hMemDC, g_aboutCloseRc, g_aboutCloseHover);

    std::wstring lines[] = {
        L"OnTop Windows",
        L"Версия " + GetCurrentVersionString(),
        L"",
        L"\u0420\u0430\u0437\u0440\u0430\u0431\u043E\u0442\u0447\u0438\u043A: giahoki",
        L"Telegram: ",
        L"",
        L"\u041B\u0438\u0446\u0435\u043D\u0437\u0438\u044F: MIT",
        L"github.com/giahoki/OnTop-Windows",
    };

    int y = 52;
    for (const auto& line : lines) {
        if (line.empty()) { y += 10; continue; }
        bool bold = (line == L"OnTop Windows") || (line.find(L"\u0420\u0430\u0437\u0440\u0430\u0431\u043E\u0442\u0447\u0438\u043A") == 0);
        bool isLink = (line.find(L"github.com/") == 0);
        bool isTg = (line == L"Telegram: ");
        int fs = isLink ? 13 : 13;
        COLORREF txtColor = (isLink || isTg) ? COLOR_ACCENT : COLOR_TEXT_PRIMARY;
        RECT lineRc = { 20, y, w - 20, y + 24 };
        if (isLink) {
            g_aboutLinkRc = { 20, y, w - 20, y + 24 };
            DrawTextStyled(hMemDC, line, lineRc, txtColor, bold, fs, DT_LEFT | DT_TOP | DT_SINGLELINE);
        } else if (isTg) {
            // Draw "Telegram: " as normal text, then "@bezd2rr" as a link
            RECT labelRc = { 20, y, 120, y + 24 };
            DrawTextStyled(hMemDC, L"Telegram: ", labelRc, COLOR_TEXT_PRIMARY, false, fs, DT_LEFT | DT_TOP | DT_SINGLELINE);
            g_aboutTgLinkRc = { 120, y, w - 20, y + 24 };
            COLORREF tgColor = g_aboutTgLinkHover ? COLOR_ACCENT_HOVER : COLOR_ACCENT;
            DrawTextStyled(hMemDC, L"@bezd2rr", g_aboutTgLinkRc, tgColor, true, fs, DT_LEFT | DT_TOP | DT_SINGLELINE);
        } else {
            DrawTextStyled(hMemDC, line, lineRc, txtColor, bold, fs, DT_LEFT | DT_TOP | DT_SINGLELINE);
        }
        y += 26;
    }

    DWORD* pixel = (DWORD*)bits;
    for (int i = 0; i < w * h; i++) {
        pixel[i] |= 0xFF000000;
    }

    POINT ptSrc = {0, 0};
    SIZE sizeWnd = {w, h};
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(hDlg, hdcScreen, nullptr, &sizeWnd, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hMemDC, hOldBmp); DeleteObject(hBmp); DeleteDC(hMemDC);
    ReleaseDC(NULL, hdcScreen);
}

static LRESULT CALLBACK AboutWndProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        g_hAboutDlg = hDlg;
        g_aboutCloseHover = false;
        g_aboutLinkHover = false;
        g_aboutTgLinkHover = false;
        ApplyWin11Effects(hDlg);
        break;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hDlg, &ps);
        RenderAboutDialog(hDlg);
        EndPaint(hDlg, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_aboutCloseRc, pt)) { DestroyWindow(hDlg); return 0; }
        if (PtInRect(&g_aboutLinkRc, pt)) {
            ShellExecuteW(hDlg, L"open", L"https://github.com/giahoki/OnTop-Windows", nullptr, nullptr, SW_SHOW);
            return 0;
        }
        if (PtInRect(&g_aboutTgLinkRc, pt)) {
            ShellExecuteW(hDlg, L"open", L"https://t.me/bezd2rr", nullptr, nullptr, SW_SHOW);
            return 0;
        }
        if (pt.y < 40) SendMessage(hDlg, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        bool nc = PtInRect(&g_aboutCloseRc, pt);
        if (g_aboutCloseHover != nc) { g_aboutCloseHover = nc; RenderAboutDialog(hDlg); }
        bool nl = PtInRect(&g_aboutLinkRc, pt);
        if (g_aboutLinkHover != nl) { g_aboutLinkHover = nl; RenderAboutDialog(hDlg); }
        bool ntl = PtInRect(&g_aboutTgLinkRc, pt);
        if (g_aboutTgLinkHover != ntl) { g_aboutTgLinkHover = ntl; RenderAboutDialog(hDlg); }
        break;
    }
    case WM_NCHITTEST: return HTCLIENT;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hDlg, &pt);
        if (PtInRect(&g_aboutCloseRc, pt) || PtInRect(&g_aboutLinkRc, pt) || PtInRect(&g_aboutTgLinkRc, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        }
        SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        return TRUE;
    }
    case WM_CLOSE: DestroyWindow(hDlg); return TRUE;
    case WM_DESTROY: g_hAboutDlg = nullptr; break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

void ShowAboutDialog(HWND hWnd) {
    if (g_hAboutDlg && IsWindow(g_hAboutDlg)) { SetForegroundWindow(g_hAboutDlg); return; }
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hWnd, GWLP_HINSTANCE);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = AboutWndProc; wc.hInstance = hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL; wc.lpszClassName = L"AboutWinClass";
    RegisterClassExW(&wc);
    HWND hDlg = CreateWindowExW(WS_EX_LAYERED, L"AboutWinClass", L"\u2139 О программе", WS_POPUP, 0, 0, 360, 280, hWnd, nullptr, hInst, nullptr);
    if (hDlg) {
        CenterWindow(hDlg, 360, 280);
        ShowWindow(hDlg, SW_SHOW);
        RenderAboutDialog(hDlg);
    }
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

    HWND hMainWnd = CreateWindowExW(WS_EX_LAYERED, L"OnTopWindowsMainClass", L"OnTop Windows", WS_POPUP | WS_VISIBLE, 0, 0, 400, 330, nullptr, nullptr, hInstance, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    GdiplusShutdown(g_gdiplusToken);
    return 0;
}
