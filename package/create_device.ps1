<#
.SYNOPSIS
    MCVAD Root Device Enumerator Helper
    Membuat root device node ROOT\MCVAD di Device Manager Windows tanpa butuh devcon.exe.
#>

param (
    [switch]$Remove,
    [switch]$Cleanup,
    # -Verify: hanya melaporkan jumlah endpoint audio DF-OpenVCable yang terdaftar
    # di MMDevices (Render/Capture) tanpa mengubah apa pun. Harapannya 5 + 5.
    [switch]$Verify
)

$Source = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class DeviceInstaller {
    private const int DIGCF_ALLCLASSES = 0x00000004;
    private const int DIGCF_PRESENT = 0x00000002;
    private const int SPDRP_HARDWAREID = 0x00000001;
    private const int DIF_REGISTERDEVICE = 0x00000019;
    private const int DIF_REMOVE = 0x00000005;

    [StructLayout(LayoutKind.Sequential)]
    public struct SP_DEVINFO_DATA {
        public int cbSize;
        public Guid ClassGuid;
        public int DevInst;
        public IntPtr Reserved;
    }

    [DllImport("setupapi.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern IntPtr SetupDiCreateDeviceInfoList(ref Guid ClassGuid, IntPtr hwndParent);

    [DllImport("setupapi.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern IntPtr SetupDiGetClassDevs(ref Guid ClassGuid, IntPtr Enumerator, IntPtr hwndParent, int Flags);

    [DllImport("setupapi.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern bool SetupDiCreateDeviceInfo(IntPtr DeviceInfoSet, string DeviceName, ref Guid ClassGuid, string DeviceDescription, IntPtr hwndParent, int CreationFlags, ref SP_DEVINFO_DATA DeviceInfoData);

    [DllImport("setupapi.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern bool SetupDiSetDeviceRegistryProperty(IntPtr DeviceInfoSet, ref SP_DEVINFO_DATA DeviceInfoData, int Property, byte[] PropertyBuffer, int PropertyBufferSize);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiCallClassInstaller(int InstallFunction, IntPtr DeviceInfoSet, ref SP_DEVINFO_DATA DeviceInfoData);

    [DllImport("setupapi.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern bool SetupDiEnumDeviceInfo(IntPtr DeviceInfoSet, int MemberIndex, ref SP_DEVINFO_DATA DeviceInfoData);

    [DllImport("setupapi.dll", CharSet = CharSet.Auto, SetLastError = true)]
    public static extern bool SetupDiGetDeviceRegistryProperty(IntPtr DeviceInfoSet, ref SP_DEVINFO_DATA DeviceInfoData, int Property, out int PropertyRegDataType, byte[] PropertyBuffer, int PropertyBufferSize, out int RequiredSize);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiDestroyDeviceInfoList(IntPtr DeviceInfoSet);

    [DllImport("newdev.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool UpdateDriverForPlugAndPlayDevices(IntPtr hwndParent, string HardwareId, string FullInfPath, uint InstallFlags, out bool RebootRequired);

    public static bool CreateRootDevice(Guid classGuid, string hardwareId, string deviceDesc, string infPath, out string errorDetail) {
        errorDetail = string.Empty;
        IntPtr devInfoSet = SetupDiCreateDeviceInfoList(ref classGuid, IntPtr.Zero);
        if (devInfoSet == (IntPtr)(-1) || devInfoSet == IntPtr.Zero) {
            errorDetail = "SetupDiCreateDeviceInfoList error " + Marshal.GetLastWin32Error();
            return false;
        }

        SP_DEVINFO_DATA devInfoData = new SP_DEVINFO_DATA();
        devInfoData.cbSize = Marshal.SizeOf(devInfoData);

        // Devcon standard: DeviceName with DICD_GENERATE_ID must NOT include 'ROOT\\'. Use 'MEDIA' or 'MCVAD'.
        string rootDevName = "MEDIA";
        if (!SetupDiCreateDeviceInfo(devInfoSet, rootDevName, ref classGuid, deviceDesc, IntPtr.Zero, 0x00000001, ref devInfoData)) {
            errorDetail = "SetupDiCreateDeviceInfo error " + Marshal.GetLastWin32Error();
            SetupDiDestroyDeviceInfoList(devInfoSet);
            return false;
        }

        byte[] hwIdBytes = Encoding.Unicode.GetBytes(hardwareId + "\0\0");
        if (!SetupDiSetDeviceRegistryProperty(devInfoSet, ref devInfoData, SPDRP_HARDWAREID, hwIdBytes, hwIdBytes.Length)) {
            errorDetail = "SetupDiSetDeviceRegistryProperty error " + Marshal.GetLastWin32Error();
            SetupDiDestroyDeviceInfoList(devInfoSet);
            return false;
        }

        if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devInfoSet, ref devInfoData)) {
            errorDetail = "SetupDiCallClassInstaller error " + Marshal.GetLastWin32Error();
            SetupDiDestroyDeviceInfoList(devInfoSet);
            return false;
        }

        SetupDiDestroyDeviceInfoList(devInfoSet);

        if (!string.IsNullOrEmpty(infPath) && System.IO.File.Exists(infPath)) {
            bool reboot = false;
            string fullInf = System.IO.Path.GetFullPath(infPath);
            UpdateDriverForPlugAndPlayDevices(IntPtr.Zero, hardwareId, fullInf, 1, out reboot);
        }

        return true;
    }

    public static int CountRootDevices(Guid classGuid, string hardwareId) {
        IntPtr devInfoSet = SetupDiGetClassDevs(ref classGuid, IntPtr.Zero, IntPtr.Zero, DIGCF_ALLCLASSES);
        if (devInfoSet == (IntPtr)(-1) || devInfoSet == IntPtr.Zero) return -1;

        SP_DEVINFO_DATA devInfoData = new SP_DEVINFO_DATA();
        devInfoData.cbSize = Marshal.SizeOf(devInfoData);
        int index = 0;
        int count = 0;

        while (SetupDiEnumDeviceInfo(devInfoSet, index++, ref devInfoData)) {
            byte[] buffer = new byte[512];
            int regType = 0;
            int requiredSize = 0;
            if (SetupDiGetDeviceRegistryProperty(devInfoSet, ref devInfoData, SPDRP_HARDWAREID, out regType, buffer, buffer.Length, out requiredSize)) {
                string curHwId = Encoding.Unicode.GetString(buffer, 0, requiredSize).TrimEnd('\0');
                if (curHwId.IndexOf(hardwareId, StringComparison.OrdinalIgnoreCase) >= 0) count++;
            }
        }

        SetupDiDestroyDeviceInfoList(devInfoSet);
        return count;
    }

    public static bool RemoveRootDevice(Guid classGuid, string hardwareId) {
        IntPtr devInfoSet = SetupDiGetClassDevs(ref classGuid, IntPtr.Zero, IntPtr.Zero, DIGCF_ALLCLASSES);
        if (devInfoSet == (IntPtr)(-1)) return false;

        SP_DEVINFO_DATA devInfoData = new SP_DEVINFO_DATA();
        devInfoData.cbSize = Marshal.SizeOf(devInfoData);
        int index = 0;
        bool found = false;

        while (SetupDiEnumDeviceInfo(devInfoSet, index++, ref devInfoData)) {
            byte[] buffer = new byte[512];
            int regType = 0;
            int requiredSize = 0;
            if (SetupDiGetDeviceRegistryProperty(devInfoSet, ref devInfoData, SPDRP_HARDWAREID, out regType, buffer, buffer.Length, out requiredSize)) {
                string curHwId = Encoding.Unicode.GetString(buffer, 0, requiredSize).TrimEnd('\0');
                if (curHwId.IndexOf(hardwareId, StringComparison.OrdinalIgnoreCase) >= 0) {
                    SetupDiCallClassInstaller(DIF_REMOVE, devInfoSet, ref devInfoData);
                    found = true;
                }
            }
        }

        SetupDiDestroyDeviceInfoList(devInfoSet);
        return found;
    }
}
"@

Add-Type -TypeDefinition $Source -ErrorAction Stop

# Helper untuk mengaktifkan privilege token (dibutuhkan untuk mengambil ownership key MMDevices)
$PrivSource = @"
using System;
using System.Runtime.InteropServices;

public static class TokenPrivilege {
    [StructLayout(LayoutKind.Sequential)] public struct LUID { public uint LowPart; public int HighPart; }
    [StructLayout(LayoutKind.Sequential)] public struct LUID_AND_ATTRIBUTES { public LUID Luid; public uint Attributes; }
    [StructLayout(LayoutKind.Sequential)] public struct TOKEN_PRIVILEGES { public uint PrivilegeCount; public LUID_AND_ATTRIBUTES Privilege; }

    [DllImport("advapi32.dll", SetLastError = true)]
    public static extern bool OpenProcessToken(IntPtr process, uint access, out IntPtr token);
    [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
    public static extern bool LookupPrivilegeValue(string system, string name, out LUID luid);
    [DllImport("advapi32.dll", SetLastError = true)]
    public static extern bool AdjustTokenPrivileges(IntPtr token, bool disableAll, ref TOKEN_PRIVILEGES newState, uint length, IntPtr previous, IntPtr returnLength);
    [DllImport("kernel32.dll")] public static extern IntPtr GetCurrentProcess();

    public static bool Enable(string name) {
        const uint TOKEN_ADJUST_PRIVILEGES = 0x0020;
        const uint TOKEN_QUERY = 0x0008;
        const uint SE_PRIVILEGE_ENABLED = 0x0002;

        IntPtr token;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, out token)) return false;
        LUID luid;
        if (!LookupPrivilegeValue(null, name, out luid)) return false;

        TOKEN_PRIVILEGES tp = new TOKEN_PRIVILEGES();
        tp.PrivilegeCount = 1;
        tp.Privilege.Luid = luid;
        tp.Privilege.Attributes = SE_PRIVILEGE_ENABLED;
        return AdjustTokenPrivileges(token, false, ref tp, 0, IntPtr.Zero, IntPtr.Zero);
    }
}
"@

Add-Type -TypeDefinition $PrivSource -ErrorAction Stop

function Get-McvadAudioEndpoints {
    <#
      Enumerasi endpoint audio milik DF-OpenVCable langsung dari database MMDevices.
      Filter memakai DEVPKEY_Device_HardwareIds = ROOT\MCVAD (presisi), dengan
      fallback nama interface untuk build lama yang belum memakai hardware id itu.
      Ini menggantikan filter lama yang mencocokkan string "MCVAD"/"OpenVCable" di
      SEMUA value properti (bisa salah tangkap endpoint driver lain).
    #>
    $result = @()
    $bases = @(
        @{ Dir = 'Render';  Path = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render'  },
        @{ Dir = 'Capture'; Path = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Capture' }
    )

    foreach ($base in $bases) {
        if (-not (Test-Path -LiteralPath $base.Path)) { continue }
        foreach ($ep in (Get-ChildItem -LiteralPath $base.Path -ErrorAction SilentlyContinue)) {
            $propPath = Join-Path $ep.PSPath 'Properties'
            if (-not (Test-Path -LiteralPath $propPath)) { continue }
            $props = Get-ItemProperty -LiteralPath $propPath -ErrorAction SilentlyContinue
            if (-not $props) { continue }

            $hwIds   = $props.'{a8b865dd-2e3d-4094-ad97-e593a70c75d6},8'
            $ifName  = $props.'{b3f8fa53-0004-438e-9003-51a46e139bfc},6'
            $devName = $props.'{a45c254e-df1c-4efd-8020-67d146a850e0},2'
            $instId  = $props.'{b3f8fa53-0004-438e-9003-51a46e139bfc},2'

            $isOurs = $false
            foreach ($h in @($hwIds)) {
                if ($h -is [string] -and $h -match '(?i)ROOT\\MCVAD') { $isOurs = $true; break }
            }
            if (-not $isOurs -and $ifName -is [string] -and $ifName -match '(?i)OpenVCable') { $isOurs = $true }
            if (-not $isOurs) { continue }

            $state = (Get-ItemProperty -LiteralPath $ep.PSPath -ErrorAction SilentlyContinue).DeviceState
            $result += [pscustomobject]@{
                Direction = $base.Dir
                Id        = $ep.PSChildName
                KeyPath   = $ep.PSPath
                Name      = $devName
                Iface     = $ifName
                Instance  = $instId
                State     = $state
                Active    = ($state -eq 1)
            }
        }
    }
    return $result
}

function Show-AudioEndpointSummary {
    param([switch]$Quiet)

    $eps = @(Get-McvadAudioEndpoints)
    $render  = @($eps | Where-Object { $_.Direction -eq 'Render' })
    $capture = @($eps | Where-Object { $_.Direction -eq 'Capture' })
    $renderActive  = @($render  | Where-Object { $_.Active })
    $captureActive = @($capture | Where-Object { $_.Active })

    if (-not $Quiet) {
        Write-Host "[MCVAD] Endpoint DF-OpenVCable terdaftar di MMDevices:" -ForegroundColor Cyan
        foreach ($g in ($eps | Group-Object Direction)) {
            Write-Host ("        {0,-8}: {1} entri ({2} aktif)" -f $g.Name, $g.Count, @($g.Group | Where-Object { $_.Active }).Count)
        }
        foreach ($e in ($eps | Sort-Object Direction, Name)) {
            $flag = if ($e.Active) { 'ACTIVE ' } else { 'stale  ' }
            Write-Host ("        [{0}] {1,-6} {2,-18} {3}" -f $flag, $e.Direction, $e.Name, $e.Id)
        }
    }

    $ok = ($renderActive.Count -eq 5) -and ($captureActive.Count -eq 5)
    if (-not $Quiet) {
        if ($ok) {
            Write-Host "[MCVAD] OK: 5 endpoint Render + 5 endpoint Capture aktif (10 total)." -ForegroundColor Green
        } else {
            Write-Host ("[MCVAD] PERINGATAN: harapannya 5 Render + 5 Capture aktif, ditemukan {0} Render + {1} Capture." -f $renderActive.Count, $captureActive.Count) -ForegroundColor Yellow
        }
    }
    return [pscustomobject]@{
        RenderTotal  = $render.Count
        CaptureTotal = $capture.Count
        RenderActive = $renderActive.Count
        CaptureActive = $captureActive.Count
        Ok           = $ok
    }
}

function Clear-AudioEndpointCache {
    <#
      Hapus paksa entri MMDevices milik DF-OpenVCable supaya AudioEndpointBuilder
      membuat ulang endpoint dari descriptor driver yang sekarang.

      Kenapa ini perlu: AudioEndpointBuilder MENYIMPAN property store endpoint
      (PKEY_Device_DeviceDesc) dan mewariskannya lintas update driver, meskipun
      GUID endpoint berubah. Akibatnya nama lama ("Speakers") menempel terus
      walaupun descriptor pin di driver sudah benar.

      Kenapa tidak cukup Remove-Item: key MMDevices di-ACL oleh Windows, dan
      BUILTIN\Administrators hanya punya SetValue + ReadKey (TANPA Delete).
      Jadi kita ambil ownership dulu (SeTakeOwnershipPrivilege) lalu beri
      Administrators FullControl, baru dihapus.
    #>
    Write-Host "[MCVAD] Menghapus cache endpoint MMDevices milik DF-OpenVCable..." -ForegroundColor Cyan

    $eps = @(Get-McvadAudioEndpoints)
    if ($eps.Count -eq 0) {
        Write-Host "[MCVAD] Tidak ada endpoint DF-OpenVCable di cache. (bersih)" -ForegroundColor Green
        return
    }

    [TokenPrivilege]::Enable('SeTakeOwnershipPrivilege') | Out-Null
    [TokenPrivilege]::Enable('SeRestorePrivilege')       | Out-Null

    $removed = 0
    $failed  = 0
    foreach ($ep in $eps) {
        if (Remove-RegistryKeyForce $ep.KeyPath) { $removed++ } else { $failed++ }
    }

    if ($removed -gt 0) {
        Write-Host "[MCVAD] $removed endpoint dihapus dari cache - akan dibuat ulang oleh AudioEndpointBuilder." -ForegroundColor Green
    }
    if ($failed -gt 0) {
        Write-Host "[MCVAD] $failed endpoint gagal dihapus (jalankan sebagai Administrator)." -ForegroundColor Yellow
    }
}

function Remove-RegistryKeyForce {
    param([string]$PsPath)

    # 1) Coba hapus langsung
    try {
        Remove-Item -LiteralPath $PsPath -Recurse -Force -ErrorAction Stop
        return $true
    } catch { }

    # 2) Ambil ownership, beri FullControl, lalu hapus
    $sub = $PsPath -replace '^Microsoft\.PowerShell\.Core\\Registry::', '' -replace '^HKLM:\\', ''
    try {
        $admins = New-Object System.Security.Principal.NTAccount('BUILTIN', 'Administrators')

        $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
            $sub,
            [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree,
            [System.Security.AccessControl.RegistryRights]::TakeOwnership)
        if (-not $key) { return $false }

        $acl = $key.GetAccessControl([System.Security.AccessControl.AccessControlSections]::None)
        $acl.SetOwner($admins)
        $key.SetAccessControl($acl)
        $key.Close()

        $key = [Microsoft.Win32.Registry]::LocalMachine.OpenSubKey(
            $sub,
            [Microsoft.Win32.RegistryKeyPermissionCheck]::ReadWriteSubTree,
            [System.Security.AccessControl.RegistryRights]::ChangePermissions)
        if (-not $key) { return $false }

        $acl = $key.GetAccessControl()
        $rule = New-Object System.Security.AccessControl.RegistryAccessRule(
            $admins, 'FullControl', 'ContainerInherit,ObjectInherit', 'None', 'Allow')
        $acl.SetAccessRule($rule)
        $key.SetAccessControl($acl)
        $key.Close()

        Remove-Item -LiteralPath $PsPath -Recurse -Force -ErrorAction Stop
        return $true
    } catch {
        return $false
    }
}

function Stop-McvadDriver {
    # PENTING: PnP akan MEMAKAI ULANG image driver yang sudah termuat kalau service
    # kernel `mcvad` masih Running ketika devnode baru dibuat. Akibatnya devnode baru
    # tetap memakai tabel PCPIN_DESCRIPTOR lama — mis. pin Render masih terbaca
    # KSNODETYPE_SPEAKER sehingga endpoint tetap bernama "Speakers" dengan form factor
    # speaker, walaupun binary di DriverStore sudah versi baru (ImagePath sudah menunjuk
    # ke folder DriverStore yang baru). Gejala khasnya: interface lama dan baru
    # (mis. CableA_Topo dan CableA_Topo_v2) muncul bersamaan di DeviceClasses.
    # Karena itu driver harus di-unload dulu sebelum devnode dihapus/dibuat ulang.
    $devs = @(Get-PnpDevice -ErrorAction SilentlyContinue |
              Where-Object { $_.InstanceId -like 'ROOT\MCVAD*' -or
                             ($_.InstanceId -like 'ROOT\MEDIA\*' -and $_.FriendlyName -like '*OpenVCable*') })
    if ($devs.Count -eq 0) {
        Write-Host "[MCVAD] Tidak ada devnode DF-OpenVCable untuk di-unload." -ForegroundColor Gray
        return
    }

    Write-Host ("[MCVAD] Memaksa unload driver mcvad ({0} devnode)..." -f $devs.Count) -ForegroundColor Cyan
    foreach ($d in $devs) {
        try {
            Disable-PnpDevice -InstanceId $d.InstanceId -Confirm:$false -ErrorAction Stop
        } catch {
            Write-Host ("[MCVAD]   disable gagal: " + $d.InstanceId + " - " + $_.Exception.Message) -ForegroundColor Yellow
        }
    }

    # Tunggu sampai image benar-benar dilepas (maks 20 detik).
    for ($i = 0; $i -lt 40; $i++) {
        $svc = Get-Service mcvad -ErrorAction SilentlyContinue
        if (-not $svc -or $svc.Status -ne 'Running') { break }
        Start-Sleep -Milliseconds 500
    }

    $svc = Get-Service mcvad -ErrorAction SilentlyContinue
    $state = if ($svc) { $svc.Status } else { 'tidak ada' }
    Write-Host "[MCVAD] Status service mcvad setelah unload: $state" -ForegroundColor Gray
    if ($state -eq 'Running') {
        Write-Host "[MCVAD] PERINGATAN: driver masih termuat. Devnode baru akan memakai binary lama." -ForegroundColor Yellow
        Write-Host "        Restart Windows lalu jalankan ulang skrip ini." -ForegroundColor Yellow
    }
}

function Restart-AudioServices {
    Write-Host "[MCVAD] Me-restart AudioEndpointBuilder + Audio Service..." -ForegroundColor Cyan
    & net stop AudioEndpointBuilder /y 2>&1 | Out-Null
    Start-Sleep -Milliseconds 800
    & net start AudioEndpointBuilder 2>&1 | Out-Null
    & net start audiosrv 2>&1 | Out-Null
    Start-Sleep -Milliseconds 500
}

$MediaClassGuid = [Guid]::Parse("{4d36e96c-e325-11ce-bfc1-08002be10318}")
$HardwareId = "ROOT\MCVAD"
$DeviceDesc = "DF-OpenVCable"
$InfPath = Join-Path $PSScriptRoot "mcvad.inf"

if ($Verify) {
    Write-Host "[MCVAD] Verifikasi endpoint audio DF-OpenVCable (tanpa mengubah apa pun)..." -ForegroundColor Cyan
    $null = Show-AudioEndpointSummary
    exit 0
}

if ($Remove) {
    Write-Host "[MCVAD] Menghapus SEMUA root device node: $HardwareId..." -ForegroundColor Yellow
    Stop-McvadDriver
    $res = [DeviceInstaller]::RemoveRootDevice($MediaClassGuid, $HardwareId)

    # Sisa instance yang masih tertinggal dihapus lewat pnputil (DIF_REMOVE bisa
    # meninggalkan instance kalau devnode sedang sibuk / dalam proses restart).
    $leftover = @(Get-PnpDevice -ErrorAction SilentlyContinue |
                  Where-Object { $_.InstanceId -like 'ROOT\MCVAD*' -or ($_.InstanceId -like 'ROOT\MEDIA\*' -and $_.FriendlyName -like '*OpenVCable*') })
    foreach ($d in $leftover) {
        Write-Host ("[MCVAD] pnputil /remove-device " + $d.InstanceId) -ForegroundColor Gray
        & pnputil /remove-device $d.InstanceId 2>&1 | Out-Null
    }

    Clear-AudioEndpointCache
    $null = Show-AudioEndpointSummary

    if ($res) {
        Write-Host "[MCVAD] Device node berhasil dihapus dari Device Manager." -ForegroundColor Green
    } else {
        Write-Host "[MCVAD] Tidak ditemukan device node $HardwareId." -ForegroundColor Gray
    }
} else {
    # Idempotensi: jangan buat devnode baru kalau sudah ada.
    # Tanpa ini, setiap kali installer dijalankan akan muncul instance duplikat
    # (ROOT\MEDIA\0004, 0005, 0006, ...) yang membuat AudioEndpointBuilder bingung.
    $existing = [DeviceInstaller]::CountRootDevices($MediaClassGuid, $HardwareId)
    if ($existing -lt 0) { $existing = 0 }

    if ($Cleanup -and $existing -gt 0) {
        Write-Host "[MCVAD] Cleanup: menghapus $existing devnode duplikat..." -ForegroundColor Yellow
        Stop-McvadDriver
        $null = [DeviceInstaller]::RemoveRootDevice($MediaClassGuid, $HardwareId)

        # Bersihkan sisa instance via pnputil sebelum devnode dibuat ulang.
        $leftover = @(Get-PnpDevice -ErrorAction SilentlyContinue |
                      Where-Object { $_.InstanceId -like 'ROOT\MCVAD*' -or ($_.InstanceId -like 'ROOT\MEDIA\*' -and $_.FriendlyName -like '*OpenVCable*') })
        foreach ($d in $leftover) {
            Write-Host ("[MCVAD] pnputil /remove-device " + $d.InstanceId) -ForegroundColor Gray
            & pnputil /remove-device $d.InstanceId 2>&1 | Out-Null
        }

        Start-Sleep -Milliseconds 800
        $existing = 0

        # Cache endpoint lama dibersihkan SETELAH devnode hilang. Kalau dibersihkan
        # lebih dulu, AudioEndpointBuilder akan membuat ulang entry dari devnode
        # yang masih hidup dan jumlahnya kembali duplikat.
        Clear-AudioEndpointCache
    }

    if ($existing -eq 0) {
        Write-Host "[MCVAD] Mendaftarkan root device node: $HardwareId..." -ForegroundColor Cyan
        $errDetail = [string]::Empty
        $res = [DeviceInstaller]::CreateRootDevice($MediaClassGuid, $HardwareId, $DeviceDesc, $InfPath, [ref]$errDetail)
        if ($res) {
            Write-Host "[MCVAD] Root device node berhasil dibuat di Device Manager!" -ForegroundColor Green
            Write-Host "[MCVAD] Memindai perangkat PnP baru..." -ForegroundColor Gray
            & pnputil /scan-devices | Out-Null
        } else {
            Write-Host "[MCVAD] Gagal membuat device node: $errDetail" -ForegroundColor Yellow
        }
    } elseif ($existing -eq 1) {
        Write-Host "[MCVAD] Devnode sudah ada (1 instance) - tidak membuat duplikat." -ForegroundColor Green
        Write-Host "[MCVAD] Memindai ulang perangkat PnP..." -ForegroundColor Gray
        & pnputil /scan-devices | Out-Null
    } else {
        Write-Host "[MCVAD] PERINGATAN: ditemukan $existing devnode duplikat untuk $HardwareId." -ForegroundColor Yellow
        Write-Host "        Jalankan ulang dengan -Cleanup untuk merapikan menjadi 1 instance:" -ForegroundColor Yellow
        Write-Host "        powershell -ExecutionPolicy Bypass -File create_device.ps1 -Cleanup" -ForegroundColor Gray
        & pnputil /scan-devices | Out-Null
    }

    Write-Host ""
    Write-Host "[MCVAD] Restart layanan audio agar AudioEndpointBuilder membuat ulang endpoint..."
    Restart-AudioServices

    Write-Host ""
    Write-Host "[MCVAD] Ringkasan endpoint audio setelah operasi:" -ForegroundColor Cyan
    $summary = Show-AudioEndpointSummary
    if (-not $summary.Ok) {
        Write-Host ""
        Write-Host "[MCVAD] Belum 5 Render + 5 Capture. Coba restart layanan audio, lalu cek ulang:" -ForegroundColor Yellow
        Write-Host "        net stop audiosrv /y & net start audiosrv" -ForegroundColor Gray
        Write-Host "        powershell -ExecutionPolicy Bypass -File create_device.ps1 -Verify" -ForegroundColor Gray
    }
}
