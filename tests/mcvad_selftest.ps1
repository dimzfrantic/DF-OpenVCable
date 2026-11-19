<#
.SYNOPSIS
    Self-test lingkungan & status instalasi driver DF-OpenVCable (mcvad.sys).

.DESCRIPTION
    Memeriksa SETIAP prasyarat dan artefak yang harus ada agar 5 pasang virtual
    audio cable bisa berfungsi, lalu melaporkan PASS / WARN / FAIL per item.

    Skrip ini TIDAK mengubah apa pun (read-only). Aman dijalankan berulang.

.PARAMETER Json
    Keluarkan hasil dalam format JSON (untuk konsumsi otomatis / CI).

.PARAMETER CertSubject
    Substring subject sertifikat test-signing yang dicari.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\mcvad_selftest.ps1 -Json

.NOTES
    Exit code = jumlah item berstatus FAIL (0 = semua prasyarat terpenuhi).
#>
[CmdletBinding()]
param(
    [switch]$Json,
    [string]$CertSubject = 'MCVAD CI Test Certificate'
)

$ErrorActionPreference = 'Continue'
$script:Results = New-Object System.Collections.ArrayList

function Add-Result {
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][ValidateSet('PASS','WARN','FAIL','INFO')][string]$Status,
        [string]$Detail = ''
    )
    $null = $script:Results.Add([pscustomobject]@{
        Check  = $Name
        Status = $Status
        Detail = $Detail
    })
    if (-not $Json) {
        $tag = switch ($Status) {
            'PASS' { '[ OK ]' }
            'WARN' { '[WARN]' }
            'FAIL' { '[FAIL]' }
            default { '[INFO]' }
        }
        Write-Host ("{0} {1,-42} {2}" -f $tag, $Name, $Detail)
    }
}

# ---------------------------------------------------------------- 1. Elevasi
$isAdmin = $false
try {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $isAdmin = (New-Object Security.Principal.WindowsPrincipal($id)).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
} catch { }

if ($isAdmin) {
    Add-Result 'Hak Administrator' 'PASS' 'Proses berjalan elevated'
} else {
    Add-Result 'Hak Administrator' 'WARN' 'Tidak elevated - cek instalasi tetap jalan, tapi langkah perbaikan butuh admin'
}

# ------------------------------------------------------- 2. Test Signing mode
$testSigning = $null
$secureBoot = $null
try {
    $bcd = & bcdedit /enum '{current}' 2>&1 | Out-String
    if ($bcd -match '(?im)^\s*testsigning\s+(\S+)') { $testSigning = $Matches[1] }
} catch { }

if ($testSigning -eq 'Yes') {
    Add-Result 'Windows Test Signing' 'PASS' 'Aktif - driver test-signed boleh dimuat'
} elseif ($testSigning) {
    Add-Result 'Windows Test Signing' 'FAIL' "Nilai: $testSigning (butuh 'Yes'; jalankan: bcdedit /set testsigning on + restart)"
} else {
    Add-Result 'Windows Test Signing' 'FAIL' 'Tidak terdeteksi di BCD - driver test-signed akan ditolak'
}

try {
    $secureBoot = Confirm-SecureBootUEFI -ErrorAction Stop
} catch { $secureBoot = $null }
if ($secureBoot -eq $true) {
    Add-Result 'Secure Boot' 'WARN' 'Aktif - Test Signing mungkin ditolak firmware; pertimbangkan matikan sementara'
} elseif ($secureBoot -eq $false) {
    Add-Result 'Secure Boot' 'PASS' 'Nonaktif'
} else {
    Add-Result 'Secure Boot' 'INFO' 'Tidak dapat dibaca (legacy BIOS / bukan UEFI)'
}

# --------------------------------------------------------- 3. Sertifikat test
$certRoot = $null
$certPub  = $null
foreach ($store in @('Root','TrustedPublisher')) {
    $found = Get-ChildItem "Cert:\LocalMachine\$store" -ErrorAction SilentlyContinue |
             Where-Object { $_.Subject -like "*$CertSubject*" } |
             Select-Object -First 1
    if ($store -eq 'Root') { $certRoot = $found } else { $certPub = $found }
}

if ($certRoot -and $certPub) {
    Add-Result 'Sertifikat test-signing' 'PASS' "Terpasang di Root + TrustedPublisher (thumbprint $($certRoot.Thumbprint.Substring(0,8))...)"
} elseif ($certRoot -or $certPub) {
    $missing = if (-not $certRoot) { 'Root' } else { 'TrustedPublisher' }
    Add-Result 'Sertifikat test-signing' 'FAIL' "Hanya ada di satu store; kurang di $missing (jalankan setup_cert.bat)"
} else {
    Add-Result 'Sertifikat test-signing' 'FAIL' "Tidak ditemukan di Root/TrustedPublisher (jalankan setup_cert.bat)"
}

# ------------------------------------------------ 4. Paket driver di DriverStore
$storeHit = Get-ChildItem 'C:\Windows\System32\DriverStore\FileRepository' -Directory -Filter 'mcvad*' -ErrorAction SilentlyContinue |
            Select-Object -First 1
if ($storeHit) {
    $hasSys = Test-Path (Join-Path $storeHit.FullName 'mcvad.sys')
    $hasCat = Test-Path (Join-Path $storeHit.FullName 'mcvad.cat')
    if ($hasSys -and $hasCat) {
        Add-Result 'Driver di DriverStore' 'PASS' "$($storeHit.Name) (sys + cat ada)"
    } else {
        $miss = @(); if (-not $hasSys) { $miss += 'sys' }; if (-not $hasCat) { $miss += 'cat' }
        Add-Result 'Driver di DriverStore' 'FAIL' "$($storeHit.Name) tidak lengkap, kurang: $($miss -join ', ')"
    }
} else {
    Add-Result 'Driver di DriverStore' 'FAIL' 'Tidak ada paket mcvad* (belum diinstal / pnputil gagal)'
}

# -------------------------------------------------------------- 5. Service entry
$svcKey = 'HKLM:\SYSTEM\CurrentControlSet\Services\mcvad'
if (Test-Path $svcKey) {
    $svc = Get-ItemProperty $svcKey
    Add-Result 'Service registry' 'PASS' "ImagePath=$($svc.ImagePath) Start=$($svc.Start)"
} else {
    Add-Result 'Service registry' 'FAIL' 'HKLM\SYSTEM\CurrentControlSet\Services\mcvad tidak ada'
}

# ----------------------------------------------------------------- 6. Devnode
$dev = $null
try {
    $dev = Get-PnpDevice -InstanceId 'ROOT\MCVAD\*' -ErrorAction SilentlyContinue | Select-Object -First 1
} catch { }
if (-not $dev) {
    try {
        $dev = Get-PnpDevice -ErrorAction SilentlyContinue |
               Where-Object { $_.InstanceId -like 'ROOT\MCVAD*' } | Select-Object -First 1
    } catch { }
}

if ($dev) {
    if ($dev.Status -eq 'OK') {
        Add-Result 'Devnode ROOT\MCVAD' 'PASS' "$($dev.InstanceId) status OK"
    } else {
        $problem = 'n/a'
        try {
            $p = Get-PnpDeviceProperty -InstanceId $dev.InstanceId -KeyName 'DEVPKEY_Device_ProblemCode' -ErrorAction Stop
            $problem = $p.Data
        } catch { }
        Add-Result 'Devnode ROOT\MCVAD' 'FAIL' "Status=$($dev.Status) ProblemCode=$problem (lihat: pnputil /enum-devices /problem)"
    }
} else {
    Add-Result 'Devnode ROOT\MCVAD' 'FAIL' 'Device node belum dibuat (jalankan create_device.ps1 sebagai admin)'
}

# --------------------------------------------------------- 7. Driver ter-load
$drv = $null
try {
    $drv = Get-CimInstance Win32_SystemDriver -Filter "Name='mcvad'" -ErrorAction SilentlyContinue
} catch { }
if ($drv) {
    if ($drv.State -eq 'Running') {
        Add-Result 'Driver berjalan' 'PASS' "State=Running"
    } else {
        Add-Result 'Driver berjalan' 'WARN' "State=$($drv.State) - driver terdaftar tapi belum aktif (start manual / butuh restart)"
    }
} else {
    Add-Result 'Driver berjalan' 'FAIL' 'Tidak terdaftar sebagai system driver yang dimuat'
}

# ------------------------------------------------------------ 8. Audio endpoint
# Catatan: pola sengaja mengecualikan "VB-Audio" supaya Virtual Audio Cable
# komersial (jika terpasang) tidak terhitung sebagai endpoint DF-OpenVCable.
$endpoints = @()
try {
    $endpoints = Get-PnpDevice -Class AudioEndpoint -ErrorAction SilentlyContinue |
                 Where-Object {
                     $_.FriendlyName -match 'DF-OpenVCable|Virtual Cable (1|[ABCD])\b' -and
                     $_.FriendlyName -notmatch 'VB-Audio|Voicemeeter'
                 }
} catch { }

$renderCount  = @($endpoints | Where-Object { $_.FriendlyName -match 'Playback|Output|Render' }).Count
$captureCount = @($endpoints | Where-Object { $_.FriendlyName -match 'Record|Input|Capture' }).Count

if ($endpoints.Count -ge 10) {
    Add-Result 'Audio endpoint terdeteksi' 'PASS' "$($endpoints.Count) endpoint (harap 10: 5 playback + 5 recording)"
} elseif ($endpoints.Count -gt 0) {
    Add-Result 'Audio endpoint terdeteksi' 'WARN' "$($endpoints.Count) endpoint saja (target 10). Nama: $(($endpoints.FriendlyName | Sort-Object -Unique) -join '; ')"
} else {
    Add-Result 'Audio endpoint terdeteksi' 'FAIL' 'Tidak ada endpoint audio virtual yang muncul'
}

foreach ($e in ($endpoints | Sort-Object FriendlyName)) {
    Add-Result "  endpoint: $($e.FriendlyName)" 'INFO' "Status=$($e.Status)"
}

# ------------------------------------------------- 9. Bugcheck / BSOD terakhir
$bugcheck = $null
try {
    $since = (Get-Date).AddHours(-24)
    $bugcheck = Get-WinEvent -FilterHashtable @{
        LogName   = 'System'
        Id        = 1001
        StartTime = $since
    } -ErrorAction SilentlyContinue | Where-Object { $_.ProviderName -eq 'Microsoft-Windows-WER-SystemErrorReporting' } |
        Select-Object -First 1
} catch { }
if ($bugcheck) {
    $firstLine = ($bugcheck.Message -split "`r?`n")[0]
    Add-Result 'Bugcheck 24 jam terakhir' 'WARN' "Ada BSOD: $firstLine"
} else {
    Add-Result 'Bugcheck 24 jam terakhir' 'PASS' 'Tidak ada bugcheck tercatat'
}

# ------------------------------------------------------ 10. Status driver build
$sysCandidates = @(
    (Join-Path $PSScriptRoot '..\package\mcvad.sys'),   # layout repo
    (Join-Path $PSScriptRoot '..\mcvad.sys')            # layout paket ZIP
)
$sysLocal = $sysCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($sysLocal) {
    $fi = Get-Item $sysLocal
    $hash = (Get-FileHash $sysLocal -Algorithm SHA256).Hash
    Add-Result 'Binary mcvad.sys lokal' 'INFO' "$($fi.Length) bytes, SHA256 $($hash.Substring(0,16))..."
} else {
    Add-Result 'Binary mcvad.sys lokal' 'INFO' 'Tidak ditemukan di package\ maupun folder paket'
}

# --------------------------------------------------------------------- Ringkas
$fail = @($script:Results | Where-Object { $_.Status -eq 'FAIL' }).Count
$warn = @($script:Results | Where-Object { $_.Status -eq 'WARN' }).Count
$pass = @($script:Results | Where-Object { $_.Status -eq 'PASS' }).Count

if ($Json) {
    [pscustomobject]@{
        Timestamp = (Get-Date).ToString('s')
        Summary   = [pscustomobject]@{ Pass = $pass; Warn = $warn; Fail = $fail }
        Checks    = $script:Results
    } | ConvertTo-Json -Depth 4
} else {
    Write-Host ''
    Write-Host ('=' * 78)
    Write-Host ("RINGKASAN: {0} PASS, {1} WARN, {2} FAIL" -f $pass, $warn, $fail)
    if ($fail -gt 0) {
        Write-Host 'Langkah perbaikan (urut):'
        if (-not ($testSigning -eq 'Yes')) { Write-Host '  1. bcdedit /set testsigning on   lalu RESTART' }
        if (-not ($certRoot -and $certPub)) { Write-Host '  2. Jalankan package\setup_cert.bat sebagai Administrator' }
        if (-not $storeHit) { Write-Host '  3. Jalankan package\install.bat sebagai Administrator' }
        if (-not $dev) { Write-Host '  4. Jalankan package\create_device.ps1 sebagai Administrator' }
        Write-Host '  Panduan lengkap: docs\VALIDATION.md'
    } else {
        Write-Host 'Semua prasyarat terpenuhi. Lanjut ke uji audio: tests\wasapi_loopback.ps1 -Test'
    }
    Write-Host ('=' * 78)
}

exit $fail
