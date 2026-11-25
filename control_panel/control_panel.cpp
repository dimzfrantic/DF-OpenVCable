#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "../src/mcvad_ioctl.h"

#pragma comment(lib, "comctl32.lib")

#define ID_TIMER_UPDATE  1001
#define ID_BTN_APPLY     1002
#define ID_BTN_RESET     1003
#define ID_CHK_ENABLE    1004
#define ID_TAB_CONTROL   1005

HWND g_hTab = NULL;
HWND g_hStatus = NULL;
HWND g_hComboRate = NULL;
HWND g_hComboBuffer = NULL;
HWND g_hChkEnable = NULL;
HWND g_hLblBufferFill = NULL;
HWND g_hLblDropouts = NULL;
HWND g_hLblActivity = NULL;
HWND g_hLblStatus = NULL;

HANDLE g_hDevice = INVALID_HANDLE_VALUE;
MCVAD_ALL_STATUS g_Status;
int g_CurrentCable = 0;

// TRUE = user sudah mengubah checkbox tapi belum menekan Apply.
// Selama TRUE, timer status TIDAK boleh menimpa pilihan user (bug: klik uncheck
// langsung berubah kembali karena UpdateCableUI menulis ulang dari driver).
bool g_bEnableDirty = false;

const ULONG g_SampleRates[] = { 44100, 48000, 96000, 192000 };
const ULONG g_BufferSizes[] = { 50, 100, 250, 500, 1000 };

void UpdateCableUI(HWND hWnd) {
    if (g_hDevice == INVALID_HANDLE_VALUE) return;

    DWORD bytesRet = 0;
    if (DeviceIoControl(g_hDevice, IOCTL_MCVAD_GET_CABLE_STATUS, NULL, 0, &g_Status, sizeof(g_Status), &bytesRet, NULL)) {
        MCVAD_CABLE_INFO* pCable = &g_Status.Cables[g_CurrentCable];

        // Update Checkbox Enable/Disable.
        // Kalau user sudah mengubah checkbox tapi belum Apply, pilihan user TIDAK
        // ditimpa oleh status driver (timer 500 ms).
        if (!g_bEnableDirty) {
            SendMessage(g_hChkEnable, BM_SETCHECK, pCable->IsEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        BOOL bCheckedNow = (SendMessage(g_hChkEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SetWindowText(g_hChkEnable, bCheckedNow ? L"Cable Enabled (Active)" : L"Cable Disabled (Muted)");

        // Update Status
        WCHAR szStatus[128];
        swprintf_s(szStatus, L"State: %s  |  Sample Rate: %u Hz  |  Buffer: %u ms (%u KB)",
            pCable->IsEnabled ? L"Active" : L"Disabled",
            pCable->SampleRate, pCable->BufferDurationMs, pCable->BufferSizeBytes / 1024);
        SetWindowText(g_hLblStatus, szStatus);

        // Update Buffer Fill
        WCHAR szFill[128];
        double fillPct = pCable->BufferSizeBytes > 0 ? ((double)pCable->CurrentBufferFill / pCable->BufferSizeBytes) * 100.0 : 0.0;
        swprintf_s(szFill, L"Buffer Usage: %u / %u bytes (%.1f%%)", pCable->CurrentBufferFill, pCable->BufferSizeBytes, fillPct);
        SetWindowText(g_hLblBufferFill, szFill);

        // Update Dropouts
        WCHAR szDrop[128];
        swprintf_s(szDrop, L"Overruns: %u  |  Underruns: %u", pCable->OverrunCount, pCable->UnderrunCount);
        SetWindowText(g_hLblDropouts, szDrop);

        // Update Activity
        WCHAR szAct[128];
        swprintf_s(szAct, L"Playback (Render): %s  |  Recording (Capture): %s",
            pCable->IsRenderActive ? L"Active (Streaming)" : L"Idle",
            pCable->IsCaptureActive ? L"Active (Listening)" : L"Idle");
        SetWindowText(g_hLblActivity, szAct);
    }
}

void ApplySettings(HWND hWnd) {
    if (g_hDevice == INVALID_HANDLE_VALUE) return;

    int rateIdx = (int)SendMessage(g_hComboRate, CB_GETCURSEL, 0, 0);
    int bufIdx = (int)SendMessage(g_hComboBuffer, CB_GETCURSEL, 0, 0);
    BOOL isChecked = (SendMessage(g_hChkEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);

    MCVAD_SET_CONFIG_REQ req;
    req.CableIndex = g_CurrentCable;
    req.SampleRate = (rateIdx >= 0 && rateIdx < 4) ? g_SampleRates[rateIdx] : 0;
    req.BufferDurationMs = (bufIdx >= 0 && bufIdx < 5) ? g_BufferSizes[bufIdx] : 0;
    req.EnableState = isChecked ? 1 : 0;

    DWORD bytesRet = 0;
    if (DeviceIoControl(g_hDevice, IOCTL_MCVAD_SET_CABLE_CONFIG, &req, sizeof(req), NULL, 0, &bytesRet, NULL)) {
        g_bEnableDirty = false;   // pilihan user sudah dikirim ke driver
        UpdateCableUI(hWnd);
        MessageBox(hWnd, L"Settings applied successfully.", L"Success", MB_OK | MB_ICONINFORMATION);
    } else {
        MessageBox(hWnd, L"Failed to apply settings to driver.", L"Error", MB_OK | MB_ICONERROR);
    }
}

void ResetStats(HWND hWnd) {
    if (g_hDevice == INVALID_HANDLE_VALUE) return;

    ULONG cableIdx = g_CurrentCable;
    DWORD bytesRet = 0;
    if (DeviceIoControl(g_hDevice, IOCTL_MCVAD_RESET_CABLE_STATS, &cableIdx, sizeof(cableIdx), NULL, 0, &bytesRet, NULL)) {
        UpdateCableUI(hWnd);
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        INITCOMMONCONTROLSEX icex;
        icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
        icex.dwICC = ICC_TAB_CLASSES;
        InitCommonControlsEx(&icex);

        // Buat Tab Control
        g_hTab = CreateWindow(WC_TABCONTROL, L"", WS_CHILD | WS_CLIPSIBLINGS | WS_VISIBLE,
            10, 10, 460, 30, hWnd, (HMENU)ID_TAB_CONTROL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);

        SendMessage(g_hTab, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

        TCITEM tie;
        tie.mask = TCIF_TEXT;
        WCHAR szTabs[5][16] = { L"Cable A", L"Cable B", L"Cable C", L"Cable D", L"Cable E" };
        for (int i = 0; i < 5; i++) {
            tie.pszText = szTabs[i];
            TabCtrl_InsertItem(g_hTab, i, &tie);
        }

        HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);

        // Groupbox Cable Settings
        HWND hGrpSettings = CreateWindow(L"BUTTON", L"Cable Configuration", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 50, 460, 110, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hGrpSettings, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Checkbox Enable/Disable
        g_hChkEnable = CreateWindow(L"BUTTON", L"Cable Enabled (Active)", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            25, 75, 200, 20, hWnd, (HMENU)ID_CHK_ENABLE, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hChkEnable, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hChkEnable, BM_SETCHECK, BST_CHECKED, 0);

        // Label & Combo Sample Rate
        HWND hLblRate = CreateWindow(L"STATIC", L"Sample Rate:", WS_CHILD | WS_VISIBLE,
            25, 105, 90, 20, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hLblRate, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hComboRate = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            120, 102, 110, 120, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hComboRate, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hComboRate, CB_ADDSTRING, 0, (LPARAM)L"44100 Hz");
        SendMessage(g_hComboRate, CB_ADDSTRING, 0, (LPARAM)L"48000 Hz");
        SendMessage(g_hComboRate, CB_ADDSTRING, 0, (LPARAM)L"96000 Hz");
        SendMessage(g_hComboRate, CB_ADDSTRING, 0, (LPARAM)L"192000 Hz");
        SendMessage(g_hComboRate, CB_SETCURSEL, 1, 0);

        // Label & Combo Buffer Size
        HWND hLblBuffer = CreateWindow(L"STATIC", L"Buffer Duration:", WS_CHILD | WS_VISIBLE,
            250, 105, 95, 20, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hLblBuffer, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hComboBuffer = CreateWindow(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
            350, 102, 105, 120, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hComboBuffer, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(g_hComboBuffer, CB_ADDSTRING, 0, (LPARAM)L"50 ms");
        SendMessage(g_hComboBuffer, CB_ADDSTRING, 0, (LPARAM)L"100 ms");
        SendMessage(g_hComboBuffer, CB_ADDSTRING, 0, (LPARAM)L"250 ms");
        SendMessage(g_hComboBuffer, CB_ADDSTRING, 0, (LPARAM)L"500 ms");
        SendMessage(g_hComboBuffer, CB_ADDSTRING, 0, (LPARAM)L"1000 ms");
        SendMessage(g_hComboBuffer, CB_SETCURSEL, 3, 0);

        // Tombol Apply
        HWND hBtnApply = CreateWindow(L"BUTTON", L"Apply Settings", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            250, 72, 120, 24, hWnd, (HMENU)ID_BTN_APPLY, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hBtnApply, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Tombol Reset Stats
        HWND hBtnReset = CreateWindow(L"BUTTON", L"Reset Stats", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            375, 72, 80, 24, hWnd, (HMENU)ID_BTN_RESET, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hBtnReset, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Groupbox Diagnostics
        HWND hGrpDiag = CreateWindow(L"BUTTON", L"Live Diagnostics & Metrics", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 170, 460, 130, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(hGrpDiag, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hLblStatus = CreateWindow(L"STATIC", L"State: Initializing...", WS_CHILD | WS_VISIBLE,
            25, 195, 430, 18, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hLblStatus, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hLblBufferFill = CreateWindow(L"STATIC", L"Buffer Usage: 0 / 0 bytes (0.0%)", WS_CHILD | WS_VISIBLE,
            25, 220, 430, 18, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hLblBufferFill, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hLblDropouts = CreateWindow(L"STATIC", L"Overruns: 0  |  Underruns: 0", WS_CHILD | WS_VISIBLE,
            25, 245, 430, 18, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hLblDropouts, WM_SETFONT, (WPARAM)hFont, TRUE);

        g_hLblActivity = CreateWindow(L"STATIC", L"Playback (Render): Idle  |  Recording (Capture): Idle", WS_CHILD | WS_VISIBLE,
            25, 270, 430, 18, hWnd, NULL, ((LPCREATESTRUCT)lParam)->hInstance, NULL);
        SendMessage(g_hLblActivity, WM_SETFONT, (WPARAM)hFont, TRUE);

        // Buka handle ke driver kontrol
        g_hDevice = CreateFile(MCVAD_USERMODE_DEVICE_PATH, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (g_hDevice == INVALID_HANDLE_VALUE) {
            SetWindowText(g_hLblStatus, L"State: Driver not detected (device handle unavailable)");
        } else {
            SetTimer(hWnd, ID_TIMER_UPDATE, 500, NULL);
            UpdateCableUI(hWnd);
        }
        break;
    }
    case WM_TIMER:
        if (wParam == ID_TIMER_UPDATE) {
            UpdateCableUI(hWnd);
        }
        break;
    case WM_NOTIFY: {
        LPNMHDR lpnmhdr = (LPNMHDR)lParam;
        if (lpnmhdr->idFrom == ID_TAB_CONTROL && lpnmhdr->code == TCN_SELCHANGE) {
            g_CurrentCable = TabCtrl_GetCurSel(g_hTab);
            g_bEnableDirty = false;   // pindah kabel: checkbox kembali mengikuti driver
            UpdateCableUI(hWnd);
        }
        break;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BTN_APPLY:
            ApplySettings(hWnd);
            break;
        case ID_BTN_RESET:
            ResetStats(hWnd);
            break;
        case ID_CHK_ENABLE:
            if (HIWORD(wParam) == BN_CLICKED) {
                // Tandai bahwa state checkbox sekarang milik user, bukan cermin driver.
                g_bEnableDirty = true;
                BOOL bChecked = (SendMessage(g_hChkEnable, BM_GETCHECK, 0, 0) == BST_CHECKED);
                SetWindowText(g_hChkEnable, bChecked ? L"Cable Enabled (Active)" : L"Cable Disabled (Muted)");
            }
            break;
        }
        break;
    case WM_DESTROY:
        KillTimer(hWnd, ID_TIMER_UPDATE);
        if (g_hDevice != INVALID_HANDLE_VALUE) {
            CloseHandle(g_hDevice);
            g_hDevice = INVALID_HANDLE_VALUE;
        }
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = { 0 };
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = L"MCVADControlPanelClass";

    if (!RegisterClassEx(&wc)) return 0;

    HWND hWnd = CreateWindow(wc.lpszClassName, L"DF-OpenVCable Control Panel",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 495, 350, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
