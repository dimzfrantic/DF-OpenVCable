<#
.SYNOPSIS
    Membaca & mendekode diagnostik StartDevice driver DF-OpenVCable (mcvad.sys).

.DESCRIPTION
    Driver menulis array record { Cable, Stage, Status } ke value REG_BINARY
    bernama StartDiag di HKLM\SOFTWARE\MCVAD setiap kali StartDevice berjalan.
    Skrip ini mendekode record tersebut menjadi tabel yang bisa dibaca, supaya
    kegagalan PcNewPort / Port::Init / PcRegisterSubdevice / PhysicalConnection
    bisa didiagnosis tanpa kernel debugger.

.PARAMETER Key
    Path registry tempat StartDiag ditulis. Default: HKLM:\SOFTWARE\MCVAD

.PARAMETER Raw
    Tampilkan juga hex mentah dari value StartDiag.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\diagnose_startdevice.ps1

.NOTES
    Exit code: 0 = semua stage SUCCESS, 1 = ada stage gagal, 2 = StartDiag belum ada.
#>
[CmdletBinding()]
param(
    [string]$Key = 'HKLM:\SOFTWARE\MCVAD',
    [switch]$Raw
)

$ErrorActionPreference = 'Stop'

$StageName = @{
    1 = 'WaveRT  PcNewPort'
    2 = 'WaveRT  Port::Init'
    3 = 'WaveRT  PcRegisterSubdevice'
    4 = 'Topo    PcNewPort'
    5 = 'Topo    Port::Init'
    6 = 'Topo    PcRegisterSubdevice'
    7 = 'PhysConn Playback'
    8 = 'PhysConn Capture'
}

$StatusName = @{
    0x00000000 = 'STATUS_SUCCESS'
    0xC0000001 = 'STATUS_UNSUCCESSFUL'
    0xC000000D = 'STATUS_INVALID_PARAMETER'
    0xC000000E = 'STATUS_NO_SUCH_DEVICE'
    0xC0000010 = 'STATUS_INVALID_DEVICE_REQUEST'
    0xC0000022 = 'STATUS_ACCESS_DENIED'
    0xC0000034 = 'STATUS_OBJECT_NAME_NOT_FOUND'
    0xC0000035 = 'STATUS_OBJECT_NAME_COLLISION'
    0xC000009A = 'STATUS_INSUFFICIENT_RESOURCES'
    0xC00000BB = 'STATUS_NOT_SUPPORTED'
    0xC0000185 = 'STATUS_IO_DEVICE_ERROR'
    0xC000020D = 'STATUS_DEVICE_NOT_READY'
}

$prop = Get-ItemProperty -Path $Key -Name StartDiag -ErrorAction SilentlyContinue
if (-not $prop -or -not $prop.StartDiag) {
    Write-Host "[INFO] Value 'StartDiag' belum ada di $Key" -ForegroundColor Yellow
    Write-Host "       Driver 1.0.0.2+ harus terpasang DAN StartDevice sudah dijalankan." -ForegroundColor Gray
    Write-Host "       Cek: sc query mcvad  |  Get-PnpDevice | Where InstanceId -like 'ROOT\MEDIA\*'" -ForegroundColor Gray
    exit 2
}

$bytes = $prop.StartDiag
if ($Raw) {
    Write-Host ("Hex mentah ({0} bytes): {1}" -f $bytes.Length, (($bytes | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')) -ForegroundColor DarkGray
    Write-Host ''
}

if (($bytes.Length % 12) -ne 0) {
    Write-Host "[WARN] Ukuran StartDiag tidak kelipatan 12 ($($bytes.Length) bytes) - format mungkin berbeda." -ForegroundColor Yellow
}

Write-Host '======================================================================'
Write-Host ' DF-OpenVCable - Diagnostik StartDevice'
Write-Host '======================================================================'

$fail = 0
$records = 0

for ($i = 0; ($i + 12) -le $bytes.Length; $i += 12) {
    $cable = [BitConverter]::ToUInt32($bytes, $i)
    $stage = [BitConverter]::ToUInt32($bytes, $i + 4)
    $st    = [BitConverter]::ToUInt32($bytes, $i + 8)

    $stageLabel = if ($StageName.ContainsKey([int]$stage)) { $StageName[[int]$stage] } else { "Stage #$stage" }
    $stName     = if ($StatusName.ContainsKey([uint32]$st)) { $StatusName[[uint32]$st] } else { '' }

    if ($st -eq 0) {
        $tag  = '[ OK ]'
        $color = 'Green'
    } else {
        $tag  = '[FAIL]'
        $color = 'Red'
        $fail++
    }

    $line = '{0} cable {1}  {2,-26} 0x{3:X8}  {4}' -f $tag, $cable, $stageLabel, $st, $stName
    Write-Host $line -ForegroundColor $color
    $records++
}

Write-Host '----------------------------------------------------------------------'
if ($fail -eq 0) {
    Write-Host "RINGKASAN: $records record, semua stage SUCCESS." -ForegroundColor Green
} else {
    Write-Host "RINGKASAN: $records record, $fail stage GAGAL." -ForegroundColor Red
}
Write-Host ''
Write-Host 'Catatan stage:' -ForegroundColor DarkGray
Write-Host '  Stage 3 gagal  -> subdevice WaveRT tidak terdaftar = endpoint tidak terbit' -ForegroundColor DarkGray
Write-Host '  Stage 6 gagal  -> subdevice Topology tidak terdaftar = tidak ada bridge fisik' -ForegroundColor DarkGray
Write-Host '  Stage 7/8 gagal -> PcRegisterPhysicalConnection ditolak PortCls' -ForegroundColor DarkGray

if ($fail -gt 0) { exit 1 } else { exit 0 }
