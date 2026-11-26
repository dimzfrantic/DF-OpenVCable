#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#define ID_BTN_HIDE    3001
#define ID_BTN_RESTORE 3002
#define ID_BTN_CLOSE   3003

HWND g_hMainWnd = NULL;
HWND g_hLblTitle = NULL;
HWND g_hLblDesc = NULL;
HWND g_hLblStatus = NULL;
HWND g_hBtnHide = NULL;
HWND g_hBtnRestore = NULL;

HFONT g_hFontTitle = NULL;
HFONT g_hFontNormal = NULL;
HFONT g_hFontBold = NULL;

// Helper eksekusi command tersembunyi
DWORD RunHiddenCommand(const std::wstring& cmd) {
    STARTUPINFOW si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = { 0 };
    std::wstring cmdCopy = cmd;
    if (!CreateProcessW(NULL, &cmdCopy[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return (DWORD)-1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exitCode;
}

// Cari semua path user32.dll.mui yang ada di System32
std::vector<std::wstring> FindUser32MuiFiles() {
    std::vector<std::wstring> results;
    wchar_t sysDir[MAX_PATH];
    GetSystemDirectoryW(sysDir, MAX_PATH);

    std::wstring searchPattern = std::wstring(sysDir) + L"\*\user32.dll.mui";
    WIN32_FIND_DATAW ffd;
    HANDLE hFind = FindFirstFileW(searchPattern.c_str(), &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                // Ambil folder bahasa
                results.push_back(std::wstring(sysDir) + L"\" + ffd.cFileName);
            }
        } while (FindNextFileW(hFind, &ffd));
        FindClose(hFind);
    }

    // Cari juga secara spesifik di folder en-US dan id-ID jika tidak tertangkap wildcard
    const wchar_t* commonLangs[] = { L"en-US", L"id-ID", L"en-GB" };
    for (const wchar_t* lang : commonLangs) {
        std::wstring p = std::wstring(sysDir) + L"\" + lang + L"\user32.dll.mui";
        if (GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) {
            bool exists = false;
            for (const auto& r : results) {
                if (_wcsicmp(r.c_str(), p.c_str()) == 0) { exists = true; break; }
            }
            if (!exists) results.push_back(p);
        }
    }
    return results;
}

bool HasBackup() {
    std::vector<std::wstring> muiFiles = FindUser32MuiFiles();
    for (const auto& path : muiFiles) {
        std::wstring bakPath = path + L".bak";
        if (GetFileAttributesW(bakPath.c_str()) != INVALID_FILE_ATTRIBUTES) return true;
    }
    return false;
}

void UpdateStatus() {
    if (HasBackup()) {
        SetWindowTextW(g_hLblStatus, L"Status: Watermark telah dimodifikasi (Backup asli tersedia).");
        EnableWindow(g_hBtnRestore, TRUE);
    } else {
        SetWindowTextW(g_hLblStatus, L"Status: Watermark sistem asli masih aktif (Belum di-patch).");
        EnableWindow(g_hBtnRestore, FALSE);
    }
}

// Restart Windows Explorer agar perubahan visual langsung muncul
void RestartExplorer() {
    RunHiddenCommand(L"taskkill.exe /f /im explorer.exe");
    Sleep(500);
    ShellExecuteW(NULL, L"open", L"explorer.exe", NULL, NULL, SW_NORMAL);
}

// Patch file mui: Mengganti string "Test Mode" dengan spasi kosong
bool PatchMuiFile(const std::wstring& filePath) {
    std::wstring bakPath = filePath + L".bak";

    // 1. Take ownership & izin write
    RunHiddenCommand(L"takeown.exe /f \"" + filePath + L"\"");
    RunHiddenCommand(L"icacls.exe \"" + filePath + L"\" /grant administrators:F");

    // 2. Buat backup jika belum ada
    if (GetFileAttributesW(bakPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!CopyFileW(filePath.c_str(), bakPath.c_str(), FALSE)) {
            return false;
        }
    }

    // 3. Baca seluruh isi file ke memori
    HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD fileSize = GetFileSize(hFile, NULL);
    if (fileSize == 0 || fileSize == INVALID_FILE_SIZE) {
        CloseHandle(hFile);
        return false;
    }

    std::vector<BYTE> buffer(fileSize);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, buffer.data(), fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
        CloseHandle(hFile);
        return false;
    }

    // 4. Cari dan samarkan string UTF-16LE: "Test Mode", "Mode Uji", "Evaluation Copy"
    const wchar_t* targets[] = {
        L"Test Mode",
        L"Mode Uji",
        L"Evaluation Copy",
        L"Salinan Evaluasi"
    };

    bool modified = false;
    for (const wchar_t* target : targets) {
        size_t targetLen = wcslen(target);
        size_t targetBytes = targetLen * sizeof(wchar_t);

        if (fileSize < targetBytes) continue;

        for (size_t i = 0; i <= fileSize - targetBytes; i += 2) {
            if (memcmp(&buffer[i], target, targetBytes) == 0) {
                // Timpa dengan karakter spasi kosong UTF-16 (0x0020)
                for (size_t k = 0; k < targetLen; k++) {
                    wchar_t spaceChar = L' ';
                    memcpy(&buffer[i + (k * 2)], &spaceChar, 2);
                }
                modified = true;
            }
        }
    }

    if (modified) {
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        DWORD bytesWritten = 0;
        WriteFile(hFile, buffer.data(), fileSize, &bytesWritten, NULL);
    }
    CloseHandle(hFile);
    return modified;
}

// Kembalikan file dari backup
bool RestoreMuiFile(const std::wstring& filePath) {
    std::wstring bakPath = filePath + L".bak";
    if (GetFileAttributesW(bakPath.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    RunHiddenCommand(L"takeown.exe /f \"" + filePath + L"\"");
    RunHiddenCommand(L"icacls.exe \"" + filePath + L"\" /grant administrators:F");

    if (CopyFileW(bakPath.c_str(), filePath.c_str(), FALSE)) {
        DeleteFileW(bakPath.c_str());
        return true;
    }
    return false;
}

void DoHideWatermark() {
    std::vector<std::wstring> files = FindUser32MuiFiles();
    if (files.empty()) {
        MessageBoxW(g_hMainWnd, L"File sistem user32.dll.mui tidak ditemukan di System32.", L"Gagal", MB_OK | MB_ICONERROR);
        return;
    }

    int countPatched = 0;
    for (const auto& f : files) {
        if (PatchMuiFile(f)) countPatched++;
    }

    UpdateStatus();
    RestartExplorer();

    MessageBoxW(g_hMainWnd,
        L"Watermark 'Test Mode' berhasil disembunyikan!\n\n"
        L"Windows Explorer telah dimuat ulang untuk memperbarui tampilan desktop.",
        L"Sukses", MB_OK | MB_ICONINFORMATION);
}

void DoRestoreWatermark() {
    std::vector<std::wstring> files = FindUser32MuiFiles();
    int countRestored = 0;
    for (const auto& f : files) {
        if (RestoreMuiFile(f)) countRestored++;
    }

    UpdateStatus();
    RestartExplorer();

    MessageBoxW(g_hMainWnd,
        L"Watermark asli berhasil dikembalikan!\n\n"
        L"Windows Explorer telah dimuat ulang.",
        L"Sukses", MB_OK | MB_ICONINFORMATION);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            g_hFontTitle = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontBold  = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            g_hFontNormal= CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            g_hLblTitle = CreateWindowW(L"STATIC", L"DF-OpenVCable - Watermark Manager (Optional)", WS_CHILD | WS_VISIBLE, 20, 15, 440, 25, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);

            g_hLblDesc = CreateWindowW(L"STATIC",
                L"Utilitas ini digunakan untuk menyembunyikan tulisan 'Test Mode' di pojok kanan bawah desktop Windows.\n\n"
                L"File sistem asli otomatis dicadangkan (.bak) dan dapat dikembalikan kapan saja.",
                WS_CHILD | WS_VISIBLE, 20, 48, 440, 55, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblDesc, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            g_hLblStatus = CreateWindowW(L"STATIC", L"Status: Memeriksa...", WS_CHILD | WS_VISIBLE, 20, 115, 440, 25, hWnd, NULL, NULL, NULL);
            SendMessage(g_hLblStatus, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnHide = CreateWindowW(L"BUTTON", L"Sembunyikan Watermark", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 20, 150, 200, 35, hWnd, (HMENU)ID_BTN_HIDE, NULL, NULL);
            SendMessage(g_hBtnHide, WM_SETFONT, (WPARAM)g_hFontBold, TRUE);

            g_hBtnRestore = CreateWindowW(L"BUTTON", L"Kembalikan Asli", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 230, 150, 160, 35, hWnd, (HMENU)ID_BTN_RESTORE, NULL, NULL);
            SendMessage(g_hBtnRestore, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);

            UpdateStatus();
            return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(wParam);
            if (id == ID_BTN_HIDE) {
                DoHideWatermark();
            } else if (id == ID_BTN_RESTORE) {
                DoRestoreWatermark();
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

// Auto-Elevate Administrator
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
        MessageBoxW(NULL, L"Hak akses Administrator diperlukan untuk memodifikasi pengaturan desktop.", L"Akses Ditolak", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icex);

    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DF_OpenVCable_Watermark_Class";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassExW(&wc);

    int w = 480, h = 240;
    int x = (GetSystemMetrics(SM_CXSCREEN) - w) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - h) / 2;

    g_hMainWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"DF-OpenVCable Watermark Manager",
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
