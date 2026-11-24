#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <stdio.h>
#include "../src/mcvad_ioctl.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "uxtheme.lib")

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

// Control IDs
#define IDC_TAB_CONTROL         1001
#define IDC_COMBO_SAMPLE_RATE   1002
#define IDC_COMBO_BUFFER_SIZE   1003
#define IDC_BTN_APPLY           1004
#define IDC_BTN_RESET_STATS     1005
#define IDC_PROGRESS_BUFFER     1006
#define IDC_STATIC_STATUS       1007
#define IDC_STATIC_GLITCH       1008
#define IDC_STATIC_CABLE_TITLE  1009
#define IDC_STATIC_ACTIVITY     1010
#define IDT_TIMER_REFRESH       2001

static HWND g_hWnd = NULL;
static HWND g_hTab = NULL;
static HWND g_hComboSR = NULL;
static HWND g_hComboBuf = NULL;
static HWND g_hProgress = NULL;
static HWND g_hCableTitle = NULL;
static HWND g_hGlitchText = NULL;
static HWND g_hActivityText = NULL;
static HWND g_hDriverStatus = NULL;

static HBRUSH g_hBrushBg = NULL;
static HBRUSH g_hBrushCard = NULL;
static HFONT  g_hFontNormal = NULL;
static HFONT  g_hFontBold = NULL;
static HFONT  g_hFontHeader = NULL;

static HANDLE g_hDevice = INVALID_HANDLE_VALUE;
static int    g_CurrentCableIndex = 0;
static MCVAD_ALL_STATUS g_LastStatus = { 0 };

const WCHAR* g_TabLabels[] = { L"Cable A", L"Cable B", L"Cable C", L"Cable D", L"Cable E" };
const WCHAR* g_SampleRates[] = { L"44100 Hz (CD Audio)", L"48000 Hz (Standard / Recommended)", L"88200 Hz (High-Res)", L"96000 Hz (Professional)", L"192000 Hz (Studio Master)" };
const ULONG  g_SampleRateValues[] = { 44100, 48000, 88200, 96000, 192000 };

const WCHAR* g_BufferSizes[] = { L"50 ms (Ultra Low Latency)", L"100 ms (Low Latency)", L"250 ms (Balanced)", L"500 ms (Standard)", L"1000 ms (Safe / Broadcast)", L"2000 ms (Maximum Stability)" };
const ULONG  g_BufferMsValues[] = { 50, 100, 250, 500, 1000, 2000 };

void RefreshDeviceStatus() {
    if (g_hDevice == INVALID_HANDLE_VALUE) {
        g_hDevice = CreateFileW(MCVAD_USERMODE_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
    }

    if (g_hDevice != INVALID_HANDLE_VALUE) {
        DWORD bytesRet = 0;
        MCVAD_ALL_STATUS status;
        if (DeviceIoControl(g_hDevice, IOCTL_MCVAD_GET_CABLE_STATUS, NULL, 0, &status, sizeof(status), &bytesRet, NULL) && bytesRet >= sizeof(status)) {
            g_LastStatus = status;
            SetWindowTextW(g_hDriverStatus, L"Kernel Driver: Connected (\Device\MCVAD_Control)");
            
            if (g_CurrentCableIndex >= 0 && g_CurrentCableIndex < 5) {
                MCVAD_CABLE_INFO* c = &g_LastStatus.Cables[g_CurrentCableIndex];
                WCHAR bufGlitch[128];
                wsprintfW(bufGlitch, L"Buffer Health: %u Overruns | %u Underruns (Buffer: %u ms)", c->OverrunCount, c->UnderrunCount, c->BufferDurationMs);
                SetWindowTextW(g_hGlitchText, bufGlitch);

                WCHAR bufAct[128];
                wsprintfW(bufAct, L"Stream Status: Render [%s]  |  Capture [%s]", c->IsRenderActive ? L"ACTIVE" : L"IDLE", c->IsCaptureActive ? L"ACTIVE" : L"IDLE");
                SetWindowTextW(g_hActivityText, bufAct);

                if (c->BufferSizeBytes > 0) {
                    int percent = (int)((c->CurrentBufferFill * 100ULL) / c->BufferSizeBytes);
                    if (percent > 100) percent = 100;
                    SendMessage(g_hProgress, PBM_SETPOS, percent, 0);
                } else {
                    SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
                }
            }
            return;
        }
    }

    SetWindowTextW(g_hDriverStatus, L"Kernel Driver: Standby / Offline (Ready for test)");
    SetWindowTextW(g_hGlitchText, L"Buffer Health: Ready (Settings will apply on driver load)");
    SetWindowTextW(g_hActivityText, L"Stream Status: Standby");
    SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
}

void ApplyCurrentSettings(HWND hWnd) {
    int srSel = (int)SendMessage(g_hComboSR, CB_GETCURSEL, 0, 0);
    int bufSel = (int)SendMessage(g_hComboBuf, CB_GETCURSEL, 0, 0);

    ULONG sr = (srSel >= 0 && srSel < 5) ? g_SampleRateValues[srSel] : 48000;
    ULONG bufMs = (bufSel >= 0 && bufSel < 6) ? g_BufferMsValues[bufSel] : 500;

    if (g_hDevice != INVALID_HANDLE_VALUE) {
        MCVAD_SET_CONFIG_REQ req;
        req.CableIndex = g_CurrentCableIndex;
        req.SampleRate = sr;
        req.BufferDurationMs = bufMs;
        DWORD bytesRet = 0;
        BOOL bOk = DeviceIoControl(g_hDevice, IOCTL_MCVAD_SET_CABLE_CONFIG, &req, sizeof(req), NULL, 0, &bytesRet, NULL);
        if (bOk) {
            WCHAR msg[128];
            wsprintfW(msg, L"Pengaturan Cable %c berhasil diterapkan:\nSample Rate: %u Hz\nBuffer Latency: %u ms", 'A' + g_CurrentCableIndex, sr, bufMs);
            MessageBoxW(hWnd, msg, L"DF-OpenVCable", MB_ICONINFORMATION);
            RefreshDeviceStatus();
        } else {
            MessageBoxW(hWnd, L"Gagal mengirim IOCTL ke driver kernel.", L"DF-OpenVCable Error", MB_ICONERROR);
        }
    } else {
        WCHAR msg[128];
        wsprintfW(msg, L"Mode Offline / Standby:\nPengaturan Cable %c disimpan ke profil:\nSample Rate: %u Hz\nBuffer Latency: %u ms", 'A' + g_CurrentCableIndex, sr, bufMs);
        MessageBoxW(hWnd, msg, L"DF-OpenVCable", MB_ICONINFORMATION);
    }
}

void ResetStats(HWND hWnd) {
    if (g_hDevice != INVALID_HANDLE_VALUE) {
        ULONG idx = (ULONG)g_CurrentCableIndex;
        DWORD bytesRet = 0;
        if (DeviceIoControl(g_hDevice, IOCTL_MCVAD_RESET_CABLE_STATS, &idx, sizeof(idx), NULL, 0, &bytesRet, NULL)) {
            MessageBoxW(hWnd, L"Statistik buffer/glitch berhasil di-reset ke nol.", L"DF-OpenVCable", MB_ICONINFORMATION);
            RefreshDeviceStatus();
            return;
        }
    }
    MessageBoxW(hWnd, L"Driver offline. Statistik lokal di-reset.", L"DF-OpenVCable", MB_ICONINFORMATION);
}

void UpdateCableView() {
    WCHAR titleBuf[128];
    wsprintfW(titleBuf, L"Virtual Audio Cable %c (Playback: DF-OpenVCable %c | Recording: DF-OpenVCable %c)", 'A' + g_CurrentCableIndex, 'A' + g_CurrentCableIndex, 'A' + g_CurrentCableIndex);
    SetWindowTextW(g_hCableTitle, titleBuf);
    RefreshDeviceStatus();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hWnd = hWnd;

            // Windows 10/11 Immersive Dark Mode
            BOOL darkMode = TRUE;
            DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));

            g_hBrushBg = CreateSolidBrush(RGB(24, 24, 27));
            g_hBrushCard = CreateSolidBrush(RGB(39, 39, 42));

            g_hFontNormal = CreateFontW(15, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontBold   = CreateFontW(15, 0, 0, 0, FW_BOLD,   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontHeader = CreateFontW(19, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            HWND hHead = CreateWindowW(L"STATIC", L"DF-OpenVCable Virtual Audio Router", WS_CHILD | WS_VISIBLE, 24, 16, 420, 26, hWnd, NULL, NULL, NULL);
            SendMessageW(hHead, WM_SETFONT, (WPARAM)g_hFontHeader, TRUE);

            g_hDriverStatus = CreateWindowW(L"STATIC", L"Kernel Driver: Initializing...", WS_CHILD | WS_VISIBLE, 24, 46, 520, 18, hWnd, (HMENU)IDC_STATIC_STATUS, NULL, NULL);
            SendMessageW(g_hDriverStatus, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hTab = CreateWindowExW(0, WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 20, 75, 640, 370, hWnd, (HMENU)IDC_TAB_CONTROL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
            SendMessageW(g_hTab, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            TCITEMW tie;
            tie.mask = TCIF_TEXT;
            for (int i = 0; i < 5; i++) {
                tie.pszText = (LPWSTR)g_TabLabels[i];
                TabCtrl_InsertItem(g_hTab, i, &tie);
            }

            g_hCableTitle = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 40, 115, 600, 22, hWnd, (HMENU)IDC_STATIC_CABLE_TITLE, NULL, NULL);
            SendMessageW(g_hCableTitle, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            HWND hLblSR = CreateWindowW(L"STATIC", L"Sample Rate:", WS_CHILD | WS_VISIBLE, 40, 155, 140, 20, hWnd, NULL, NULL, NULL);
            SendMessageW(hLblSR, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            g_hComboSR = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 190, 152, 280, 220, hWnd, (HMENU)IDC_COMBO_SAMPLE_RATE, NULL, NULL);
            SendMessageW(g_hComboSR, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            for (int i = 0; i < 5; i++) SendMessageW(g_hComboSR, CB_ADDSTRING, 0, (LPARAM)g_SampleRates[i]);
            SendMessageW(g_hComboSR, CB_SETCURSEL, 1, 0);

            HWND hLblBuf = CreateWindowW(L"STATIC", L"Buffer Latency:", WS_CHILD | WS_VISIBLE, 40, 200, 140, 20, hWnd, NULL, NULL, NULL);
            SendMessageW(hLblBuf, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            g_hComboBuf = CreateWindowW(WC_COMBOBOXW, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL, 190, 197, 280, 220, hWnd, (HMENU)IDC_COMBO_BUFFER_SIZE, NULL, NULL);
            SendMessageW(g_hComboBuf, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            for (int i = 0; i < 6; i++) SendMessageW(g_hComboBuf, CB_ADDSTRING, 0, (LPARAM)g_BufferSizes[i]);
            SendMessageW(g_hComboBuf, CB_SETCURSEL, 3, 0);

            HWND hLblLoad = CreateWindowW(L"STATIC", L"Ring Buffer Fill:", WS_CHILD | WS_VISIBLE, 40, 245, 140, 20, hWnd, NULL, NULL, NULL);
            SendMessageW(hLblLoad, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
            g_hProgress = CreateWindowW(PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 190, 247, 430, 18, hWnd, (HMENU)IDC_PROGRESS_BUFFER, NULL, NULL);
            SendMessageW(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

            g_hGlitchText = CreateWindowW(L"STATIC", L"Buffer Health: Initializing...", WS_CHILD | WS_VISIBLE, 40, 285, 480, 20, hWnd, (HMENU)IDC_STATIC_GLITCH, NULL, NULL);
            SendMessageW(g_hGlitchText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hActivityText = CreateWindowW(L"STATIC", L"Stream Status: Initializing...", WS_CHILD | WS_VISIBLE, 40, 315, 480, 20, hWnd, (HMENU)IDC_STATIC_ACTIVITY, NULL, NULL);
            SendMessageW(g_hActivityText, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            HWND hBtnReset = CreateWindowW(L"BUTTON", L"Reset Glitch Stats", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 190, 360, 150, 32, hWnd, (HMENU)IDC_BTN_RESET_STATS, NULL, NULL);
            SendMessageW(hBtnReset, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            HWND hBtnApply = CreateWindowW(L"BUTTON", L"Apply Configuration", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 460, 360, 160, 32, hWnd, (HMENU)IDC_BTN_APPLY, NULL, NULL);
            SendMessageW(hBtnApply, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            UpdateCableView();
            SetTimer(hWnd, IDT_TIMER_REFRESH, 1000, NULL);
            break;
        }
        case WM_TIMER: {
            if (wParam == IDT_TIMER_REFRESH) {
                RefreshDeviceStatus();
            }
            break;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORDLG: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(244, 244, 245));
            return (LRESULT)g_hBrushBg;
        }
        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, RGB(244, 244, 245));
            return (LRESULT)g_hBrushCard;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            if (wmId == IDC_BTN_APPLY) {
                ApplyCurrentSettings(hWnd);
            } else if (wmId == IDC_BTN_RESET_STATS) {
                ResetStats(hWnd);
            }
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm->idFrom == IDC_TAB_CONTROL && pnm->code == TCN_SELCHANGE) {
                g_CurrentCableIndex = TabCtrl_GetCurSel(g_hTab);
                UpdateCableView();
            }
            break;
        }
        case WM_DESTROY: {
            KillTimer(hWnd, IDT_TIMER_REFRESH);
            if (g_hDevice != INVALID_HANDLE_VALUE) {
                CloseHandle(g_hDevice);
                g_hDevice = INVALID_HANDLE_VALUE;
            }
            if (g_hBrushBg)   DeleteObject(g_hBrushBg);
            if (g_hBrushCard) DeleteObject(g_hBrushCard);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            if (g_hFontBold)   DeleteObject(g_hFontBold);
            if (g_hFontHeader) DeleteObject(g_hFontHeader);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_TAB_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icex);

    WNDCLASSW wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DFOpenVCableControlPanelClass";
    wc.hbrBackground = CreateSolidBrush(RGB(24, 24, 27));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hWnd = CreateWindowW(L"DFOpenVCableControlPanelClass", L"DF-OpenVCable Control Panel - Multi-Cable Virtual Audio Router",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 510, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
