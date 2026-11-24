#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include "../src/mcvad_ioctl.h"

#pragma comment(lib, "comctl32.lib")

// Identifikasi Control ID Win32
#define IDC_TAB_CONTROL         1001
#define IDC_COMBO_SAMPLE_RATE   1002
#define IDC_COMBO_BUFFER_SIZE   1003
#define IDC_BTN_APPLY           1004
#define IDC_BTN_RESET_STATS     1005
#define IDC_PROGRESS_BUFFER     1006
#define IDC_STATIC_STATUS       1007
#define IDC_STATIC_GLITCH       1008

static HWND g_hTab = NULL;
static HWND g_hComboSR = NULL;
static HWND g_hComboBuf = NULL;
static HWND g_hProgress = NULL;
static HWND g_hStatusText = NULL;
static HWND g_hGlitchText = NULL;
static HANDLE g_hDevice = INVALID_HANDLE_VALUE;
static int g_CurrentCableIndex = 0;

const WCHAR* g_TabLabels[] = { L"Cable 1", L"Cable A", L"Cable B", L"Cable C", L"Cable D" };
const WCHAR* g_SampleRates[] = { L"48000 Hz (Default)", L"44100 Hz (CD)", L"88200 Hz", L"96000 Hz (Hi-Res)", L"192000 Hz" };
const WCHAR* g_BufferSizes[] = { L"500 ms (Default)", L"50 ms (Ultra Low)", L"100 ms", L"250 ms", L"1000 ms (Safe)", L"2000 ms" };

void ApplyCurrentSettings(HWND hWnd) {
    int srSel = (int)SendMessage(g_hComboSR, CB_GETCURSEL, 0, 0);
    int bufSel = (int)SendMessage(g_hComboBuf, CB_GETCURSEL, 0, 0);

    ULONG sr = 48000;
    if (srSel == 0) sr = 44100;
    else if (srSel == 1) sr = 48000;
    else if (srSel == 2) sr = 88200;
    else if (srSel == 3) sr = 96000;
    else if (srSel == 4) sr = 192000;

    ULONG bufMs = 500;
    if (bufSel == 0) bufMs = 50;
    else if (bufSel == 1) bufMs = 100;
    else if (bufSel == 2) bufMs = 250;
    else if (bufSel == 3) bufMs = 500;
    else if (bufSel == 4) bufMs = 1000;
    else if (bufSel == 5) bufMs = 2000;

    if (g_hDevice != INVALID_HANDLE_VALUE) {
        MCVAD_SET_CONFIG_REQ req;
        req.CableIndex = g_CurrentCableIndex;
        req.SampleRate = sr;
        req.BufferDurationMs = bufMs;
        DWORD bytesRet = 0;
        BOOL bOk = DeviceIoControl(g_hDevice, IOCTL_MCVAD_SET_CABLE_CONFIG, &req, sizeof(req), NULL, 0, &bytesRet, NULL);
        if (bOk) {
            MessageBox(hWnd, L"Pengaturan berhasil dikirim ke driver kernel.", L"MCVAD Control Panel", MB_ICONINFORMATION);
        } else {
            MessageBox(hWnd, L"Gagal mengirim IOCTL ke kernel driver.", L"Error", MB_ICONERROR);
        }
    } else {
        MessageBox(hWnd, L"Mode Desain/Offline: Nilai parameter disimpan.", L"MCVAD Control Panel", MB_ICONINFORMATION);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hDevice = CreateFileW(MCVAD_USERMODE_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);

            // Tab Control
            g_hTab = CreateWindowEx(0, WC_TABCONTROL, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                15, 45, 600, 320, hWnd, (HMENU)IDC_TAB_CONTROL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

            TCITEM tie;
            tie.mask = TCIF_TEXT;
            for (int i = 0; i < 5; i++) {
                tie.pszText = (LPWSTR)g_TabLabels[i];
                TabCtrl_InsertItem(g_hTab, i, &tie);
            }

            // Status text
            CreateWindow(L"STATIC", L"Sample Rate:", WS_CHILD | WS_VISIBLE, 40, 95, 120, 20, hWnd, NULL, NULL, NULL);
            g_hComboSR = CreateWindow(WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 92, 160, 200, hWnd, (HMENU)IDC_COMBO_SAMPLE_RATE, NULL, NULL);
            for (int i = 0; i < 5; i++) SendMessage(g_hComboSR, CB_ADDSTRING, 0, (LPARAM)g_SampleRates[i]);
            SendMessage(g_hComboSR, CB_SETCURSEL, 1, 0); // 48000 Hz default

            CreateWindow(L"STATIC", L"Buffer Latency:", WS_CHILD | WS_VISIBLE, 40, 135, 120, 20, hWnd, NULL, NULL, NULL);
            g_hComboBuf = CreateWindow(WC_COMBOBOX, L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST, 180, 132, 160, 200, hWnd, (HMENU)IDC_COMBO_BUFFER_SIZE, NULL, NULL);
            for (int i = 0; i < 6; i++) SendMessage(g_hComboBuf, CB_ADDSTRING, 0, (LPARAM)g_BufferSizes[i]);
            SendMessage(g_hComboBuf, CB_SETCURSEL, 3, 0); // 500 ms default

            CreateWindow(L"STATIC", L"Buffer Load:", WS_CHILD | WS_VISIBLE, 40, 185, 120, 20, hWnd, NULL, NULL, NULL);
            g_hProgress = CreateWindow(PROGRESS_CLASS, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 180, 185, 250, 18, hWnd, (HMENU)IDC_PROGRESS_BUFFER, NULL, NULL);
            SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            SendMessage(g_hProgress, PBM_SETPOS, 25, 0);

            g_hGlitchText = CreateWindow(L"STATIC", L"Glitch Status: 0 Overruns / 0 Underruns", WS_CHILD | WS_VISIBLE, 40, 225, 350, 20, hWnd, (HMENU)IDC_STATIC_GLITCH, NULL, NULL);

            CreateWindow(L"BUTTON", L"Apply Configuration", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 380, 290, 160, 30, hWnd, (HMENU)IDC_BTN_APPLY, NULL, NULL);
            break;
        }
        case WM_COMMAND: {
            if (LOWORD(wParam) == IDC_BTN_APPLY) {
                ApplyCurrentSettings(hWnd);
            }
            break;
        }
        case WM_NOTIFY: {
            LPNMHDR pnm = (LPNMHDR)lParam;
            if (pnm->idFrom == IDC_TAB_CONTROL && pnm->code == TCN_SELCHANGE) {
                g_CurrentCableIndex = TabCtrl_GetCurSel(g_hTab);
            }
            break;
        }
        case WM_DESTROY: {
            if (g_hDevice != INVALID_HANDLE_VALUE) CloseHandle(g_hDevice);
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
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

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MCVADControlPanelClass";
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClass(&wc);

    HWND hWnd = CreateWindow(L"MCVADControlPanelClass", L"MCVAD Control Panel - Multi-Cable Virtual Audio",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 650, 420, NULL, NULL, hInstance, NULL);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
