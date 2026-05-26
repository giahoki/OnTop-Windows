#include "main.h"

HWND g_hCloneWnd = nullptr;
HTHUMBNAIL g_hThumbnail = nullptr;
HWND g_hSourceWindow = nullptr;
double g_sourceAspectRatio = 4.0 / 3.0;
bool g_clickThrough = false;
bool g_showBorder = true;
bool g_isResetting = false;

bool g_isCropping = false;
RECT g_cropRectStart = { 0, 0, 0, 0 };
RECT g_cropRectCurrent = { 0, 0, 0, 0 };
HWND g_hCropOverlay = nullptr;
RECT g_cropSourceRect = { 0, 0, 0, 0 };

KeyBinding g_bindCrop;

#ifndef DWM_TNP_RECTSOURCE
#define DWM_TNP_RECTSOURCE 0x00000002
#endif
#ifndef DWM_TNP_RECTDESTINATION
#define DWM_TNP_RECTDESTINATION 0x00000004
#endif
#ifndef DWM_TNP_SOURCECLIENTAREAONLY
#define DWM_TNP_SOURCECLIENTAREAONLY 0x00000010
#endif
#ifndef PW_RENDERFULLCONTENT
#define PW_RENDERFULLCONTENT 0x00000002
#endif

static bool g_manualCapture = false;
static HBITMAP g_captureBmp = nullptr;
static int g_captureBmpW = 0, g_captureBmpH = 0;
static UINT_PTR g_checkTimer = 0;
static UINT_PTR g_captureTimer = 0;

#define TIMER_CHECK_ID 1001
#define TIMER_CAPTURE_ID 1002
#define CAPTURE_INTERVAL_MS 50
#define CHECK_INTERVAL_MS 500

void UpdateThumbnail() {
    if (g_hThumbnail && g_hCloneWnd) {
        DWM_THUMBNAIL_PROPERTIES props = { 0 };
        props.dwFlags = DWM_TNP_VISIBLE | DWM_TNP_RECTDESTINATION | DWM_TNP_SOURCECLIENTAREAONLY;
        props.fVisible = g_manualCapture ? FALSE : TRUE;
        props.fSourceClientAreaOnly = TRUE;
        props.rcDestination = g_thumbRect;
        
        props.dwFlags |= DWM_TNP_RECTSOURCE;
        if (g_cropSourceRect.right > g_cropSourceRect.left && g_cropSourceRect.bottom > g_cropSourceRect.top) {
            props.rcSource = g_cropSourceRect;
        } else {
            RECT fullRc;
            GetClientRect(g_hSourceWindow, &fullRc);
            props.rcSource = fullRc;
        }
        DwmUpdateThumbnailProperties(g_hThumbnail, &props);
    }
}

static bool IsSourceWindowCapturable() {
    if (!g_hSourceWindow || !IsWindow(g_hSourceWindow)) return false;
    if (IsIconic(g_hSourceWindow)) return false;
    BOOL isCloaked = FALSE;
    DwmGetWindowAttribute(g_hSourceWindow, (DWMWINDOWATTRIBUTE)DWMWA_CLOAKED, &isCloaked, sizeof(isCloaked));
    if (isCloaked) return false;
    return true;
}

static bool CaptureSourceContents() {
    if (!g_hSourceWindow || !IsWindow(g_hSourceWindow)) return false;

    RECT srcRc;
    GetWindowRect(g_hSourceWindow, &srcRc);
    int w = srcRc.right - srcRc.left;
    int h = srcRc.bottom - srcRc.top;
    if (w <= 0 || h <= 0) return false;

    HDC hWinDC = GetWindowDC(g_hSourceWindow);
    if (!hWinDC) return false;

    if (!g_captureBmp || g_captureBmpW != w || g_captureBmpH != h) {
        if (g_captureBmp) DeleteObject(g_captureBmp);
        g_captureBmp = CreateCompatibleBitmap(hWinDC, w, h);
        g_captureBmpW = w;
        g_captureBmpH = h;
    }

    HDC hMemDC = CreateCompatibleDC(hWinDC);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemDC, g_captureBmp);

    PrintWindow(g_hSourceWindow, hMemDC, PW_RENDERFULLCONTENT);

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
    ReleaseDC(g_hSourceWindow, hWinDC);

    return true;
}

static void StartManualCapture(HWND hCloneWnd) {
    if (g_manualCapture) return;
    g_manualCapture = true;
    if (g_hThumbnail) {
        DWM_THUMBNAIL_PROPERTIES props = { 0 };
        props.dwFlags = DWM_TNP_VISIBLE;
        props.fVisible = FALSE;
        DwmUpdateThumbnailProperties(g_hThumbnail, &props);
    }
    g_captureTimer = SetTimer(hCloneWnd, TIMER_CAPTURE_ID, CAPTURE_INTERVAL_MS, nullptr);
}

static void StopManualCapture(HWND hCloneWnd) {
    if (!g_manualCapture) return;
    if (g_captureTimer) { KillTimer(hCloneWnd, g_captureTimer); g_captureTimer = 0; }
    if (g_captureBmp) { DeleteObject(g_captureBmp); g_captureBmp = nullptr; }
    g_captureBmpW = g_captureBmpH = 0;
    g_manualCapture = false;
    UpdateThumbnail();
}

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
#define ID_CROP 1003
#define ID_CLOSE 1004
#define ID_RETURN_MAIN 1005

static RECT g_cropOverlayRect = { 0, 0, 0, 0 };
static POINT g_cropDragStart = { 0, 0 };
static POINT g_cropDragCurrent = { 0, 0 };
static bool g_cropDragging = false;
static HWND g_hCropSourceWnd = nullptr;

LRESULT CALLBACK CropOverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int w = rc.right, h = rc.bottom;
        if (w <= 0 || h <= 0) { EndPaint(hWnd, &ps); return 0; }

        
        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

        
        HBRUSH hDarkBrush = CreateSolidBrush(RGB(8, 8, 8));
        FillRect(hMemDC, &rc, hDarkBrush);
        DeleteObject(hDarkBrush);

        
        if (g_cropDragging) {
            int x1 = (int)g_cropDragStart.x, y1 = (int)g_cropDragStart.y;
            int x2 = (int)g_cropDragCurrent.x, y2 = (int)g_cropDragCurrent.y;
            if (x2 < x1) { int t = x1; x1 = x2; x2 = t; }
            if (y2 < y1) { int t = y1; y1 = y2; y2 = t; }
            if (x2 == x1) x2 = x1 + 1;
            if (y2 == y1) y2 = y1 + 1;

            
            RECT cropRc = { x1, y1, x2, y2 };
            HBRUSH hLightBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hMemDC, &cropRc, hLightBrush);
            DeleteObject(hLightBrush);

            
            HPEN hPen = CreatePen(PS_SOLID, 2, COLOR_ACCENT);
            HPEN hOldPen = (HPEN)SelectObject(hMemDC, hPen);
            HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, GetStockObject(NULL_BRUSH));
            Rectangle(hMemDC, x1, y1, x2, y2);
            SelectObject(hMemDC, hOldPen);
            SelectObject(hMemDC, hOldBrush);
            DeleteObject(hPen);

            
            int cropW = x2 - x1, cropH = y2 - y1;
            int textX = (x1 + x2) / 2;
            int textY = y1 < 30 ? 0 : (y1 - 30);
            wchar_t dimText[32];
            swprintf_s(dimText, L"%d \u00D7 %d", cropW, cropH);
            RECT textRc = { textX - 60, textY, textX + 60, textY + 24 };
            DrawTextStyled(hMemDC, dimText, textRc, COLOR_ACCENT, true, 14, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }

        
        RECT instrRc = { 0, h - 40, w, h };
        DrawTextStyled(hMemDC, L"\u041E\u0442\u043F\u0443\u0441\u0442\u0438\u0442\u0435 \u043A\u043D\u043E\u043F\u043A\u0443 \u0434\u043B\u044F \u043F\u043E\u0434\u0442\u0432\u0435\u0440\u0436\u0434\u0435\u043D\u0438\u044F \u2022 Esc \u0434\u043B\u044F \u043E\u0442\u043C\u0435\u043D\u044B",
                       instrRc, COLOR_TEXT_SECOND, false, 12, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        BitBlt(hdc, 0, 0, w, h, hMemDC, 0, 0, SRCCOPY);
        SelectObject(hMemDC, hOldBmp);
        DeleteObject(hMemBmp);
        DeleteDC(hMemDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        g_cropDragging = true;
        g_cropDragStart = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        g_cropDragCurrent = g_cropDragStart;
        SetCapture(hWnd);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (g_cropDragging) {
            RECT cr; GetClientRect(hWnd, &cr);
            int mx = GET_X_LPARAM(lParam);
            int my = GET_Y_LPARAM(lParam);
            if (mx < cr.left) mx = cr.left;
            if (mx > cr.right) mx = cr.right;
            if (my < cr.top) my = cr.top;
            if (my > cr.bottom) my = cr.bottom;
            g_cropDragCurrent = { mx, my };
            InvalidateRect(hWnd, nullptr, FALSE);
            UpdateWindow(hWnd);
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        if (g_cropDragging) {
            g_cropDragging = false;
            ReleaseCapture();
            RECT cr; GetClientRect(hWnd, &cr);
            int x1 = g_cropDragStart.x;
            int y1 = g_cropDragStart.y;
            int x2 = g_cropDragCurrent.x;
            int y2 = g_cropDragCurrent.y;
            if (x1 < cr.left) x1 = cr.left;
            if (x1 > cr.right) x1 = cr.right;
            if (y1 < cr.top) y1 = cr.top;
            if (y1 > cr.bottom) y1 = cr.bottom;
            if (x2 < cr.left) x2 = cr.left;
            if (x2 > cr.right) x2 = cr.right;
            if (y2 < cr.top) y2 = cr.top;
            if (y2 > cr.bottom) y2 = cr.bottom;
            
            if (x2 < x1) { int t = x1; x1 = x2; x2 = t; }
            if (y2 < y1) { int t = y1; y1 = y2; y2 = t; }
            int cw = x2 - x1;
            int ch = y2 - y1;
            
            if (cw > 50 && ch > 50) {
                
                RECT* cropRect = new RECT{ x1, y1, x2, y2 };
                PostMessage(g_hCropSourceWnd, WM_APP + 100, 0, (LPARAM)cropRect);
            }
        }
        DestroyWindow(hWnd);
        g_hCropOverlay = nullptr;
        g_isCropping = false;
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) {
            g_isCropping = false;
            DestroyWindow(hWnd);
            g_hCropOverlay = nullptr;
        }
        return 0;
    case WM_DESTROY:
        g_hCropOverlay = nullptr;
        g_isCropping = false;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ShowCropOverlay(HWND hCloneWnd) {
    if (g_hCropOverlay && IsWindow(g_hCropOverlay)) return;

    RECT cloneRc;
    GetWindowRect(hCloneWnd, &cloneRc);

    int clientTop = g_showBorder ? 36 : 0;
    RECT overlayRc = {
        cloneRc.left,
        cloneRc.top + clientTop,
        cloneRc.right,
        cloneRc.bottom
    };

    g_cropOverlayRect = overlayRc;
    g_hCropSourceWnd = hCloneWnd;
    g_isCropping = true;
    g_cropDragging = false;
    g_cropRectCurrent = { 0, 0, 0, 0 };

    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtr(hCloneWnd, GWLP_HINSTANCE);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    if (!GetClassInfoExW(hInst, L"CropOverlayClass", &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = CropOverlayWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_CROSS);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"CropOverlayClass";
        RegisterClassExW(&wc);
    }

    int overlayW = overlayRc.right - overlayRc.left;
    int overlayH = overlayRc.bottom - overlayRc.top;

    g_hCropOverlay = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        L"CropOverlayClass", L"", WS_POPUP,
        overlayRc.left, overlayRc.top, overlayW, overlayH,
        hCloneWnd, nullptr, hInst, nullptr
    );

    ShowWindow(g_hCropOverlay, SW_SHOW);
    SetLayeredWindowAttributes(g_hCropOverlay, 0, 200, LWA_ALPHA);

    
    POINT cursorPt;
    GetCursorPos(&cursorPt);
    ScreenToClient(g_hCropOverlay, &cursorPt);
    RECT cr; GetClientRect(g_hCropOverlay, &cr);
    if (cursorPt.x < cr.left) cursorPt.x = cr.left;
    if (cursorPt.x > cr.right) cursorPt.x = cr.right;
    if (cursorPt.y < cr.top) cursorPt.y = cr.top;
    if (cursorPt.y > cr.bottom) cursorPt.y = cr.bottom;
    g_cropDragging = true;
    g_cropDragStart = cursorPt;
    g_cropDragCurrent = cursorPt;
    SetCapture(g_hCropOverlay);

    InvalidateRect(g_hCropOverlay, nullptr, TRUE);
    UpdateWindow(g_hCropOverlay);
}

void ApplyCrop(HWND hCloneWnd, const RECT& cropRect) {
    int cropW = cropRect.right - cropRect.left;
    int cropH = cropRect.bottom - cropRect.top;
    if (cropW < 100) cropW = 100;
    if (cropH < 60) cropH = 60;

    
    RECT oldClientRc;
    GetClientRect(hCloneWnd, &oldClientRc);
    int oldCW = oldClientRc.right - oldClientRc.left;
    int oldCH = oldClientRc.bottom - oldClientRc.top;
    if (oldCW < 1) oldCW = 1;
    if (oldCH < 1) oldCH = 1;

    
    RECT viewRc = g_cropSourceRect;
    if (viewRc.right <= viewRc.left || viewRc.bottom <= viewRc.top) {
        RECT srcRc = { 0, 0, 1, 1 };
        if (g_hSourceWindow && IsWindow(g_hSourceWindow)) {
            GetClientRect(g_hSourceWindow, &srcRc);
        }
        viewRc = { 0, 0, srcRc.right, srcRc.bottom };
    }
    int viewW = viewRc.right - viewRc.left;
    int viewH = viewRc.bottom - viewRc.top;

    
    
    g_cropSourceRect.left   = viewRc.left + cropRect.left   * viewW / oldCW;
    g_cropSourceRect.top    = viewRc.top  + cropRect.top    * viewH / oldCH;
    g_cropSourceRect.right  = viewRc.left + cropRect.right  * viewW / oldCW;
    g_cropSourceRect.bottom = viewRc.top  + cropRect.bottom * viewH / oldCH;

    RECT cloneRc;
    GetWindowRect(hCloneWnd, &cloneRc);

    int titleH = g_showBorder ? 36 : 0;
    int newLeft = cloneRc.left + cropRect.left;
    int newTop  = cloneRc.top  + cropRect.top;

    SetWindowPos(hCloneWnd, nullptr, newLeft, newTop, cropW, cropH + titleH, SWP_NOZORDER);

    UpdateThumbnail();
    InvalidateRect(hCloneWnd, nullptr, TRUE);
}

void ResetCrop() {
    if (!g_hCloneWnd || !IsWindow(g_hCloneWnd)) return;

    
    RECT cloneRc;
    GetWindowRect(g_hCloneWnd, &cloneRc);
    int savedW = cloneRc.right - cloneRc.left;
    int savedH = cloneRc.bottom - cloneRc.top;
    int savedX = cloneRc.left;
    int savedY = cloneRc.top;

    g_isResetting = true;
    DestroyWindow(g_hCloneWnd);

    
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    
    if (g_hCloneWnd && IsWindow(g_hCloneWnd)) {
        SetWindowPos(g_hCloneWnd, nullptr, savedX, savedY, savedW, savedH, SWP_NOZORDER);
    } else {
        
        g_isResetting = false;
        HINSTANCE hInst = GetModuleHandleW(nullptr);
        
        CreateWindowExW(WS_EX_LAYERED, L"OnTopWindowsMainClass", L"OnTop Windows", WS_POPUP | WS_VISIBLE,
                        0, 0, 400, 330, nullptr, nullptr, hInst, nullptr);
    }
}

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
            COLORREF txtColor = item.hover ? COLOR_ACCENT : COLOR_TEXT_PRIMARY;
            if (item.id == ID_CLOSE) txtColor = item.hover ? RGB(0xFF, 0x6B, 0x6B) : RGB(0xFF, 0x8A, 0x8A);
            DrawTextStyled(hMemDC, item.text, textRc, txtColor, false, 14, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
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

LRESULT CALLBACK CloneWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hCloneWnd = hWnd;
        ApplyWin11Effects(hWnd);
        g_checkTimer = SetTimer(hWnd, TIMER_CHECK_ID, CHECK_INTERVAL_MS, nullptr);
        RECT srcRc;
        GetClientRect(g_hSourceWindow, &srcRc);
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
        if (SUCCEEDED(hr)) {
            
            g_thumbRect = g_showBorder ? RECT{ 0, 36, cloneW, cloneH } : RECT{ 0, 0, cloneW, cloneH };
            UpdateThumbnail();
        }
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

        g_thumbRect = g_showBorder ? RECT{ 0, 36, w, h } : RECT{ 0, 0, w, h };

        if (g_manualCapture && g_captureBmp) {
            HDC hCapDC = CreateCompatibleDC(hMemDC);
            HBITMAP hOldCap = (HBITMAP)SelectObject(hCapDC, g_captureBmp);
            SetStretchBltMode(hMemDC, COLORONCOLOR);
            StretchBlt(hMemDC, g_thumbRect.left, g_thumbRect.top,
                        g_thumbRect.right - g_thumbRect.left,
                        g_thumbRect.bottom - g_thumbRect.top,
                        hCapDC, 0, 0, g_captureBmpW, g_captureBmpH, SRCCOPY);
            SelectObject(hCapDC, hOldCap);
            DeleteDC(hCapDC);
        }

        if (g_showBorder) {
            g_titleBarRect = { 0, 0, w, 36 };
            FillRectWithAlpha(hMemDC, g_titleBarRect, RGB(0x15, 0x15, 0x15), 200);

            std::wstring titleStr = L"OnTop Windows";
            if (g_clickThrough) titleStr += L" \u25CB";
            DrawTextStyled(hMemDC, titleStr, { 12, 0, w - 50, 36 }, g_clickThrough ? COLOR_TEXT_SECOND : COLOR_TEXT_PRIMARY, false, 13);

            g_closeBtnRect = { w - 36, 0, w, 36 };
            DrawCloseButton(hMemDC, g_closeBtnRect, g_closeBtnHover);

            DrawRoundedRectOutline(hMemDC, { 1, 1, w - 1, h - 1 }, COLOR_BORDER, 8, 1);
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
        if (g_manualCapture) InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

        
        bool cropModsMatch = (g_bindCrop.modifiers != 0);
        if (cropModsMatch) {
            UINT mods = 0;
            if (GetAsyncKeyState(VK_CONTROL) & 0x8000) mods |= MOD_CONTROL;
            if (GetAsyncKeyState(VK_MENU) & 0x8000) mods |= MOD_ALT;
            if (GetAsyncKeyState(VK_SHIFT) & 0x8000) mods |= MOD_SHIFT;
            if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) mods |= MOD_WIN;
            cropModsMatch = (mods == g_bindCrop.modifiers);
        }
        if (cropModsMatch && g_bindCrop.vk == MOUSE_BIND_LBUTTON) {
            ShowCropOverlay(hWnd);
            return 0;
        }

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
    case WM_TIMER: {
        if (wParam == TIMER_CHECK_ID) {
            bool capturable = IsSourceWindowCapturable();
            if (!capturable && !g_manualCapture) {
                StartManualCapture(hWnd);
                InvalidateRect(hWnd, nullptr, TRUE);
            } else if (capturable && g_manualCapture) {
                StopManualCapture(hWnd);
                InvalidateRect(hWnd, nullptr, TRUE);
            }
            return 0;
        }
        if (wParam == TIMER_CAPTURE_ID) {
            if (g_manualCapture) {
                CaptureSourceContents();
                InvalidateRect(hWnd, nullptr, FALSE);
            }
            return 0;
        }
        break;
    }
    case WM_APP + 100: {
        
        RECT* pCropRect = (RECT*)lParam;
        if (pCropRect) {
            ApplyCrop(hWnd, *pCropRect);
            delete pCropRect;
        }
        return 0;
    }
    case WM_DESTROY:
        if (g_checkTimer) { KillTimer(hWnd, g_checkTimer); g_checkTimer = 0; }
        if (g_captureTimer) { KillTimer(hWnd, g_captureTimer); g_captureTimer = 0; }
        if (g_captureBmp) { DeleteObject(g_captureBmp); g_captureBmp = nullptr; }
        g_captureBmpW = g_captureBmpH = 0;
        g_manualCapture = false;
        UnregisterHotKey(hWnd, 1);
        if (g_hThumbnail) DwmUnregisterThumbnail(g_hThumbnail);
        g_hThumbnail = nullptr;
        g_hCloneWnd = nullptr;
        g_cropSourceRect = { 0, 0, 0, 0 };
        if (g_isResetting) {
            g_isResetting = false;
            HINSTANCE hInst = GetModuleHandleW(nullptr);
            CreateWindowExW(WS_EX_LAYERED, L"OnTopWindowsMainClass", L"OnTop Windows", WS_POPUP | WS_VISIBLE, 0, 0, 400, 330, nullptr, nullptr, hInst, nullptr);
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
    int menuW = 210;
    int n = 4; 
    int menuH = n * itemH + pad * 2;

    g_menuResult = 0;
    g_menuDestroying = false;
    g_menuItems.clear();
    g_menuItems.push_back({ L"\u27F2 \u0421\u0431\u0440\u043E\u0441\u0438\u0442\u044C \u043E\u043A\u043D\u043E", ID_RESET, { pad, pad, menuW - pad, pad + itemH }, false });
    g_menuItems.push_back({ L"\u25A1 \u0413\u0440\u0430\u043D\u0438\u0446\u044B", ID_TOGGLE_BORDER, { pad, pad + itemH, menuW - pad, pad + 2 * itemH }, false });
    g_menuItems.push_back({ L"\u21A9 \u041D\u0430 \u0433\u043B\u0430\u0432\u043D\u043E\u0435", ID_RETURN_MAIN, { pad, pad + 2 * itemH, menuW - pad, pad + 3 * itemH }, false });
    g_menuItems.push_back({ L"\u2715 \u0417\u0430\u043A\u0440\u044B\u0442\u044C", ID_CLOSE, { pad, pad + 3 * itemH, menuW - pad, pad + 4 * itemH }, false });

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
        ResetCloneWindow(hWnd);
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
    case ID_RETURN_MAIN:
        ResetToMainWindow(hWnd);
        break;
    case ID_CLOSE:
        DestroyWindow(hWnd);
        break;
    }
}

void ResetCloneWindow(HWND hCloneWnd) {
    g_cropSourceRect = { 0, 0, 0, 0 };
    RECT srcRc;
    GetClientRect(g_hSourceWindow, &srcRc);
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
    SetWindowPos(hCloneWnd, nullptr, 0, 0, cloneW, cloneH, SWP_NOMOVE | SWP_NOZORDER);
    UpdateThumbnail();
    InvalidateRect(hCloneWnd, nullptr, TRUE);
}

void ResetToMainWindow(HWND hCloneWnd) {
    g_cropSourceRect = { 0, 0, 0, 0 };
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
