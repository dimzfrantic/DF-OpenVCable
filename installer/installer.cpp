#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <string>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#define ID_BTN_INSTALL    2001
#define ID_BTN_UNINSTALL  2002
#define ID_BTN_RESTART    2003
#define ID_BTN_CLOSE      2004

HWND g_hMainWnd = NULL;
HWND g_hLblTitle = NULL;
HWND g_hLblSecureBoot = NULL;
HWND g_hLblTestMode = NULL;
HWND g_hLblDriverStatus = NULL;
HWND g_hLblLog = NULL;
HWND g_hBtnInstall = NULL;
HWND g_hBtnUninstall = NULL;
HWND g_hBtnRestart = NULL;
HWND g_hBtnClose = NULL;

HFONT g_hFontTitle = NULL;
HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;

bool g_bSecureBootActive = false;
bool g_bTestModeActive = false;
bool g_bDriverInstalled = false;

// Helper: Jalankan perintah secara tersembunyi (tanpa konsol hitam berkedip)
DWORD RunHiddenCommand(const std::wstring& cmd, std::wstring* pOutput = NULL) {
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    HANDLE hRead = NULL, hWrite = NULL;
    if (pOutput) {
        CreatePipe(&hRead, &hWrite, &sa, 0);
        SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (pOutput) {
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdOutput = hWrite;
        si.hStdError = hWrite;
    }

    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdCopy = cmd;
    if (!CreateProcessW(NULL, &cmdCopy[0], NULL, NULL, pOutput ? TRUE : FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        if (hWrite) CloseHandle(hWrite);
        if (hRead) CloseHandle(hRead);
        return (DWORD)-1;
    }

    if (hWrite) {
        CloseHandle(hWrite);
        hWrite = NULL;
    }

    if (pOutput && hRead) {
        char buffer[1024];
        DWORD bytesRead = 0;
        std::string rawOutput;
        while (ReadFile(hRead, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            rawOutput += buffer;
        }
        CloseHandle(hRead);
        int len = MultiByteToWideChar(CP_OEMCP, 0, rawOutput.c_str(), -1, NULL, 0);
        if (len > 0) {
            pOutput->resize(len - 1);
            MultiByteToWideChar(CP_OEMCP, 0, rawOutput.c_str(), -1, &(*pOutput)[0], len);
        }
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

// Deteksi status Secure Boot via Registry
bool CheckSecureBoot() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\SecureBoot\\State", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD enabled = 0;
        DWORD size = sizeof(enabled);
        if (RegQueryValueExW(hKey, L"UEFISecureBootEnabled", NULL, NULL, (LPBYTE)&enabled, &size) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return (enabled == 1);
        }
        RegCloseKey(hKey);
    }
    return false;
}

// Deteksi status Test Signing
bool CheckTestSigning() {
    std::wstring out;
    RunHiddenCommand(L"bcdedit.exe", &out);
    return (out.find(L"testsigning             Yes") != std::wstring::npos ||
            out.find(L"testsigning             yes") != std::wstring::npos ||
            out.find(L"testsigning = Yes") != std::wstring::npos);
}

// Deteksi apakah driver sudah terdaftar di sistem
bool CheckDriverInstalled() {
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\mcvad", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

// Ambil path folder tempat installer berada
std::wstring GetAppDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    wchar_t* lastSlash = wcsrchr(path, L'\\');
    if (lastSlash) *(lastSlash + 1) = L'\0';
    return std::wstring(path);
}

void RefreshSystemStatus() {
    g_bSecureBootActive = CheckSecureBoot();
    g_bTestModeActive = CheckTestSigning();
    g_bDriverInstalled = CheckDriverInstalled();

    if (g_bSecureBootActive) {
        SetWindowTextW(g_hLblSecureBoot, L"Secure Boot: [AKTIF] - Wajib dimatikan di BIOS sebelum instalasi!");
    } else {
        SetWindowTextW(g_hLblSecureBoot, L"Secure Boot: [NONAKTIF] - Aman untuk memuat driver uji coba.");
    }

    if (g_bTestModeActive) {
        SetWindowTextW(g_hLblTestMode, L"Test Signing: [AKTIF] - Windows siap menjalankan driver.");
    } else {
        SetWindowTextW(g_hLblTestMode, L"Test Signing: [NONAKTIF] - Akan diaktifkan otomatis oleh installer.");
    }

    if (g_bDriverInstalled) {
        SetWindowTextW(g_hLblDriverStatus, L"Status Driver: [TERPASANG] - 5 Pasang DF-OpenVCable terdaftar.");
        SetWindowTextW(g_hBtnInstall, L"Perbarui / Pasang Ulang");
        EnableWindow(g_hBtnUninstall, TRUE);
    } else {
        SetWindowTextW(g_hLblDriverStatus, L"Status Driver: [BELUM TERPASANG]");
        SetWindowTextW(g_hBtnInstall, L"Pasang DF-OpenVCable");
        EnableWindow(g_hBtnUninstall, FALSE);
    }
}

void AppendLog(const std::wstring& msg) {
    SetWindowTextW(g_hLblLog, msg.c_str());
    UpdateWindow(g_hLblLog);
}

void DoInstall() {
    std::wstring appDir = GetAppDir();

    // 1. Cek Secure Boot
    if (g_bSecureBootActive) {
        int res = MessageBoxW(g_hMainWnd,
            L"PERINGATAN: Fitur Secure Boot di BIOS terdeteksi AKTIF!\n\n"
            L"Windows tidak mengizinkan pemuatan driver kernel uji coba (test-signed) selama Secure Boot menyala.\n\n"
            L"Apakah Anda ingin tetap melanjutkan? (Disarankan untuk restart ke BIOS dan matikan Secure Boot terlebih dahulu).",
            L"Secure Boot Terdeteksi",
            MB_YESNO | MB_ICONWARNING);
        if (res != IDYES) return;
    }

    EnableWindow(g_hBtnInstall, FALSE);
    EnableWindow(g_hBtnUninstall, FALSE);

    // 2. Import Sertifikat Test ke Root & TrustedPublisher
    AppendLog(L"[1/4] Mengimpor sertifikat pengujian (MCVAD_TestCert.cer)...");
    std::wstring certPath = appDir + L"MCVAD_TestCert.cer";
    if (GetFileAttributesW(certPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring cmdCert1 = L"certutil.exe -addstore -f Root \"" + certPath + L"\"";
        std::wstring cmdCert2 = L"certutil.exe -addstore -f TrustedPublisher \"" + certPath + L"\"";
        RunHiddenCommand(cmdCert1);
        RunHiddenCommand(cmdCert2);
    }

    // 3. Aktifkan Test Signing
    AppendLog(L"[2/4] Mengaktifkan Windows Test Signing (bcdedit)...");
    bool needRestart = false;
    if (!g_bTestModeActive) {
        DWORD exitCode = RunHiddenCommand(L"bcdedit.exe /set testsigning on");
        if (exitCode == 0) {
            needRestart = true;
        }
    }

    // 4. Mendaftarkan Root Device Node via create_device.ps1
    AppendLog(L"[3/4] Mendaftarkan Root Device Node audio di Windows...");
    std::wstring psScript = appDir + L"create_device.ps1";
    if (GetFileAttributesW(psScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring cmdPs = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + psScript + L"\"";
        RunHiddenCommand(cmdPs);
    }

    // 5. Install INF via PnPUtil
    AppendLog(L"[4/4] Memasang paket driver INF via PnPUtil...");
    std::wstring infPath = appDir + L"mcvad.inf";
    if (GetFileAttributesW(infPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring cmdPnp = L"pnputil.exe /add-driver \"" + infPath + L"\" /install";
        RunHiddenCommand(cmdPnp);
    }

    RefreshSystemStatus();
    EnableWindow(g_hBtnInstall, TRUE);

    if (needRestart) {
        AppendLog(L"Instalasi selesai! Restart Windows diperlukan agar Test Signing aktif.");
        ShowWindow(g_hBtnRestart, SW_SHOW);
        int r = MessageBoxW(g_hMainWnd,
            L"Instalasi driver DF-OpenVCable telah selesai!\n\n"
            L"Windows Test Signing telah diaktifkan, namun komputer HARUS di-restart agar driver dapat dimuat.\n\n"
            L"Apakah Anda ingin me-restart komputer sekarang?",
            L"Restart Diperlukan",
            MB_YESNO | MB_ICONQUESTION);
        if (r == IDYES) {
            RunHiddenCommand(L"shutdown.exe /r /t 0");
        }
    } else {
        AppendLog(L"SUKSES: DF-OpenVCable berhasil dipasang dan siap digunakan!");
        MessageBoxW(g_hMainWnd,
            L"Selamat! Driver DF-OpenVCable berhasil dipasang.\n\n"
            L"Ke-5 kabel virtual (A s.d. E) kini siap digunakan di aplikasi dan Control Panel.",
            L"Instalasi Sukses",
            MB_OK | MB_ICONINFORMATION);
    }
}

void DoUninstall() {
    int res = MessageBoxW(g_hMainWnd,
        L"Apakah Anda yakin ingin mencopot (uninstall) DF-OpenVCable dari komputer ini?",
        L"Konfirmasi Uninstall",
        MB_YESNO | MB_ICONQUESTION);
    if (res != IDYES) return;

    EnableWindow(g_hBtnInstall, FALSE);
    EnableWindow(g_hBtnUninstall, FALSE);
    AppendLog(L"Menghapus driver DF-OpenVCable dari sistem...");

    std::wstring appDir = GetAppDir();
    std::wstring psScript = appDir + L"create_device.ps1";
    if (GetFileAttributesW(psScript.c_str()) != INVALID_FILE_ATTRIBUTES) {
        std::wstring cmdPs = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"" + psScript + L"\" -Remove";
        RunHiddenCommand(cmdPs);
    }

    std::wstring infPath = appDir + L"mcvad.inf";
    std::wstring cmdPnp = L"pnputil.exe /delete-driver \"" + infPath + L"\" /uninstall /force";
    RunHiddenCommand(cmdPnp);

    RefreshSystemStatus();
    EnableWindow(g_hBtnInstall, TRUE);
    AppendLog(L"DF-OpenVCable telah berhasil dicopot dari sistem.");

    if (g_bTestSigningActive) {
        int optTest = MessageBoxW(g_hMainWnd,
            L"Driver DF-OpenVCable telah berhasil dihapus dari sistem.\n\n"
            L"Apakah Anda ingin menonaktifkan mode Windows Test Signing sekarang?\n\n"
            L"(Pilih 'Yes' untuk menjalankan 'bcdedit /set testsigning off' dan memulihkan keamanan standar Windows. Komputer perlu di-restart).",
            L"Nonaktifkan Test Mode?",
            MB_YESNO | MB_ICONQUESTION);

        if (optTest == IDYES) {
            RunHiddenCommand(L"bcdedit.exe /set testsigning off");
            RefreshSystemStatus();

            int optRestart = MessageBoxW(g_hMainWnd,
                L"Mode Test Signing telah berhasil dinonaktifkan.\n\n"
                L"Komputer harus di-restart agar perubahan berlaku dan watermark Test Mode hilang.\n\n"
                L"Apakah Anda ingin me-restart komputer sekarang?",
                L"Restart Komputer",
                MB_YESNO | MB_ICONQUESTION);

            if (optRestart == IDYES) {
                RunHiddenCommand(L"shutdown.exe /r /t 0");
            }
            return;
        }
    }

    MessageBoxW(g_hMainWnd, L"Driver DF-OpenVCable telah berhasil dihapus dari sistem.", L"Uninstall Selesai", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFontTitle = CreateFontW(19, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontBold  = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontNormal= CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            g_hLblTitle = CreateWindowW(L"STATIC", L"DF-OpenVCable - Setup & Driver Manager", WS_CHILD | WS_VISIBLE, 20, 15, 460, 25, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

            HWND hGrp = CreateWindowW(L"BUTTON", L" Status Sistem & Keamanan ", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 50, 480, 120, hWnd, NULL, NULL, NULL);
            SendMessage(hGrp, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hLblSecureBoot = CreateWindowW(L"STATIC", L"Memeriksa status Secure Boot...", WS_CHILD | WS_VISIBLE, 35, 75, 450, 20, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblSecureBoot, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hLblTestMode = CreateWindowW(L"STATIC", L"Memeriksa status Test Signing...", WS_CHILD | WS_VISIBLE, 35, 100, 450, 20, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblTestMode, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hLblDriverStatus = CreateWindowW(L"STATIC", L"Memeriksa status driver...", WS_CHILD | WS_VISIBLE, 35, 125, 450, 20, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblDriverStatus, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hLblLog = CreateWindowW(L"STATIC", L"Siap untuk instalasi.", WS_CHILD | WS_VISIBLE | SS_LEFTNOWORDWRAP, 20, 185, 480, 30, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblLog, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnInstall = CreateWindowW(L"BUTTON", L"Pasang DF-OpenVCable", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 225, 180, 35, hWnd, (HMENU)ID_BTN_INSTALL, NULL, NULL);
            SendMessage(g_hBtnInstall, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnUninstall = CreateWindowW(L"BUTTON", L"Hapus Driver", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 210, 225, 130, 35, hWnd, (HMENU)ID_BTN_UNINSTALL, NULL, NULL);
            SendMessage(g_hBtnUninstall, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hBtnRestart = CreateWindowW(L"BUTTON", L"Restart PC Sekarang", WS_CHILD | BS_PUSHBUTTON, 350, 225, 150, 35, hWnd, (HMENU)ID_BTN_RESTART, NULL, NULL);
            SendMessage(g_hBtnRestart, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            RefreshSystemStatus();
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_INSTALL) {
                DoInstall();
            } else if (id == ID_BTN_UNINSTALL) {
                DoUninstall();
            } else if (id == ID_BTN_RESTART) {
                RunHiddenCommand(L"shutdown.exe /r /t 0");
            }
            return 0;
        }
        case WM_DESTROY: {
            if (g_hFontTitle) DeleteObject(g_hFontTitle);
            if (g_hFontBold) DeleteObject(g_hFontBold);
            if (g_hFontNormal) DeleteObject(g_hFontNormal);
            PostQuitMessage(0);
            return 0;
        }
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

// Auto-Elevate jika belum Administrator
bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return (isAdmin == TRUE);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    if (!IsRunningAsAdmin()) {
        wchar_t szPath[MAX_PATH];
        if (GetModuleFileNameW(NULL, szPath, MAX_PATH)) {
            SHELLEXECUTEINFOW sei = { sizeof(sei) };
            sei.lpVerb = L"runas";
            sei.lpFile = szPath;
            sei.hwnd = NULL;
            sei.nShow = SW_NORMAL;
            if (ShellExecuteExW(&sei)) {
                return 0;
            }
        }
        MessageBoxW(NULL, L"Hak akses Administrator diperlukan untuk memasang driver kernel.", L"Akses Ditolak", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DF_OpenVCable_Installer_Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    int w = 535, h = 315;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hMainWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"DF-OpenVCable Setup",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        x, y, w, h,
        NULL, NULL, hInstance, NULL);

    if (!g_hMainWnd) return 1;

    ShowWindow(g_hMainWnd, nCmdShow);
    UpdateWindow(g_hMainWnd);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}
