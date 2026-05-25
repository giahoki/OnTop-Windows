#include "main.h"

HWND g_hCloneWnd = nullptr;
HTHUMBNAIL g_hThumbnail = nullptr;
HWND g_hSourceWindow = nullptr;
double g_sourceAspectRatio = 4.0 / 3.0;
bool g_clickThrough = false;
bool g_showBorder = true;
bool g_isResetting = false;

static HWND g_hMenuWnd = nullptr;
static int g_menuResult = 0;
static bool g_menuDestroying = false;

struct MenuItem {
    std::wstring text;
    int id;
    RECT rect;
    bool hover;
};

static std::vector<MenuItem> g_menuItems;

#define ID_RESET 1001
#define ID_TOGGLE_BORDER 1002

static LRESULT CALLBACK ContextMenuWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

        for (auto& item : g_menuItems) {
            FillRectWithColor(hMemDC, item.rect, item.hover ? COLOR_BG_HOVER : COLOR_BG_CARD);
            RECT textRc = item.rect;
            textRc.left += 12;
            if (item.id == ID_TOGGLE_BORDER && g_showBorder) {
                RECT chkRc = textRc;
                chkRc.right = chkRc.left + 16;
                DrawTextStyled(hMemDC, L"\u2713", chkRc, COLOR_TEXT_PRIMARY, false, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                textRc.left += 18;
            }
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
        for (auto& item : g_menuItems) {
            bool hover = PtInRect(&item.rect, pt);
            if (item.hover != hover) { item.hover = hover; needRepaint = true; }
        }
        if (needRepaint) InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        g_menuResult = 0;
        for (auto& item : g_menuItems) {
            if (PtInRect(&item.rect, pt)) { g_menuResult = item.id; break; }
        }
        g_menuDestroying = true;
        DestroyWindow(hWnd);
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_menuResult = 0;
            g_menuDestroying = true;
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE && !g_menuDestroying) {
            g_menuResult = 0;
            g_menuDestroying = true;
            DestroyWindow(hWnd);
        }
        return 0;
    case WM_DESTROY:
        g_hMenuWnd = nullptr;
        g_menuItems.clear();
        g_menuDestroying = false;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
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

LRESULT CALLBACK CloneWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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
        RegisterHotKey(hWnd, 1, g_bindClickThrough.modifiers, g_bindClickThrough.vk);
        HRESULT hr = DwmRegisterThumbnail(hWnd, g_hSourceWindow, &g_hThumbnail);
        break;
    }
    case WM_HOTKEY: {
        if (wParam == 1) {
            g_clickThrough = !g_clickThrough;
            if (g_clickThrough) {
                SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED | WS_EX_TRANSPARENT);
                SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);
            } else {
                SetWindowLong(hWnd, GWL_EXSTYLE, GetWindowLong(hWnd, GWL_EXSTYLE) & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT));
            }
            SetWindowPos(hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            InvalidateRect(hWnd, nullptr, TRUE);
        }
        break;
    }
    case WM_MOUSEACTIVATE: {
        if (g_clickThrough) return MA_NOACTIVATE;
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

        if (g_showBorder) {
            g_titleBarRect = { 0, 0, w, 36 };
            FillRectWithAlpha(hMemDC, g_titleBarRect, RGB(0x15, 0x15, 0x15), 200);

            std::wstring titleStr = L"OnTop Windows";
            if (g_clickThrough) titleStr += L" \x25CB";
            DrawTextStyled(hMemDC, titleStr, { 12, 0, w - 50, 36 }, g_clickThrough ? COLOR_TEXT_SECOND : COLOR_TEXT_PRIMARY, false, 13);

            g_closeBtnRect = { w - 36, 0, w, 36 };
            DrawCloseButton(hMemDC, g_closeBtnRect, g_closeBtnHover);

            g_thumbRect = { 0, 36, w, h };

            DrawRoundedRectOutline(hMemDC, { 1, 1, w - 1, h - 1 }, COLOR_BORDER, 8, 1);
        } else {
            g_thumbRect = { 0, 0, w, h };
        }

        BitBlt(hdc, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
        SelectObject(hMemDC, hOldBmp);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
        break;
    }
    case WM_SIZE: {
        RECT rc; GetClientRect(hWnd, &rc);
        g_thumbRect = g_showBorder ?
            RECT{ 0, 36, rc.right, rc.bottom } :
            RECT{ 0, 0, rc.right, rc.bottom };
        UpdateThumbnail();
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g_showBorder) {
            if (PtInRect(&g_closeBtnRect, pt)) { DestroyWindow(hWnd); return 0; }
            if (pt.y < 36) {
                SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
            }
        } else {
            SendMessage(hWnd, WM_SYSCOMMAND, SC_MOVE | 0x2, 0);
        }
        break;
    }
    case WM_MOUSEMOVE: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (g_showBorder) {
            bool nc = PtInRect(&g_closeBtnRect, pt);
            if (g_closeBtnHover != nc) { g_closeBtnHover = nc; InvalidateRect(hWnd, nullptr, FALSE); }
        }
        break;
    }
    case WM_RBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hWnd, &pt);
        ShowCloneContextMenu(hWnd, pt.x, pt.y);
        break;
    }
    case WM_LBUTTONUP: break;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hWnd, &pt);
        if (g_showBorder && PtInRect(&g_closeBtnRect, pt)) {
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
        } else {
            SetCursor(LoadCursorW(nullptr, IDC_ARROW));
        }
        return TRUE;
    }
    case WM_DESTROY:
        UnregisterHotKey(hWnd, 1);
        if (g_hThumbnail) DwmUnregisterThumbnail(g_hThumbnail);
        g_hThumbnail = nullptr;
        g_hCloneWnd = nullptr;
        if (g_isResetting) {
            g_isResetting = false;
            HINSTANCE hInst = GetModuleHandleW(nullptr);
            CreateWindowExW(WS_EX_LAYERED, L"OnTopWindowsMainClass", L"OnTop Windows", WS_POPUP | WS_VISIBLE, 0, 0, 400, 300, nullptr, nullptr, hInst, nullptr);
        } else {
            PostQuitMessage(0);
        }
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ShowCloneContextMenu(HWND hWnd, int screenX, int screenY) {
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    if (!GetClassInfoExW(hInst, L"ContextMenuClass", &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = ContextMenuWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"ContextMenuClass";
        RegisterClassExW(&wc);
    }

    int itemH = 30;
    int pad = 1;
    int menuW = 190;
    int n = 2;
    int menuH = n * itemH + pad * 2;

    g_menuResult = 0;
    g_menuDestroying = false;
    g_menuItems.clear();
    g_menuItems.push_back({ L"Сбросить окно", ID_RESET, { pad, pad, menuW - pad, pad + itemH }, false });
    g_menuItems.push_back({ L"Границы", ID_TOGGLE_BORDER, { pad, pad + itemH, menuW - pad, pad + 2 * itemH }, false });

    g_hMenuWnd = CreateWindowExW(WS_EX_TOPMOST, L"ContextMenuClass", L"", WS_POPUP, screenX, screenY, menuW, menuH, nullptr, nullptr, hInst, nullptr);

    ShowWindow(g_hMenuWnd, SW_SHOW);
    SetForegroundWindow(g_hMenuWnd);

    MSG msg;
    while (g_hMenuWnd && IsWindow(g_hMenuWnd) && GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    switch (g_menuResult) {
    case ID_RESET:
        ResetToMainWindow(hWnd);
        break;
    case ID_TOGGLE_BORDER:
        g_showBorder = !g_showBorder;
        SaveSettings();
        {
            RECT rc; GetWindowRect(hWnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            int dh = 36;
            if (g_showBorder) {
                SetWindowPos(hWnd, nullptr, rc.left, rc.top - dh, w, h + dh, SWP_NOZORDER);
            } else {
                SetWindowPos(hWnd, nullptr, rc.left, rc.top + dh, w, h - dh, SWP_NOZORDER);
            }
        }
        InvalidateRect(hWnd, nullptr, TRUE);
        break;
    }
}

void ResetToMainWindow(HWND hCloneWnd) {
    g_isResetting = true;
    DestroyWindow(hCloneWnd);
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
