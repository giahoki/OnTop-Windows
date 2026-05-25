#include "main.h"

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

void DrawRoundedRect(HDC hdc, const RECT& rc, COLORREF color, int radius) {
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

void DrawRoundedRectBorder(HDC hdc, const RECT& rc, COLORREF bgColor, COLORREF borderColor, int radius, int borderWidth) {
    DrawRoundedRect(hdc, rc, bgColor, radius);
    HPEN hPen = CreatePen(PS_SOLID, borderWidth, borderColor);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius, radius);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
}

void DrawTextStyled(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, bool bold, int fontSize, UINT format, BYTE fontQuality) {
    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
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

    HFONT hFont = CreateFontW(fontSize, 0, 0, 0, bold ? FW_BOLD : FW_NORMAL,
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
    DeleteDC(hTempDC);
}

void AddRoundedRectPath(GraphicsPath& path, const Gdiplus::Rect& r, int radius) {
    int d = 2 * radius;
    if (d > r.Width) d = r.Width;
    if (d > r.Height) d = r.Height;
    path.AddArc(r.X, r.Y, d, d, 180, 90);
    path.AddArc(r.X + r.Width - d, r.Y, d, d, 270, 90);
    path.AddArc(r.X + r.Width - d, r.Y + r.Height - d, d, d, 0, 90);
    path.AddArc(r.X, r.Y + r.Height - d, d, d, 90, 90);
    path.CloseFigure();
}

void DrawGradientRoundedRect(HDC hdc, const RECT& rc, COLORREF top, COLORREF bot, int radius) {
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    Gdiplus::Rect r(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
    LinearGradientBrush brush(r, Color(255, GetRValue(top), GetGValue(top), GetBValue(top)),
                              Color(255, GetRValue(bot), GetGValue(bot), GetBValue(bot)),
                              LinearGradientModeVertical);
    GraphicsPath path;
    AddRoundedRectPath(path, r, radius);
    g.FillPath(&brush, &path);
}

void DrawRoundedRectOutline(HDC hdc, const RECT& rc, COLORREF color, int radius, int width) {
    HPEN hPen = CreatePen(PS_SOLID, width, color);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    SetBkMode(hdc, TRANSPARENT);
    RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, radius * 2, radius * 2);
    SelectObject(hdc, hOldPen);
    SelectObject(hdc, hOldBrush);
    DeleteObject(hPen);
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
    const int radius = 10;

    if (btn.isAccent) {
        if (btn.hover) {
            DrawGradientRoundedRect(hdc, btn.rect, RGB(0x70, 0xD0, 0xFF), RGB(0x50, 0xB8, 0xE8), radius);
        } else {
            DrawGradientRoundedRect(hdc, btn.rect, RGB(0x68, 0xCD, 0xFF), RGB(0x4A, 0xB0, 0xE0), radius);
        }
    } else {
        COLORREF bg = btn.hover ? COLOR_BG_HOVER : COLOR_BG_CARD;
        DrawRoundedRect(hdc, btn.rect, bg, radius);
        DrawRoundedRectOutline(hdc, btn.rect, COLOR_BORDER, radius, 1);
    }

    COLORREF txt = btn.isAccent ? RGB(0, 0, 0) : COLOR_TEXT_PRIMARY;
    DrawTextStyled(hdc, btn.text, btn.rect, txt, btn.isAccent, 13, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void DrawSliderTrack(HDC hdc, int trackL, int trackR, int cy, int trackH, int thumbX, int thumbR) {
    RECT trackRc = {trackL, cy - trackH/2, trackR, cy + trackH/2};
    FillRectWithColor(hdc, trackRc, COLOR_BG_HOVER);
    if (thumbX > trackL) {
        RECT fillRc = {trackL, cy - trackH/2, thumbX, cy + trackH/2};
        FillRectWithColor(hdc, fillRc, COLOR_ACCENT);
    }
    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeHighQuality);
    int d = thumbR * 2;
    Gdiplus::Rect rc(thumbX - thumbR, cy - thumbR, d, d);
    Gdiplus::Rect rcShadow(thumbX - thumbR + 1, cy - thumbR + 2, d, d);
    SolidBrush shadowBrush(Color(40, 0, 0, 0));
    g.FillEllipse(&shadowBrush, rcShadow);
    LinearGradientBrush gradBrush(rc, Color(255, 0x88, 0xDD, 0xFF), Color(255, 0x4A, 0xB0, 0xE0), LinearGradientModeVertical);
    g.FillEllipse(&gradBrush, rc);
    Pen borderPen(Color(200, 255, 255, 255), 1.5f);
    g.DrawEllipse(&borderPen, rc);
}

int SliderXFromValue(int val, int minV, int maxV, int trackL, int trackR) {
    return trackL + (val - minV) * (trackR - trackL) / (maxV - minV);
}

int SliderValueFromX(int x, int minV, int maxV, int trackL, int trackR) {
    if (x < trackL) return minV;
    if (x > trackR) return maxV;
    return minV + (x - trackL) * (maxV - minV) / (trackR - trackL);
}
