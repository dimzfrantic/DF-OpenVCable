<#
.SYNOPSIS
    DF-OpenVCable - Endpoint Heal.
    Memastikan endpoint audio DF-OpenVCable benar-benar terbentuk oleh
    AudioEndpointBuilder (AEB) setelah Windows boot, tanpa perlu user
    men-disable/enable perangkat di Device Manager.

.DESCRIPTION
    Masalah yang diselesaikan:
    Devnode ROOT\MEDIA\xxxx (service mcvad) sudah StartDevice dengan sukses dan
    mendaftarkan seluruh subdevice + interface KS (KSCATEGORY_AUDIO / RENDER /
    CAPTURE / REALTIME / TOPOLOGY). Tetapi setelah cold boot, AEB kadang tidak
    pernah membangun endpoint dari interface tersebut, sehingga Sound Settings
    kosong. Mematikan lalu menyalakan perangkat di Device Manager memaksa
    PnP mengirim ulang notifikasi arrival, dan endpoint langsung muncul.

    Skrip ini meniru langkah itu secara otomatis: kalau endpoint DF-OpenVCable
    tidak ada setelah boot, devnode di-restart (pnputil /restart-device).
    Kalau endpoint sudah normal, skrip tidak melakukan apa-apa.

.PARAMETER Install
    Daftarkan scheduled task "DF-OpenVCable Endpoint Heal" (trigger AtStartup,
    berjalan sebagai SYSTEM) supaya self-heal jalan otomatis tiap boot,
    lalu jalankan heal sekali sekarang. Butuh Administrator.

.PARAMETER Uninstall
    Hapus scheduled task. Butuh Administrator.

.PARAMETER Force
    Restart devnode walaupun jumlah endpoint sudah terlihat normal.

.PARAMETER TimeoutSec
    Batas waktu menunggu endpoint muncul setelah restart devnode. Default 90.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File Repair-DFEndpoints.ps1
    powershell -ExecutionPolicy Bypass -File Repair-DFEndpoints.ps1 -Install

.NOTES
    Exit code: 0 = endpoint OK / tidak ada yang perlu dilakukan,
               1 = endpoint tetap tidak muncul,
               2 = butuh Administrator.
#>
[CmdletBinding()]
param(
    [switch]$Install,
    [switch]$Uninstall,
    [switch]$Force,
    [int]$TimeoutSec = 90
)

$ErrorActionPreference = 'Continue'

$TaskName       = 'DF-OpenVCable Endpoint Heal'
$DataDir        = Join-Path $env:ProgramData 'DF-OpenVCable'
$LogPath        = Join-Path $DataDir 'endpoint_heal.log'
$InstalledCopy  = Join-Path $DataDir 'Repair-DFEndpoints.ps1'
$ExpectedCount  = 10   # 5 render + 5 capture

# ---------------------------------------------------------------
# Helper
# ---------------------------------------------------------------
function Test-Elevated {
    try {
        $id = [Security.Principal.WindowsIdentity]::GetCurrent()
        return ([Security.Principal.WindowsPrincipal]$id).IsInRole(
            [Security.Principal.WindowsBuiltInRole]::Administrator)
    } catch { return $false }
}

function Write-Log {
    param([string]$Message, [string]$Level = 'INFO')
    $line = '{0} [{1}] {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Level, $Message
    Write-Host $line
    try {
        if (-not (Test-Path -LiteralPath $DataDir)) {
            New-Item -ItemType Directory -Path $DataDir -Force | Out-Null
        }
        Add-Content -LiteralPath $LogPath -Value $line -Encoding UTF8 -ErrorAction SilentlyContinue
    } catch { }
}

function Get-DfDevices {
    param([switch]$IncludeNotOk)
    $devs = @(Get-PnpDevice -ErrorAction SilentlyContinue | Where-Object {
        $_.InstanceId -like 'ROOT\MCVAD*' -or
        ($_.InstanceId -like 'ROOT\MEDIA\*' -and $_.FriendlyName -like '*OpenVCable*')
    })
    if (-not $IncludeNotOk) {
        $devs = @($devs | Where-Object { $_.Status -eq 'OK' })
    }
    return $devs
}

function Get-DfEndpointCount {
    <#
      Hitung endpoint DF-OpenVCable yang benar-benar hidup (Status OK).
      Endpoint "phantom" (sisa cache dari sesi sebelumnya) tidak dihitung.
    #>
    $eps = @(Get-PnpDevice -Class AudioEndpoint -ErrorAction SilentlyContinue | Where-Object {
        $_.Status -eq 'OK' -and $_.FriendlyName -like 'DF-OpenVCable*'
    })
    return $eps.Count
}

function Wait-AudioStack {
    param([int]$MaxSeconds = 120)
    $deadline = (Get-Date).AddSeconds($MaxSeconds)
    while ((Get-Date) -lt $deadline) {
        $svc = @(Get-Service Audiosrv, AudioEndpointBuilder -ErrorAction SilentlyContinue)
        if ($svc.Count -eq 2 -and ($svc | Where-Object { $_.Status -ne 'Running' }).Count -eq 0) {
            return $true
        }
        Start-Sleep -Seconds 2
    }
    return $false
}

function Wait-Endpoints {
    param([int]$Seconds, [int]$Target)
    $deadline = (Get-Date).AddSeconds($Seconds)
    $last = -1
    while ((Get-Date) -lt $deadline) {
        $last = Get-DfEndpointCount
        if ($last -ge $Target) { return $last }
        Start-Sleep -Seconds 2
    }
    return $last
}

# ---------------------------------------------------------------
# Uninstall
# ---------------------------------------------------------------
if ($Uninstall) {
    if (-not (Test-Elevated)) {
        Write-Log 'Uninstall butuh Administrator.' 'ERROR'
        exit 2
    }
    $t = Get-ScheduledTask -TaskName $TaskName -ErrorAction SilentlyContinue
    if ($t) {
        Unregister-ScheduledTask -TaskName $TaskName -Confirm:$false
        Write-Log "Scheduled task '$TaskName' dihapus."
    } else {
        Write-Log "Scheduled task '$TaskName' tidak ada."
    }
    exit 0
}

# ---------------------------------------------------------------
# Install (self-heal tiap boot)
# ---------------------------------------------------------------
if ($Install) {
    if (-not (Test-Elevated)) {
        Write-Log 'Install scheduled task butuh Administrator.' 'ERROR'
        exit 2
    }

    $devs = @(Get-DfDevices -IncludeNotOk)
    if ($devs.Count -eq 0) {
        Write-Log 'Devnode DF-OpenVCable tidak ditemukan. Install driver dulu (install.bat).' 'ERROR'
        exit 1
    }

    if (-not (Test-Path -LiteralPath $DataDir)) {
        New-Item -ItemType Directory -Path $DataDir -Force | Out-Null
    }
    if ($PSCommandPath -and ($PSCommandPath -ne $InstalledCopy)) {
        Copy-Item -LiteralPath $PSCommandPath -Destination $InstalledCopy -Force
        Write-Log "Skrip disalin ke $InstalledCopy"
    }

    $action = New-ScheduledTaskAction -Execute 'powershell.exe' `
        -Argument ('-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "{0}"' -f $InstalledCopy)
    $trigger = New-ScheduledTaskTrigger -AtStartup
    $principal = New-ScheduledTaskPrincipal -UserId 'SYSTEM' -LogonType ServiceAccount -RunLevel Highest
    $settings = New-ScheduledTaskSettingsSet -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries `
        -StartWhenAvailable -MultipleInstances IgnoreNew `
        -ExecutionTimeLimit (New-TimeSpan -Minutes 5)

    Register-ScheduledTask -TaskName $TaskName -Action $action -Trigger $trigger `
        -Principal $principal -Settings $settings -Force `
        -Description 'Cek endpoint DF-OpenVCable setelah boot; restart devnode bila AudioEndpointBuilder tidak membangun endpoint.' | Out-Null

    Write-Log "Scheduled task '$TaskName' terdaftar (AtStartup, SYSTEM)."
}

# ---------------------------------------------------------------
# Heal
# ---------------------------------------------------------------
Write-Log '--- Endpoint heal mulai ---'

$devs = @(Get-DfDevices)
$allDevs = @(Get-DfDevices -IncludeNotOk)

if ($allDevs.Count -eq 0) {
    Write-Log 'Devnode DF-OpenVCable tidak ada (driver belum terpasang / sudah di-uninstall). Tidak ada yang dikerjakan.' 'WARN'
    exit 0
}

if (-not (Wait-AudioStack -MaxSeconds 120)) {
    Write-Log 'Audio Service / AudioEndpointBuilder tidak kunjung Running. Heal dibatalkan.' 'ERROR'
    exit 1
}

# Beri AEB waktu menyelesaikan enumerasi awal sebelum menyimpulkan endpoint hilang.
Start-Sleep -Seconds 4
$before = Get-DfEndpointCount
Write-Log ("Endpoint hidup sebelum heal: {0} (harapan {1})" -f $before, $ExpectedCount)

if ($before -ge $ExpectedCount -and -not $Force) {
    Write-Log 'Endpoint sudah lengkap. Tidak ada tindakan.'
    exit 0
}

if (-not (Test-Elevated)) {
    Write-Log 'Endpoint belum lengkap tetapi skrip tidak berjalan sebagai Administrator.' 'ERROR'
    Write-Log 'Jalankan fix_endpoints.bat (klik kanan > Run as administrator) atau install_endpoint_heal.bat.' 'ERROR'
    exit 2
}

# Devnode yang tertinggal disabled harus dinyalakan dulu, bukan di-restart.
foreach ($d in $allDevs) {
    if ($d.Status -ne 'OK') {
        Write-Log ("Devnode {0} berstatus {1} - mencoba Enable-PnpDevice." -f $d.InstanceId, $d.Status) 'WARN'
        try {
            Enable-PnpDevice -InstanceId $d.InstanceId -Confirm:$false -ErrorAction Stop
            Write-Log ("Devnode {0} diaktifkan." -f $d.InstanceId)
        } catch {
            Write-Log ("Enable gagal: " + $_.Exception.Message) 'ERROR'
        }
    }
}

$devs = @(Get-DfDevices)
if ($devs.Count -eq 0) {
    Write-Log 'Tidak ada devnode OK setelah enable. Heal berhenti.' 'ERROR'
    exit 1
}

foreach ($d in $devs) {
    Write-Log ("Restart devnode {0} ({1}) ..." -f $d.InstanceId, $d.FriendlyName)
    $out = & pnputil.exe /restart-device $d.InstanceId 2>&1
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Write-Log ("pnputil /restart-device gagal (exit {0}): {1}" -f $code, ($out -join ' ')) 'ERROR'
    }
}

$after = Wait-Endpoints -Seconds $TimeoutSec -Target $ExpectedCount
Write-Log ("Endpoint hidup setelah heal: {0} (harapan {1})" -f $after, $ExpectedCount)

if ($after -ge $ExpectedCount) {
    Write-Log 'Endpoint DF-OpenVCable normal.' 
    exit 0
} elseif ($after -gt 0) {
    Write-Log 'Sebagian endpoint muncul. Cek Sound Settings; bila masih kurang jalankan ulang.' 'WARN'
    exit 1
} else {
    Write-Log 'Endpoint tetap tidak muncul. Periksa docs/VALIDATION.md dan tests/diagnose_startdevice.ps1.' 'ERROR'
    exit 1
}
