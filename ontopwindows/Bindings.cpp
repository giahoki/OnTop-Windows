#include "main.h"

KeyBinding g_bindClickThrough;
KeyBinding g_bindResizeSlow;
KeyBinding g_bindResizeFast;
KeyBinding* g_capturingBinding = nullptr;
HHOOK g_hCaptureHook = nullptr;
HHOOK g_hCaptureMouseHook = nullptr;
UINT g_captureHeldMods = 0;
UINT g_captureAllMods = 0;
UINT g_captureLastVk = 0;

static std::wstring VKToName(UINT vk) {
    if (vk >= '0' && vk <= '9') return std::wstring(1, (wchar_t)vk);
    if (vk >= 'A' && vk <= 'Z') return std::wstring(1, (wchar_t)vk);
    switch (vk) {
        case VK_F1: case VK_F2: case VK_F3: case VK_F4: case VK_F5: case VK_F6:
        case VK_F7: case VK_F8: case VK_F9: case VK_F10: case VK_F11: case VK_F12:
            { wchar_t buf[8]; swprintf_s(buf, L"F%d", vk - VK_F1 + 1); return buf; }
        case VK_SPACE: return L"Space";
        case VK_RETURN: return L"Enter";
        case VK_ESCAPE: return L"Esc";
        case VK_TAB: return L"Tab";
        case VK_BACK: return L"Back";
        case VK_DELETE: return L"Del";
        case VK_INSERT: return L"Ins";
        case VK_HOME: return L"Home";
        case VK_END: return L"End";
        case VK_PRIOR: return L"PgUp";
        case VK_NEXT: return L"PgDn";
        case VK_LEFT: return L"Left";
        case VK_RIGHT: return L"Right";
        case VK_UP: return L"Up";
        case VK_DOWN: return L"Down";
        case VK_SNAPSHOT: return L"PrtSc";
        case VK_SCROLL: return L"ScrLk";
        case VK_PAUSE: return L"Pause";
        case VK_NUMLOCK: return L"NumLk";
        case VK_CAPITAL: return L"Caps";
        case VK_OEM_1: return L";";
        case VK_OEM_2: return L"/";
        case VK_OEM_3: return L"`";
        case VK_OEM_4: return L"[";
        case VK_OEM_5: return L"\\";
        case VK_OEM_6: return L"]";
        case VK_OEM_7: return L"'";
        case VK_OEM_COMMA: return L",";
        case VK_OEM_MINUS: return L"-";
        case VK_OEM_PERIOD: return L".";
        case VK_OEM_PLUS: return L"=";
        case VK_DIVIDE: return L"/";
        case VK_MULTIPLY: return L"*";
        case VK_SUBTRACT: return L"-";
        case VK_ADD: return L"+";
        case VK_DECIMAL: return L".";
        case VK_NUMPAD0: return L"Num0";
        case VK_NUMPAD1: return L"Num1";
        case VK_NUMPAD2: return L"Num2";
        case VK_NUMPAD3: return L"Num3";
        case VK_NUMPAD4: return L"Num4";
        case VK_NUMPAD5: return L"Num5";
        case VK_NUMPAD6: return L"Num6";
        case VK_NUMPAD7: return L"Num7";
        case VK_NUMPAD8: return L"Num8";
        case VK_NUMPAD9: return L"Num9";
        case MOUSE_BIND_MBUTTON: return L"MButton";
        case MOUSE_BIND_WHEELUP: return L"Wheel\u2191";
        case MOUSE_BIND_WHEELDOWN: return L"Wheel\u2193";
        case MOUSE_BIND_LBUTTON: return L"LButton";
    }
    wchar_t buf[16]; swprintf_s(buf, L"VK_%d", vk);
    return buf;
}

std::wstring KeyBinding::ToString() const {
    std::wstring s;
    if (modifiers & MOD_CONTROL) s += L"Ctrl+";
    if (modifiers & MOD_ALT) s += L"Alt+";
    if (modifiers & MOD_SHIFT) s += L"Shift+";
    if (modifiers & MOD_WIN) s += L"Win+";
    if (vk > 0) {
        s += VKToName(vk);
    } else if (vk == 0) {
        s += L"Wheel";
    } else {
        if (s.size() >= 2) s.pop_back();
        if (s.empty()) s = L"\u2014";
    }
    return s;
}

void KeyBinding::FromString(const std::wstring& str) {
    modifiers = 0; vk = 0;
    size_t pos = str.rfind(L':');
    if (pos != std::wstring::npos && pos + 1 < str.size()) {
        if (pos > 0 && iswdigit(str[0])) {
            modifiers = (UINT)_wtoi(str.c_str());
            vk = (UINT)_wtoi(str.c_str() + pos + 1);
            return;
        }
    }
    if (str.find(L"Ctrl+") != std::wstring::npos) modifiers |= MOD_CONTROL;
    if (str.find(L"Alt+") != std::wstring::npos) modifiers |= MOD_ALT;
    if (str.find(L"Shift+") != std::wstring::npos) modifiers |= MOD_SHIFT;
    if (str.find(L"Win+") != std::wstring::npos) modifiers |= MOD_WIN;
    size_t last = str.rfind(L'+');
    if (last == std::wstring::npos) return;
    std::wstring key = str.substr(last + 1);
    if (key.empty() || key == L"\u2014") { vk = 0; return; }
    if (key.size() == 1 && key[0] >= 'A' && key[0] <= 'Z') { vk = key[0]; return; }
    if (key.size() == 1 && key[0] >= '0' && key[0] <= '9') { vk = key[0]; return; }
    if (key.size() > 1 && key[0] == L'F') {
        int n = _wtoi(key.c_str() + 1);
        if (n >= 1 && n <= 12) { vk = VK_F1 + n - 1; return; }
    }
    struct { const wchar_t* name; UINT vk; } tbl[] = {
        {L"Space", VK_SPACE}, {L"Enter", VK_RETURN}, {L"Esc", VK_ESCAPE},
        {L"Tab", VK_TAB}, {L"Back", VK_BACK}, {L"Del", VK_DELETE},
        {L"Ins", VK_INSERT}, {L"Home", VK_HOME}, {L"End", VK_END},
        {L"PgUp", VK_PRIOR}, {L"PgDn", VK_NEXT},
        {L"Left", VK_LEFT}, {L"Right", VK_RIGHT}, {L"Up", VK_UP}, {L"Down", VK_DOWN},
        {L"PrtSc", VK_SNAPSHOT}, {L"ScrLk", VK_SCROLL}, {L"Pause", VK_PAUSE},
        {L"NumLk", VK_NUMLOCK}, {L"Caps", VK_CAPITAL},
        {L"Num0", VK_NUMPAD0}, {L"Num1", VK_NUMPAD1}, {L"Num2", VK_NUMPAD2},
        {L"Num3", VK_NUMPAD3}, {L"Num4", VK_NUMPAD4}, {L"Num5", VK_NUMPAD5},
        {L"Num6", VK_NUMPAD6}, {L"Num7", VK_NUMPAD7}, {L"Num8", VK_NUMPAD8},
        {L"Num9", VK_NUMPAD9},
        {L";", VK_OEM_1}, {L"/", VK_OEM_2}, {L"`", VK_OEM_3},
        {L"[", VK_OEM_4}, {L"\\", VK_OEM_5}, {L"]", VK_OEM_6}, {L"'", VK_OEM_7},
        {L",", VK_OEM_COMMA}, {L"-", VK_OEM_MINUS}, {L".", VK_OEM_PERIOD}, {L"=", VK_OEM_PLUS},
        {L"*", VK_MULTIPLY}, {L"+", VK_ADD},
        {L"MButton", MOUSE_BIND_MBUTTON},
        {L"WheelUp", MOUSE_BIND_WHEELUP},
        {L"WheelDown", MOUSE_BIND_WHEELDOWN},
        {L"Wheel", 0},
        {L"LButton", MOUSE_BIND_LBUTTON},
    };
    for (const auto& e : tbl) {
        if (key == e.name) { vk = e.vk; return; }
    }
}

void FinishCapture() {
    g_capturingBinding = nullptr;
    g_captureHeldMods = 0;
    g_captureAllMods = 0;
    if (g_hSettingsDlg) PostMessage(g_hSettingsDlg, WM_BINDING_UPDATE, 0, 0);
    if (g_hCaptureMouseHook) { UnhookWindowsHookEx(g_hCaptureMouseHook); g_hCaptureMouseHook = nullptr; }
    SaveSettings();
    ApplyBindings();
}

void CancelCapture() {
    g_capturingBinding = nullptr;
    g_captureHeldMods = 0;
    g_captureAllMods = 0;
    if (g_hSettingsDlg) PostMessage(g_hSettingsDlg, WM_BINDING_UPDATE, 0, 0);
    if (g_hCaptureMouseHook) { UnhookWindowsHookEx(g_hCaptureMouseHook); g_hCaptureMouseHook = nullptr; }
}

void ProcessCaptureKey(UINT vk, bool down) {
    if (!g_capturingBinding) return;
    g_captureLastVk = vk;

    UINT modFlags = 0;
    if (vk == VK_CONTROL || vk == VK_LCONTROL || vk == VK_RCONTROL) modFlags = MOD_CONTROL;
    else if (vk == VK_MENU || vk == VK_LMENU || vk == VK_RMENU) modFlags = MOD_ALT;
    else if (vk == VK_SHIFT || vk == VK_LSHIFT || vk == VK_RSHIFT) modFlags = MOD_SHIFT;
    else if (vk == VK_LWIN || vk == VK_RWIN) modFlags = MOD_WIN;

    bool isMod = (modFlags != 0);

    if (isMod) {
        if (down) {
            g_captureHeldMods |= modFlags;
            g_captureAllMods |= modFlags;
        } else {
            g_captureHeldMods &= ~modFlags;
            if (g_capturingBinding->IsMouseBased() && g_captureHeldMods == 0 && g_captureAllMods != 0) {
                g_capturingBinding->modifiers = g_captureAllMods;
                FinishCapture();
            }
        }
        if (g_hSettingsDlg) PostMessage(g_hSettingsDlg, WM_BINDING_UPDATE, 0, 0);
        return;
    }

    if (g_capturingBinding->IsMouseBased()) {
        return;
    }

    if (down) {
        g_capturingBinding->modifiers = g_captureAllMods;
        g_capturingBinding->vk = vk;
    } else if (g_capturingBinding->vk == vk) {
        FinishCapture();
    }
    if (g_hSettingsDlg) PostMessage(g_hSettingsDlg, WM_BINDING_UPDATE, 0, 0);
}

LRESULT CALLBACK CaptureMouseHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_capturingBinding) {
        if (wParam == WM_LBUTTONDOWN) {
            g_capturingBinding->modifiers = g_captureAllMods;
            g_capturingBinding->vk = MOUSE_BIND_LBUTTON;
            FinishCapture();
            return 1;
        }
        if (wParam == WM_MBUTTONDOWN) {
            g_capturingBinding->modifiers = g_captureAllMods;
            g_capturingBinding->vk = MOUSE_BIND_MBUTTON;
            FinishCapture();
            return 1;
        }
        if (wParam == WM_MOUSEWHEEL) {
            g_capturingBinding->modifiers = g_captureAllMods;
            g_capturingBinding->vk = 0;
            FinishCapture();
            return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK CaptureKeyHookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && g_capturingBinding &&
        (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN || wParam == WM_KEYUP || wParam == WM_SYSKEYUP)) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
        ProcessCaptureKey((UINT)p->vkCode, wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        return 1;
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void StartCapture(KeyBinding* binding) {
    if (!binding || g_capturingBinding) return;
    g_captureHeldMods = 0;
    g_captureAllMods = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) g_captureAllMods |= MOD_CONTROL;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) g_captureAllMods |= MOD_ALT;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) g_captureAllMods |= MOD_SHIFT;
    if ((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) g_captureAllMods |= MOD_WIN;
    g_capturingBinding = binding;
    if (g_hSettingsDlg) PostMessage(g_hSettingsDlg, WM_BINDING_UPDATE, 0, 0);
    if (!g_hCaptureMouseHook) {
        g_hCaptureMouseHook = SetWindowsHookExW(WH_MOUSE_LL, CaptureMouseHookProc, GetModuleHandleW(nullptr), 0);
    }
}

void ApplyBindings() {
    if (g_hCloneWnd && IsWindow(g_hCloneWnd)) {
        bool isMouse = (g_bindClickThrough.vk == 0 || g_bindClickThrough.vk >= MOUSE_BIND_MBUTTON);
        UnregisterHotKey(g_hCloneWnd, 1);
        if (!isMouse) {
            RegisterHotKey(g_hCloneWnd, 1, g_bindClickThrough.modifiers, g_bindClickThrough.vk);
        }
    }
}
