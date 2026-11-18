<#
.SYNOPSIS
    MCVAD Root Device Enumerator Helper
    Membuat root device node ROOT\MCVAD di Device Manager Windows tanpa butuh devcon.exe.
#>

param (
    [switch]$Remove
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

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern IntPtr SetupDiCreateDeviceInfoList(ref Guid ClassGuid, IntPtr hwndParent);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern IntPtr SetupDiGetClassDevs(ref Guid ClassGuid, IntPtr Enumerator, IntPtr hwndParent, int Flags);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiCreateDeviceInfo(IntPtr DeviceInfoSet, string DeviceName, ref Guid ClassGuid, string DeviceDescription, IntPtr hwndParent, int CreationFlags, ref SP_DEVINFO_DATA DeviceInfoData);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiSetDeviceRegistryProperty(IntPtr DeviceInfoSet, ref SP_DEVINFO_DATA DeviceInfoData, int Property, byte[] PropertyBuffer, int PropertyBufferSize);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiCallClassInstaller(int InstallFunction, IntPtr DeviceInfoSet, ref SP_DEVINFO_DATA DeviceInfoData);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiEnumDeviceInfo(IntPtr DeviceInfoSet, int MemberIndex, ref SP_DEVINFO_DATA DeviceInfoData);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiGetDeviceRegistryProperty(IntPtr DeviceInfoSet, ref SP_DEVINFO_DATA DeviceInfoData, int Property, out int PropertyRegDataType, byte[] PropertyBuffer, int PropertyBufferSize, out int RequiredSize);

    [DllImport("setupapi.dll", SetLastError = true)]
    public static extern bool SetupDiDestroyDeviceInfoList(IntPtr DeviceInfoSet);

    public static bool CreateRootDevice(Guid classGuid, string hardwareId, string deviceDesc) {
        IntPtr devInfoSet = SetupDiCreateDeviceInfoList(ref classGuid, IntPtr.Zero);
        if (devInfoSet == (IntPtr)(-1)) return false;

        SP_DEVINFO_DATA devInfoData = new SP_DEVINFO_DATA();
        devInfoData.cbSize = Marshal.SizeOf(devInfoData);

        if (!SetupDiCreateDeviceInfo(devInfoSet, hardwareId, ref classGuid, deviceDesc, IntPtr.Zero, 0x00000001, ref devInfoData)) {
            SetupDiDestroyDeviceInfoList(devInfoSet);
            return false;
        }

        byte[] hwIdBytes = Encoding.Unicode.GetBytes(hardwareId + "\0\0");
        if (!SetupDiSetDeviceRegistryProperty(devInfoSet, ref devInfoData, SPDRP_HARDWAREID, hwIdBytes, hwIdBytes.Length)) {
            SetupDiDestroyDeviceInfoList(devInfoSet);
            return false;
        }

        if (!SetupDiCallClassInstaller(DIF_REGISTERDEVICE, devInfoSet, ref devInfoData)) {
            SetupDiDestroyDeviceInfoList(devInfoSet);
            return false;
        }

        SetupDiDestroyDeviceInfoList(devInfoSet);
        return true;
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
$DeviceDesc = "Multi-Cable Virtual Audio Adapter"

if ($Remove) {
    Write-Host "[MCVAD] Menghapus root device node: $HardwareId..." -ForegroundColor Yellow
    $res = [DeviceInstaller]::RemoveRootDevice($MediaClassGuid, $HardwareId)
    if ($res) {
        Write-Host "[MCVAD] Device node berhasil dihapus dari Device Manager." -ForegroundColor Green
    } else {
        Write-Host "[MCVAD] Tidak ditemukan device node $HardwareId." -ForegroundColor Gray
    }
} else {
    Write-Host "[MCVAD] Mendaftarkan root device node: $HardwareId..." -ForegroundColor Cyan
    $res = [DeviceInstaller]::CreateRootDevice($MediaClassGuid, $HardwareId, $DeviceDesc)
    if ($res) {
        Write-Host "[MCVAD] Root device node berhasil dibuat di Device Manager!" -ForegroundColor Green
    } else {
        Write-Host "[MCVAD] Gagal membuat device node (mungkin sudah ada atau izin tidak memadai)." -ForegroundColor Yellow
    }
}
