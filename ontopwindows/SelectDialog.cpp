#include "main.h"

HWND g_hSelectDlg = nullptr;
HWND g_hListBox = nullptr;
std::vector<std::pair<HWND, std::wstring>> g_windowList;
RECT g_dlgCloseBtn = { 0, 0, 0, 0 };
bool g_dlgCloseBtnHover = false;

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
    switch (msg) {
    case WM_CREATE: {
        g_hSelectDlg = hDlg;
        ApplyWin11Effects(hDlg);
        CenterWindow(hDlg, 400, 450);
        g_hListBox = CreateWindowW(L"LISTBOX", 0,
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS,
            16, 50, 368, 310, hDlg, (HMENU)100, ((LPCREATESTRUCT)lParam)->hInstance, 0);

        CreateWindowW(L"BUTTON", g_str->select_btn, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 16, 375, 178, 40, hDlg, (HMENU)101, ((LPCREATESTRUCT)lParam)->hInstance, 0);
        CreateWindowW(L"BUTTON", g_str->select_cancel, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 206, 375, 178, 40, hDlg, (HMENU)102, ((LPCREATESTRUCT)lParam)->hInstance, 0);

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
        DrawTextStyled(hMemDC, g_str->select_title, { 16, 0, w - 56, 40 }, COLOR_TEXT_PRIMARY, true, 13);

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
            RECT rc = lpdis->rcItem;
            bool isAccent = (lpdis->CtlID == 101);
            bool isHover = (lpdis->itemState & ODS_SELECTED) || (lpdis->itemState & ODS_HOTLIGHT);

            FillRectWithColor(lpdis->hDC, rc, COLOR_BG_MAIN);

            Button btn;
            btn.rect = rc;
            btn.hover = isHover;
            btn.isAccent = isAccent;
            wchar_t txt[256] = {0}; GetWindowTextW(lpdis->hwndItem, txt, 256);
            btn.text = txt;
            DrawButton(lpdis->hDC, btn);
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
    HWND hDlg = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED, L"SelectWinClass", g_str->select_window_title, WS_POPUP, 0, 0, 400, 450, g_hMainWnd, nullptr, hInst, nullptr);
    if (hDlg) {
        SetLayeredWindowAttributes(hDlg, 0, 190, LWA_ALPHA);
        UpdateWindow(hDlg);
        ShowWindow(hDlg, SW_SHOW);
    }
}