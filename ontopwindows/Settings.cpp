#include "main.h"
#include "lang/ru.h"
#include "lang/en.h"

#define ID_SLOW_SLIDER  201
#define ID_FAST_SLIDER  202

AppSettings g_settings;

HWND g_hSettingsDlg = nullptr;
RECT g_setsCloseBtn = { 0, 0, 0, 0 };
bool g_setsCloseBtnHover = false;
bool g_sliderDrag = false;
int g_sliderDragId = 0;
bool g_setsOkBtnHover = false;
bool g_setsCancelBtnHover = false;

RECT g_slowBindRect = { 264, 63, 362, 81 };
RECT g_fastBindRect = { 264, 111, 362, 129 };
RECT g_ctBindRect = { 264, 159, 362, 177 };
RECT g_cropBindRect = { 264, 207, 362, 225 };
bool g_slowBindHover = false;
bool g_fastBindHover = false;
bool g_ctBindHover = false;
bool g_cropBindHover = false;
RECT g_autoUpdateRect = { 0, 0, 0, 0 };
bool g_autoUpdateHover = false;
RECT g_langRect = { 0, 0, 0, 0 };
bool g_langHover = false;

const int TRACK_L = 20, TRACK_R = 230, THUMB_R = 7;
const int SLOW_CY = 72, FAST_CY = 120, CT_CY = 168, CROP_CY = 216, AUTO_CY = 264, LANG_CY = 312;
const int SLOW_LABEL_Y = 50, FAST_LABEL_Y = 98, CT_LABEL_Y = 146, CROP_LABEL_Y = 194, AUTO_LABEL_Y = 242, LANG_LABEL_Y = 290;
const int BIND_X = 264, BIND_W = 98;

void GetSettingsPath(wchar_t* buf, size_t len) {
    GetEnvironmentVariableW(L"APPDATA", buf, (DWORD)len);
    wcscat_s(buf, len, L"\\OnTop Windows");
    CreateDirectoryW(buf, nullptr);
    wcscat_s(buf, len, L"\\settings.ini");
}

void LoadSettings() {
    wchar_t path[MAX_PATH] = L"";
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

    g_showBorder = GetPrivateProfileIntW(L"Window", L"ShowBorder", 1, path) != 0;

    wchar_t buf[64] = {0};
    GetPrivateProfileStringW(L"Binds", L"ClickThrough", L"5:79", buf, 64, path);
    g_bindClickThrough.FromString(buf);
    GetPrivateProfileStringW(L"Binds", L"ResizeSlow", L"1:0", buf, 64, path);
    g_bindResizeSlow.FromString(buf);
    GetPrivateProfileStringW(L"Binds", L"ResizeFast", L"5:0", buf, 64, path);
    g_bindResizeFast.FromString(buf);
    GetPrivateProfileStringW(L"Binds", L"Crop", L"2:4100", buf, 64, path);
    g_bindCrop.FromString(buf);

    g_settings.autoUpdate = GetPrivateProfileIntW(L"General", L"AutoUpdate", 1, path) != 0;
    g_settings.language = GetPrivateProfileIntW(L"General", L"Language", 0, path);
    if (g_settings.language < 0 || g_settings.language > 6) g_settings.language = 1;

    
    wchar_t lastVer[64] = {0};
    GetPrivateProfileStringW(L"General", L"LastVersion", L"", lastVer, 64, path);
    if (wcscmp(lastVer, APP_VERSION) != 0) {
        g_settings.autoUpdate = true;
    }

    int fileVersion = GetPrivateProfileIntW(L"General", L"Version", 0, path);
    if (fileVersion < SETTINGS_VERSION) {
        if (fileVersion == 0) {
            g_bindClickThrough.FromString(L"5:79");
            g_bindResizeSlow.FromString(L"1:0");
            g_bindResizeFast.FromString(L"5:0");
        }
        if (fileVersion < 3) {
            if (g_bindCrop.vk == 0 || g_bindCrop.vk == MOUSE_BIND_LBUTTON) {
                g_bindCrop.modifiers = MOD_CONTROL;
                g_bindCrop.vk = MOUSE_BIND_LBUTTON;
            }
        }
        SaveSettings();
    }
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
    swprintf_s(buf, L"%d", g_showBorder ? 1 : 0);
    WritePrivateProfileStringW(L"Window", L"ShowBorder", buf, path);

    swprintf_s(buf, L"%d", SETTINGS_VERSION);
    WritePrivateProfileStringW(L"General", L"Version", buf, path);

    swprintf_s(buf, L"%u:%u", g_bindClickThrough.modifiers, g_bindClickThrough.vk);
    WritePrivateProfileStringW(L"Binds", L"ClickThrough", buf, path);
    swprintf_s(buf, L"%u:%u", g_bindResizeSlow.modifiers, g_bindResizeSlow.vk);
    WritePrivateProfileStringW(L"Binds", L"ResizeSlow", buf, path);
    swprintf_s(buf, L"%u:%u", g_bindResizeFast.modifiers, g_bindResizeFast.vk);
    WritePrivateProfileStringW(L"Binds", L"ResizeFast", buf, path);
    swprintf_s(buf, L"%u:%u", g_bindCrop.modifiers, g_bindCrop.vk);
    WritePrivateProfileStringW(L"Binds", L"Crop", buf, path);

    swprintf_s(buf, L"%d", g_settings.language);
    WritePrivateProfileStringW(L"General", L"Language", buf, path);
    swprintf_s(buf, L"%d", g_settings.autoUpdate ? 1 : 0);
    WritePrivateProfileStringW(L"General", L"AutoUpdate", buf, path);
    WritePrivateProfileStringW(L"General", L"LastVersion", APP_VERSION, path);
}

static void ShowLangDropdown(HWND hParent, int screenX, int screenY);

void DrawBindingChip(HDC hdc, const RECT& rc, const std::wstring& text, bool hover, bool capturing) {
    COLORREF bg = capturing ? RGB(0x2A, 0x33, 0x3A) : (hover ? COLOR_BG_HOVER : COLOR_BG_CARD);
    DrawRoundedRect(hdc, rc, bg, 6);
    DrawRoundedRectOutline(hdc, rc, capturing ? COLOR_ACCENT : COLOR_BORDER, 6, 1);
    DrawTextStyled(hdc, text, rc, capturing ? COLOR_ACCENT : COLOR_TEXT_SECOND, false, 11, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void RenderSettingsDialog(HWND hDlg) {
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
    DrawTextStyled(hMemDC, g_str->settings_title, { 16, 0, w - 56, 40 }, COLOR_TEXT_PRIMARY, true, 13, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    g_setsCloseBtn = { w - 46, 0, w, 40 };
    DrawCloseButton(hMemDC, g_setsCloseBtn, g_setsCloseBtnHover);

    RECT sepRc = { 0, 40, w, 41 };
    FillRectWithColor(hMemDC, sepRc, COLOR_BORDER);

    wchar_t val[16];

    g_slowBindRect = { BIND_X, SLOW_CY - 9, BIND_X + BIND_W, SLOW_CY + 9 };
    g_fastBindRect = { BIND_X, FAST_CY - 9, BIND_X + BIND_W, FAST_CY + 9 };
    g_ctBindRect = { BIND_X, CT_CY - 9, BIND_X + BIND_W, CT_CY + 9 };
    g_cropBindRect = { BIND_X, CROP_CY - 9, BIND_X + BIND_W, CROP_CY + 9 };

    DrawTextStyled(hMemDC, g_str->settings_slow_resize, { 24, SLOW_LABEL_Y, TRACK_R, SLOW_CY - 4 }, COLOR_TEXT_PRIMARY, false, 13, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    int tx = SliderXFromValue(g_settings.slowResizeStep, 1, 10, TRACK_L, TRACK_R);
    DrawSliderTrack(hMemDC, TRACK_L, TRACK_R, SLOW_CY, 4, tx, THUMB_R);
    swprintf_s(val, L"%d\u00D7", g_settings.slowResizeStep);
    DrawTextStyled(hMemDC, val, { TRACK_R + 8, SLOW_CY - 10, BIND_X - 2, SLOW_CY + 10 }, COLOR_ACCENT, true, 13, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    wchar_t captureDbg[32] = L"...";
    if (g_capturingBinding) {
        swprintf_s(captureDbg, L"H=%d A=%d VK=%u", g_captureHeldMods, g_captureAllMods, g_captureLastVk);
    }
    DrawBindingChip(hMemDC, g_slowBindRect,
        (g_capturingBinding == &g_bindResizeSlow) ? captureDbg : g_bindResizeSlow.ToString(),
        g_slowBindHover, g_capturingBinding == &g_bindResizeSlow);

    DrawTextStyled(hMemDC, g_str->settings_fast_resize, { 24, FAST_LABEL_Y, TRACK_R, FAST_CY - 4 }, COLOR_TEXT_PRIMARY, false, 13, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    tx = SliderXFromValue(g_settings.fastResizeStep, 1, 20, TRACK_L, TRACK_R);
    DrawSliderTrack(hMemDC, TRACK_L, TRACK_R, FAST_CY, 4, tx, THUMB_R);
    swprintf_s(val, L"%d\u00D7", g_settings.fastResizeStep);
    DrawTextStyled(hMemDC, val, { TRACK_R + 8, FAST_CY - 10, BIND_X - 2, FAST_CY + 10 }, COLOR_ACCENT, true, 13, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    DrawBindingChip(hMemDC, g_fastBindRect,
        (g_capturingBinding == &g_bindResizeFast) ? captureDbg : g_bindResizeFast.ToString(),
        g_fastBindHover, g_capturingBinding == &g_bindResizeFast);

    DrawTextStyled(hMemDC, g_str->settings_click_through, { 24, CT_LABEL_Y, w - 24, CT_CY - 4 }, COLOR_TEXT_PRIMARY, false, 13, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    DrawBindingChip(hMemDC, g_ctBindRect,
        (g_capturingBinding == &g_bindClickThrough) ? captureDbg : g_bindClickThrough.ToString(),
        g_ctBindHover, g_capturingBinding == &g_bindClickThrough);

    DrawTextStyled(hMemDC, g_str->settings_crop, { 24, CROP_LABEL_Y, w - 24, CROP_CY - 4 }, COLOR_TEXT_PRIMARY, false, 13, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    DrawBindingChip(hMemDC, g_cropBindRect,
        (g_capturingBinding == &g_bindCrop) ? captureDbg : g_bindCrop.ToString(),
        g_cropBindHover, g_capturingBinding == &g_bindCrop);

    
    g_autoUpdateRect = { BIND_X, AUTO_CY - 8, BIND_X + BIND_W, AUTO_CY + 8 };
    DrawTextStyled(hMemDC, g_str->settings_auto_update, { 24, AUTO_LABEL_Y, TRACK_R, AUTO_CY - 4 }, COLOR_TEXT_PRIMARY, false, 13, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    COLORREF auBg = g_autoUpdateHover ? COLOR_BG_HOVER : COLOR_BG_CARD;
    DrawRoundedRect(hMemDC, g_autoUpdateRect, auBg, 6);
    DrawRoundedRectOutline(hMemDC, g_autoUpdateRect, g_settings.autoUpdate ? COLOR_ACCENT : COLOR_BORDER, 6, 1);
    DrawTextStyled(hMemDC, g_settings.autoUpdate ? g_str->settings_on : g_str->settings_off, g_autoUpdateRect,
                    g_settings.autoUpdate ? COLOR_ACCENT : COLOR_TEXT_SECOND, true, 11,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    g_langRect = { BIND_X, LANG_CY - 9, BIND_X + BIND_W, LANG_CY + 9 };
    DrawTextStyled(hMemDC, g_str->settings_language, { 24, LANG_LABEL_Y, TRACK_R, LANG_CY - 4 }, COLOR_TEXT_PRIMARY, false, 13, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    COLORREF langBg = g_langHover ? COLOR_BG_HOVER : COLOR_BG_CARD;
    DrawRoundedRect(hMemDC, g_langRect, langBg, 6);
    DrawRoundedRectOutline(hMemDC, g_langRect, COLOR_BORDER, 6, 1);
    const wchar_t* langChipNames[] = { g_str->lang_russian, g_str->lang_english, g_str->lang_spanish, g_str->lang_ukrainian, g_str->lang_french, g_str->lang_german, g_str->lang_polish };
    int langIdx = g_settings.language;
    if (langIdx < 0 || langIdx > 6) langIdx = 0;
    DrawTextStyled(hMemDC, langChipNames[langIdx], g_langRect,
                    COLOR_TEXT_SECOND, false, 11,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    DrawTextStyled(hMemDC, L"\u25BC", { BIND_X + BIND_W - 20, LANG_CY - 9, BIND_X + BIND_W - 4, LANG_CY + 9 },
                    COLOR_TEXT_SECOND, false, 9, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    RECT okRc = { 186, 344, 262, 374 };
    RECT cancelRc = { 276, 344, 352, 374 };
    const int btnRadius = 10;
    for (int bi = 0; bi < 2; bi++) {
        bool isOk = (bi == 0);
        RECT brc = isOk ? okRc : cancelRc;
        bool hover = isOk ? g_setsOkBtnHover : g_setsCancelBtnHover;
        RECT brcInner = brc;
        InflateRect(&brcInner, -1, -1);

        if (isOk) {
            if (hover) {
                DrawGradientRoundedRect(hMemDC, brcInner, RGB(0x70, 0xD0, 0xFF), RGB(0x50, 0xB8, 0xE8), btnRadius);
            } else {
                DrawGradientRoundedRect(hMemDC, brcInner, RGB(0x68, 0xCD, 0xFF), RGB(0x4A, 0xB0, 0xE0), btnRadius);
            }
        } else {
            COLORREF bg = hover ? COLOR_BG_HOVER : COLOR_BG_CARD;
            DrawRoundedRect(hMemDC, brcInner, bg, btnRadius);
            DrawRoundedRectOutline(hMemDC, brcInner, COLOR_BORDER, btnRadius, 1);
        }
        DrawTextStyled(hMemDC, isOk ? g_str->settings_ok : g_str->settings_cancel, brc, isOk ? RGB(0,0,0) : COLOR_TEXT_PRIMARY, isOk, 13, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    DWORD* pixel = (DWORD*)bits;
    BYTE bgR[4], bgG[4], bgB[4]; DWORD bgA[3];
    bgR[0] = GetRValue(COLOR_BG_MAIN); bgG[0] = GetGValue(COLOR_BG_MAIN); bgB[0] = GetBValue(COLOR_BG_MAIN); bgA[0] = 0x01;
    bgR[1] = GetRValue(COLOR_BG_CARD); bgG[1] = GetGValue(COLOR_BG_CARD); bgB[1] = GetBValue(COLOR_BG_CARD); bgA[1] = 0x30;
    bgR[2] = GetRValue(COLOR_BG_HOVER); bgG[2] = GetGValue(COLOR_BG_HOVER); bgB[2] = GetBValue(COLOR_BG_HOVER); bgA[2] = 0x30;
    for (int i = 0; i < w * h; i++) {
        DWORD c = pixel[i];
        BYTE b = (BYTE)c, g = (BYTE)(c >> 8), r = (BYTE)(c >> 16);
        bool isBg = false;
        DWORD A = 0xFF;
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
    UpdateLayeredWindow(hDlg, hdcScreen, nullptr, &sizeWnd, hMemDC, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hMemDC, hOldBmp);
    DeleteObject(hBmp);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK SettingsWinProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hSettingsDlg = hDlg;
        g_sliderDrag = false;
        g_setsOkBtnHover = false;
        g_setsCancelBtnHover = false;
        g_slowBindHover = false;
        g_fastBindHover = false;
        g_ctBindHover = false;
        g_cropBindHover = false;
        g_autoUpdateHover = false;
        ApplyWin11Effects(hDlg);
        ACCENT_POLICY accent = { ACCENT_ENABLE_ACRYLICBLURBEHIND, 0, 0xCC202020, 0 };
        WINDOWCOMPOSITIONATTRIBDATA wca = { WCA_ACCENT_POLICY, &accent, sizeof(accent) };
        if (g_pSetWindowCompositionAttribute) {
            g_pSetWindowCompositionAttribute(hDlg, &wca);
        }
        g_hCaptureHook = SetWindowsHookExW(WH_KEYBOARD_LL, CaptureKeyHookProc, GetModuleHandleW(nullptr), 0);
        break;
    }
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
        PAINTSTRUCT ps;
        BeginPaint(hDlg, &ps);
        RenderSettingsDialog(hDlg);
        EndPaint(hDlg, &ps);
        return 0;
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        if (PtInRect(&g_setsCloseBtn, pt)) { CancelCapture(); if (g_hMainWnd) EnableWindow(g_hMainWnd, TRUE); DestroyWindow(hDlg); return 0; }

        RECT okRc = { 186, 344, 262, 374 };
        RECT cancelRc = { 276, 344, 352, 374 };
        if (PtInRect(&okRc, pt)) { CancelCapture(); SaveSettings(); SetLanguage(); if (g_hMainWnd) EnableWindow(g_hMainWnd, TRUE); DestroyWindow(hDlg); return 0; }
        if (PtInRect(&cancelRc, pt)) {
            CancelCapture(); LoadSettings(); SetLanguage(); if (g_hMainWnd) EnableWindow(g_hMainWnd, TRUE); DestroyWindow(hDlg); return 0;
        }

        if (PtInRect(&g_slowBindRect, pt)) { CancelCapture(); StartCapture(&g_bindResizeSlow); return 0; }
        if (PtInRect(&g_fastBindRect, pt)) { CancelCapture(); StartCapture(&g_bindResizeFast); return 0; }
        if (PtInRect(&g_ctBindRect, pt)) { CancelCapture(); StartCapture(&g_bindClickThrough); return 0; }
        if (PtInRect(&g_cropBindRect, pt)) { CancelCapture(); StartCapture(&g_bindCrop); return 0; }
        if (PtInRect(&g_autoUpdateRect, pt)) { g_settings.autoUpdate = !g_settings.autoUpdate; InvalidateRect(hDlg, nullptr, FALSE); return 0; }
        if (PtInRect(&g_langRect, pt)) {
            POINT langPt = { g_langRect.left, g_langRect.bottom };
            ClientToScreen(hDlg, &langPt);
            ShowLangDropdown(hDlg, langPt.x, langPt.y);
            return 0;
        }

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
        RECT okRc = { 186, 344, 262, 374 };
        RECT cancelRc = { 276, 344, 352, 374 };
        bool oh = PtInRect(&okRc, pt);
        if (g_setsOkBtnHover != oh) { g_setsOkBtnHover = oh; InvalidateRect(hDlg, nullptr, FALSE); }
        bool ch = PtInRect(&cancelRc, pt);
        if (g_setsCancelBtnHover != ch) { g_setsCancelBtnHover = ch; InvalidateRect(hDlg, nullptr, FALSE); }

        bool sh = PtInRect(&g_slowBindRect, pt);
        if (g_slowBindHover != sh) { g_slowBindHover = sh; InvalidateRect(hDlg, nullptr, FALSE); }
        bool fh = PtInRect(&g_fastBindRect, pt);
        if (g_fastBindHover != fh) { g_fastBindHover = fh; InvalidateRect(hDlg, nullptr, FALSE); }
        bool cth = PtInRect(&g_ctBindRect, pt);
        if (g_ctBindHover != cth) { g_ctBindHover = cth; InvalidateRect(hDlg, nullptr, FALSE); }
        bool crh = PtInRect(&g_cropBindRect, pt);
        if (g_cropBindHover != crh) { g_cropBindHover = crh; InvalidateRect(hDlg, nullptr, FALSE); }
        bool auh = PtInRect(&g_autoUpdateRect, pt);
        if (g_autoUpdateHover != auh) { g_autoUpdateHover = auh; InvalidateRect(hDlg, nullptr, FALSE); }
        bool lh = PtInRect(&g_langRect, pt);
        if (g_langHover != lh) { g_langHover = lh; InvalidateRect(hDlg, nullptr, FALSE); }

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
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE) CancelCapture();
        break;
    case WM_BINDING_UPDATE:
        RenderSettingsDialog(hDlg);
        return 0;
    case WM_NCHITTEST: return HTCLIENT;
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(hDlg, &pt);
        RECT okRc = { 186, 344, 262, 374 };
        RECT cancelRc = { 276, 344, 352, 374 };
        bool overBtn = PtInRect(&g_setsCloseBtn, pt) || PtInRect(&okRc, pt) || PtInRect(&cancelRc, pt) ||
                       PtInRect(&g_slowBindRect, pt) || PtInRect(&g_fastBindRect, pt) || PtInRect(&g_ctBindRect, pt) ||
                       PtInRect(&g_cropBindRect, pt) || PtInRect(&g_autoUpdateRect, pt) || PtInRect(&g_langRect, pt);
        SetCursor(LoadCursorW(nullptr, overBtn ? IDC_HAND : IDC_ARROW));
        return TRUE;
    }
    case WM_CLOSE: if (g_hMainWnd) EnableWindow(g_hMainWnd, TRUE); DestroyWindow(hDlg); return TRUE;
    case WM_DESTROY: g_hSettingsDlg = nullptr; CancelCapture(); if (g_hCaptureHook) { UnhookWindowsHookEx(g_hCaptureHook); g_hCaptureHook = nullptr; } if (g_hMainWnd && IsWindow(g_hMainWnd)) EnableWindow(g_hMainWnd, TRUE); break;
    }
    return DefWindowProcW(hDlg, msg, wParam, lParam);
}

static HWND g_hLangPopup = nullptr;
static int g_langPopupResult = -1;
static bool g_langPopupItems[7] = {};

static LRESULT CALLBACK LangPopupWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc; GetClientRect(hWnd, &rc);
        int w = rc.right, h = rc.bottom;

        HDC hMemDC = CreateCompatibleDC(hdc);
        HBITMAP hMemBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

        FillRectWithColor(hMemDC, rc, COLOR_BG_CARD);
        DrawRoundedRectOutline(hMemDC, rc, COLOR_BORDER, 6, 1);

        int itemH = 28;
        const wchar_t* langNames[] = { g_str->lang_russian, g_str->lang_english, g_str->lang_spanish, g_str->lang_ukrainian, g_str->lang_french, g_str->lang_german, g_str->lang_polish };
        for (int i = 0; i < 7; i++) {
            RECT itemRc = { 2, 2 + i * itemH, w - 2, 2 + (i + 1) * itemH };
            if (g_langPopupItems[i]) {
                DrawRoundedRect(hMemDC, itemRc, COLOR_BG_HOVER, 4);
            }
            DrawTextStyled(hMemDC, langNames[i],
                            itemRc, (i == g_settings.language) ? COLOR_ACCENT : COLOR_TEXT_PRIMARY,
                            (i == g_settings.language), 12,
                            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
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
        int itemH = 28;
        RECT clientRc; GetClientRect(hWnd, &clientRc);
        bool needRepaint = false;
        for (int i = 0; i < 7; i++) {
            RECT itemRc = { 2, 2 + i * itemH, clientRc.right - 2, 2 + (i + 1) * itemH };
            bool hover = PtInRect(&itemRc, pt);
            if (g_langPopupItems[i] != hover) { g_langPopupItems[i] = hover; needRepaint = true; }
        }
        if (needRepaint) InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        int itemH = 28;
        RECT clientRc; GetClientRect(hWnd, &clientRc);
        for (int i = 0; i < 7; i++) {
            RECT itemRc = { 2, 2 + i * itemH, clientRc.right - 2, 2 + (i + 1) * itemH };
            if (PtInRect(&itemRc, pt)) { g_langPopupResult = i; DestroyWindow(hWnd); return 0; }
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { g_langPopupResult = -1; DestroyWindow(hWnd); }
        return 0;
    case WM_ACTIVATE:
        if (LOWORD(wParam) == WA_INACTIVE) { DestroyWindow(hWnd); }
        return 0;
    case WM_DESTROY:
        g_hLangPopup = nullptr;
        break;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

static void ShowLangDropdown(HWND hParent, int screenX, int screenY) {
    if (g_hLangPopup) return;
    g_langPopupResult = -1;
    for (int i = 0; i < 7; i++) g_langPopupItems[i] = false;

    HINSTANCE hInst = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    if (!GetClassInfoExW(hInst, L"LangPopupClass", &wc)) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = LangPopupWndProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.hbrBackground = NULL;
        wc.lpszClassName = L"LangPopupClass";
        RegisterClassExW(&wc);
    }

    int popupW = g_langRect.right - g_langRect.left;
    int popupH = 7 * 28 + 4;
    g_hLangPopup = CreateWindowExW(WS_EX_TOPMOST, L"LangPopupClass", L"", WS_POPUP,
                                    screenX, screenY, popupW, popupH,
                                    hParent, nullptr, hInst, nullptr);
    if (g_hLangPopup) {
        HRGN rgn = CreateRoundRectRgn(0, 0, popupW, popupH, 12, 12);
        SetWindowRgn(g_hLangPopup, rgn, TRUE);
        ShowWindow(g_hLangPopup, SW_SHOW);
        SetForegroundWindow(g_hLangPopup);

        MSG msg;
        while (g_hLangPopup && GetMessageW(&msg, nullptr, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (g_langPopupResult >= 0) {
            g_settings.language = g_langPopupResult;
            SetLanguage();
            RenderSettingsDialog(hParent);
        }
    }
}

void ShowSettingsDialog() {
    if (g_hSettingsDlg && IsWindow(g_hSettingsDlg)) { SetForegroundWindow(g_hSettingsDlg); return; }
    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(g_hMainWnd, GWLP_HINSTANCE);
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW) };
    wc.lpfnWndProc = SettingsWinProc; wc.hInstance = hInst; wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = NULL; wc.lpszClassName = L"SettingsWinClass";
    RegisterClassExW(&wc);
    HWND hDlg = CreateWindowExW(WS_EX_LAYERED, L"SettingsWinClass", g_str->settings_title, WS_POPUP, 0, 0, 380, 410, g_hMainWnd, nullptr, hInst, nullptr);
    if (hDlg) {
        CenterWindow(hDlg, 380, 410);
        EnableWindow(g_hMainWnd, FALSE);
        ShowWindow(hDlg, SW_SHOW);
        RenderSettingsDialog(hDlg);
    }
}