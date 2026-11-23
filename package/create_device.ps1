<#
.SYNOPSIS
    MCVAD Root Device Enumerator Helper
    Membuat root device node ROOT\MCVAD di Device Manager Windows tanpa butuh devcon.exe.
#>

param (
    [switch]$Remove,
    [switch]$Cleanup
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

$MediaClassGuid = [Guid]::Parse("{4d36e96c-e325-11ce-bfc1-08002be10318}")
$HardwareId = "ROOT\MCVAD"
$DeviceDesc = "DF-OpenVCable Virtual Audio Adapter"
$InfPath = Join-Path $PSScriptRoot "mcvad.inf"

if ($Remove) {
    Write-Host "[MCVAD] Menghapus SEMUA root device node: $HardwareId..." -ForegroundColor Yellow
    $res = [DeviceInstaller]::RemoveRootDevice($MediaClassGuid, $HardwareId)
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
        $null = [DeviceInstaller]::RemoveRootDevice($MediaClassGuid, $HardwareId)
        Start-Sleep -Milliseconds 800
        $existing = 0
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
}
