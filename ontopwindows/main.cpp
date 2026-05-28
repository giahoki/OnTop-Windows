#include "main.h"
#include "lang/ru.h"
#include "lang/en.h"
#include "lang/es.h"
#include "lang/uk.h"
#include "lang/fr.h"
#include "lang/de.h"
#include "lang/pl.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow) {
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
