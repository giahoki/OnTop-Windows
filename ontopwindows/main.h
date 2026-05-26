#pragma once

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
#include <winhttp.h>
#include <vector>
#include <string>

#pragma comment(linker, "/SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "winhttp.lib")

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
const COLORREF COLOR_BG_MAIN = RGB(0x20, 0x20, 0x20);
const COLORREF COLOR_BG_CARD = RGB(0x2C, 0x2C, 0x2C);
const COLORREF COLOR_BG_HOVER = RGB(0x38, 0x38, 0x38);
const COLORREF COLOR_TEXT_PRIMARY = RGB(0xFF, 0xFF, 0xFF);
const COLORREF COLOR_TEXT_SECOND = RGB(0xA0, 0xA0, 0xA0);
const COLORREF COLOR_ACCENT = RGB(0x60, 0xCD, 0xFF);
const COLORREF COLOR_ACCENT_HOVER = RGB(0x4F, 0xB4, 0xE6);
const COLORREF COLOR_BORDER = RGB(0x45, 0x45, 0x45);

#define SETTINGS_VERSION 4

#define APP_VERSION L"1.0.4"

struct Button {
    RECT rect = {0, 0, 0, 0};
    std::wstring text;
    bool hover = false;
    bool isAccent = true;
};

struct AppSettings {
    int slowResizeStep = 4;
    int fastResizeStep = 10;
    bool preserveAspectRatio = true;
    int maxCloneWidth = 800;
    int maxCloneHeight = 600;
    bool autoUpdate = true;
};

#define MOUSE_BIND_MBUTTON   0x1001
#define MOUSE_BIND_WHEELUP   0x1002
#define MOUSE_BIND_WHEELDOWN 0x1003
#define MOUSE_BIND_LBUTTON   0x1004

struct KeyBinding {
    UINT modifiers;
    UINT vk;

    std::wstring ToString() const;
    void FromString(const std::wstring& str);
    bool IsMouseBased() const { return vk == 0 || vk >= MOUSE_BIND_MBUTTON; }
};

extern pfnSetWindowCompositionAttribute g_pSetWindowCompositionAttribute;
extern ULONG_PTR g_gdiplusToken;
extern HWND g_hMainWnd;
extern HWND g_hSelectDlg;
extern HWND g_hCloneWnd;
extern HTHUMBNAIL g_hThumbnail;
extern HWND g_hSourceWindow;
extern bool g_isDragging;
extern POINT g_dragStart;
extern RECT g_wndRectStart;
extern Button g_btnMainSelect;
extern Button g_btnMainMenu;
extern RECT g_closeBtnRect;
extern bool g_closeBtnHover;
extern RECT g_titleBarRect;
extern RECT g_thumbRect;
extern RECT g_dlgCloseBtn;
extern bool g_dlgCloseBtnHover;
extern RECT g_setsCloseBtn;
extern bool g_setsCloseBtnHover;
extern bool g_sliderDrag;
extern int g_sliderDragId;
extern bool g_setsOkBtnHover;
extern bool g_setsCancelBtnHover;
extern AppSettings g_settings;
extern RECT g_settingsBtnRect;
extern bool g_settingsBtnHover;
extern HWND g_hSettingsDlg;
extern HHOOK g_hMouseHook;
extern HWND g_hListBox;
extern std::vector<std::pair<HWND, std::wstring>> g_windowList;
extern double g_sourceAspectRatio;
extern bool g_clickThrough;
extern bool g_showBorder;
extern bool g_isResetting;
extern KeyBinding g_bindClickThrough;
extern KeyBinding g_bindResizeSlow;
extern KeyBinding g_bindResizeFast;
extern KeyBinding g_bindCrop;
extern bool g_isCropping;
extern RECT g_cropRectStart;
extern RECT g_cropRectCurrent;
extern HWND g_hCropOverlay;
extern RECT g_cropSourceRect;
extern RECT g_cropLinkRc;
extern bool g_cropLinkHover;
extern KeyBinding* g_capturingBinding;
extern HHOOK g_hCaptureHook;
extern HHOOK g_hCaptureMouseHook;
extern UINT g_captureHeldMods;
extern UINT g_captureAllMods;
extern UINT g_captureLastVk;
extern RECT g_slowBindRect;
extern RECT g_fastBindRect;
extern RECT g_ctBindRect;
extern bool g_slowBindHover;
extern bool g_fastBindHover;
extern bool g_ctBindHover;

void FillRectWithColor(HDC hdc, const RECT& rc, COLORREF color);
void FillRectWithAlpha(HDC hdc, const RECT& rc, COLORREF color, BYTE alpha);
void DrawRoundedRect(HDC hdc, const RECT& rc, COLORREF color, int radius = 8);
void DrawRoundedRectBorder(HDC hdc, const RECT& rc, COLORREF bgColor, COLORREF borderColor, int radius = 8, int borderWidth = 1);
void DrawTextStyled(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, bool bold, int fontSize, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS, BYTE fontQuality = CLEARTYPE_QUALITY);
void DrawTextOnClearBg(HDC hdc, const std::wstring& text, RECT rc, COLORREF color, bool bold, int fontSize, UINT format);
void AddRoundedRectPath(GraphicsPath& path, const Gdiplus::Rect& r, int radius);
void DrawGradientRoundedRect(HDC hdc, const RECT& rc, COLORREF top, COLORREF bot, int radius);
void DrawRoundedRectOutline(HDC hdc, const RECT& rc, COLORREF color, int radius, int width);
void DrawCloseButton(HDC hdc, const RECT& rc, bool hover);
void DrawButton(HDC hdc, const Button& btn);
void DrawSliderTrack(HDC hdc, int trackL, int trackR, int cy, int trackH, int thumbX, int thumbR);
int SliderXFromValue(int val, int minV, int maxV, int trackL, int trackR);
int SliderValueFromX(int x, int minV, int maxV, int trackL, int trackR);

BOOL GetWindowTextSafe(HWND hwnd, LPWSTR lpString, int nMaxCount);
void ApplyWin11Effects(HWND hWnd);
void ApplyAcrylic(HWND hWnd);
void CenterWindow(HWND hwnd, int w, int h);
void UpdateThumbnail();
void GetSettingsPath(wchar_t* buf, size_t len);
void LoadSettings();
void SaveSettings();

void RenderMainWindow(HWND hWnd);
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void InstallMouseHook();
void UninstallMouseHook();
LRESULT CALLBACK MouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

BOOL IsWindowSelectable(HWND hwnd);
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam);
void ShowSelectWindowDialog();
LRESULT CALLBACK SelectWinProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void CreateCloneWindow();
void ShowCloneContextMenu(HWND hWnd, int screenX, int screenY);
void ResetToMainWindow(HWND hCloneWnd);
LRESULT CALLBACK CloneWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define ID_CHECK_UPDATES 2001
#define ID_ABOUT 2002
#define WM_AUTO_UPDATE (WM_APP + 0x200)
void ShowMainContextMenu(HWND hWnd, int screenX, int screenY);
void CheckForUpdates(HWND hWnd, bool silent = false);
void ShowAboutDialog(HWND hWnd);

void ShowSettingsDialog();
void RenderSettingsDialog(HWND hDlg);
LRESULT CALLBACK SettingsWinProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

void ShowCropOverlay(HWND hCloneWnd);
void ResetCrop();
LRESULT CALLBACK CropOverlayWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define WM_BINDING_UPDATE (WM_APP + 1)
#define WM_CROP_CONFIRM (WM_APP + 2)
void StartCapture(KeyBinding* binding);
void CancelCapture();
void FinishCapture();
void ProcessCaptureKey(UINT vk, bool down);
void ApplyBindings();
LRESULT CALLBACK CaptureKeyHookProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CaptureMouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);
