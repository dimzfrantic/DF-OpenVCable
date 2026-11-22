<#
.SYNOPSIS
    Uji fungsional DF-OpenVCable: mainkan tone ke endpoint playback, rekam dari
    endpoint recording kabel yang sama, lalu ukur apakah sinyal benar-benar lewat.

.DESCRIPTION
    Menggunakan WinMM (waveOut/waveIn) via P/Invoke sehingga tidak butuh
    dependensi eksternal. Format uji: PCM 16-bit stereo 48 kHz.

    Mode:
      -List   Tampilkan semua endpoint playback/recording yang cocok dengan pola.
      -Probe  Buka capture client dan ukur level 1 detik (membuktikan jalur
              capture hidup, tanpa perlu playback).
      -Test   (default) Mainkan tone 1 kHz sambil merekam, lalu hitung RMS dan
              magnitudo Goertzel pada frekuensi tone.

.PARAMETER Pattern
    Substring nama endpoint yang dicari. Default 'DF-OpenVCable'.

.PARAMETER DurationMs
    Durasi tone & rekaman dalam milidetik. Default 1500.

.PARAMETER Freq
    Frekuensi tone uji dalam Hz. Default 1000.

.PARAMETER MinRms
    Ambang RMS minimum agar dianggap sinyal terdeteksi. Default 0.02.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -List

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tests\audio_loopback.ps1 -Test -Pattern 'DF-OpenVCable 1'

.NOTES
    Exit code: 0 = PASS, 1 = FAIL, 2 = endpoint tidak ditemukan.
#>
[CmdletBinding()]
param(
    [switch]$List,
    [switch]$Probe,
    [switch]$Test,
    [string]$Pattern = 'DF-OpenVCable',
    [string]$RenderPattern = '',
    [string]$CapturePattern = '',
    [int]$DurationMs = 1500,
    [double]$Freq = 1000,
    [double]$MinRms = 0.02
)

$ErrorActionPreference = 'Stop'

if (-not ($List -or $Probe -or $Test)) { $Test = $true }

$csharp = @'
using System;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class VcAudio
{
    public const int WAVE_MAPPER = -1;
    public const int WAVE_FORMAT_PCM = 1;
    public const uint WHDR_DONE = 0x00000001;
    public const uint WHDR_PREPARED = 0x00000002;

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEFORMATEX
    {
        public ushort wFormatTag;
        public ushort nChannels;
        public uint   nSamplesPerSec;
        public uint   nAvgBytesPerSec;
        public ushort nBlockAlign;
        public ushort wBitsPerSample;
        public ushort cbSize;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct WAVEOUTCAPS
    {
        public ushort wMid;
        public ushort wPid;
        public uint   vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string szPname;
        public uint   dwFormats;
        public ushort wChannels;
        public ushort wReserved1;
        public uint   dwSupport;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
    public struct WAVEINCAPS
    {
        public ushort wMid;
        public ushort wPid;
        public uint   vDriverVersion;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string szPname;
        public uint   dwFormats;
        public ushort wChannels;
        public ushort wReserved1;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WAVEHDR
    {
        public IntPtr lpData;
        public uint   dwBufferLength;
        public uint   dwBytesRecorded;
        public IntPtr dwUser;
        public uint   dwFlags;
        public uint   dwLoops;
        public IntPtr lpNext;
        public IntPtr reserved;
    }

    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    static extern int waveOutGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    static extern int waveOutGetDevCaps(UIntPtr uDeviceID, ref WAVEOUTCAPS caps, uint cbCaps);
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    static extern int waveOutOpen(out IntPtr hwo, int uDeviceID, ref WAVEFORMATEX fmt, IntPtr cb, IntPtr inst, int flags);
    [DllImport("winmm.dll")]
    static extern int waveOutPrepareHeader(IntPtr hwo, IntPtr hdr, int cbHdr);
    [DllImport("winmm.dll")]
    static extern int waveOutWrite(IntPtr hwo, IntPtr hdr, int cbHdr);
    [DllImport("winmm.dll")]
    static extern int waveOutReset(IntPtr hwo);
    [DllImport("winmm.dll")]
    static extern int waveOutUnprepareHeader(IntPtr hwo, IntPtr hdr, int cbHdr);
    [DllImport("winmm.dll")]
    static extern int waveOutClose(IntPtr hwo);

    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    static extern int waveInGetNumDevs();
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    static extern int waveInGetDevCaps(UIntPtr uDeviceID, ref WAVEINCAPS caps, uint cbCaps);
    [DllImport("winmm.dll", CharSet = CharSet.Unicode)]
    static extern int waveInOpen(out IntPtr hwi, int uDeviceID, ref WAVEFORMATEX fmt, IntPtr cb, IntPtr inst, int flags);
    [DllImport("winmm.dll")]
    static extern int waveInPrepareHeader(IntPtr hwi, IntPtr hdr, int cbHdr);
    [DllImport("winmm.dll")]
    static extern int waveInUnprepareHeader(IntPtr hwi, IntPtr hdr, int cbHdr);
    [DllImport("winmm.dll")]
    static extern int waveInAddBuffer(IntPtr hwi, IntPtr hdr, int cbHdr);
    [DllImport("winmm.dll")]
    static extern int waveInStart(IntPtr hwi);
    [DllImport("winmm.dll")]
    static extern int waveInStop(IntPtr hwi);
    [DllImport("winmm.dll")]
    static extern int waveInReset(IntPtr hwi);
    [DllImport("winmm.dll")]
    static extern int waveInClose(IntPtr hwi);

    public static WAVEFORMATEX Pcm16Stereo(uint rate)
    {
        WAVEFORMATEX f = new WAVEFORMATEX();
        f.wFormatTag = WAVE_FORMAT_PCM;
        f.nChannels = 2;
        f.nSamplesPerSec = rate;
        f.wBitsPerSample = 16;
        f.nBlockAlign = (ushort)(f.nChannels * f.wBitsPerSample / 8);
        f.nAvgBytesPerSec = rate * f.nBlockAlign;
        f.cbSize = 0;
        return f;
    }

    public static string[] RenderNames()
    {
        int n = waveOutGetNumDevs();
        string[] r = new string[n];
        uint cb = (uint)Marshal.SizeOf(typeof(WAVEOUTCAPS));
        for (int i = 0; i < n; i++)
        {
            WAVEOUTCAPS c = new WAVEOUTCAPS();
            if (waveOutGetDevCaps((UIntPtr)(uint)i, ref c, cb) == 0)
                r[i] = c.szPname;
            else
                r[i] = "";
        }
        return r;
    }

    public static string[] CaptureNames()
    {
        int n = waveInGetNumDevs();
        string[] r = new string[n];
        uint cb = (uint)Marshal.SizeOf(typeof(WAVEINCAPS));
        for (int i = 0; i < n; i++)
        {
            WAVEINCAPS c = new WAVEINCAPS();
            if (waveInGetDevCaps((UIntPtr)(uint)i, ref c, cb) == 0)
                r[i] = c.szPname;
            else
                r[i] = "";
        }
        return r;
    }

    public static int FindRender(string pattern)
    {
        string[] names = RenderNames();
        for (int i = 0; i < names.Length; i++)
            if (names[i].IndexOf(pattern, StringComparison.OrdinalIgnoreCase) >= 0) return i;
        return -1;
    }

    public static int FindCapture(string pattern)
    {
        string[] names = CaptureNames();
        for (int i = 0; i < names.Length; i++)
            if (names[i].IndexOf(pattern, StringComparison.OrdinalIgnoreCase) >= 0) return i;
        return -1;
    }

    // Mengembalikan: [0]=rms, [1]=goertzel magnitude (0..1 relatif), [2]=bytes captured
    public static double[] Measure(int outDev, int inDev, int durationMs, double freq)
    {
        const uint RATE = 48000;
        WAVEFORMATEX fmt = Pcm16Stereo(RATE);
        int blockAlign = fmt.nBlockAlign;
        int totalBytes = (int)(RATE * blockAlign * durationMs / 1000);

        IntPtr hwi = IntPtr.Zero, hwo = IntPtr.Zero;
        IntPtr pInBuf = IntPtr.Zero, pInHdr = IntPtr.Zero, pOutBuf = IntPtr.Zero, pOutHdr = IntPtr.Zero;

        try
        {
            // ---- Siapkan capture ----
            int rc = waveInOpen(out hwi, inDev, ref fmt, IntPtr.Zero, IntPtr.Zero, 0);
            if (rc != 0) throw new Exception("waveInOpen gagal, MMRESULT=" + rc);

            pInBuf = Marshal.AllocHGlobal(totalBytes);
            for (int i = 0; i < totalBytes; i++) Marshal.WriteByte(pInBuf, i, 0);

            WAVEHDR inHdr = new WAVEHDR();
            inHdr.lpData = pInBuf;
            inHdr.dwBufferLength = (uint)totalBytes;
            pInHdr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(WAVEHDR)));
            Marshal.StructureToPtr(inHdr, pInHdr, false);

            rc = waveInPrepareHeader(hwi, pInHdr, Marshal.SizeOf(typeof(WAVEHDR)));
            if (rc != 0) throw new Exception("waveInPrepareHeader gagal, MMRESULT=" + rc);
            rc = waveInAddBuffer(hwi, pInHdr, Marshal.SizeOf(typeof(WAVEHDR)));
            if (rc != 0) throw new Exception("waveInAddBuffer gagal, MMRESULT=" + rc);
            rc = waveInStart(hwi);
            if (rc != 0) throw new Exception("waveInStart gagal, MMRESULT=" + rc);

            // ---- Siapkan & kirim tone ----
            if (outDev >= 0)
            {
                rc = waveOutOpen(out hwo, outDev, ref fmt, IntPtr.Zero, IntPtr.Zero, 0);
                if (rc != 0) throw new Exception("waveOutOpen gagal, MMRESULT=" + rc);

                pOutBuf = Marshal.AllocHGlobal(totalBytes);
                short[] pcm = new short[totalBytes / 2];
                double amp = 0.25 * short.MaxValue;
                for (int i = 0; i < pcm.Length; i++)
                {
                    double t = (double)(i / 2) / RATE;
                    pcm[i] = (short)(amp * Math.Sin(2.0 * Math.PI * freq * t));
                }
                Marshal.Copy(pcm, 0, pOutBuf, pcm.Length);

                WAVEHDR outHdr = new WAVEHDR();
                outHdr.lpData = pOutBuf;
                outHdr.dwBufferLength = (uint)totalBytes;
                pOutHdr = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(WAVEHDR)));
                Marshal.StructureToPtr(outHdr, pOutHdr, false);

                rc = waveOutPrepareHeader(hwo, pOutHdr, Marshal.SizeOf(typeof(WAVEHDR)));
                if (rc != 0) throw new Exception("waveOutPrepareHeader gagal, MMRESULT=" + rc);
                rc = waveOutWrite(hwo, pOutHdr, Marshal.SizeOf(typeof(WAVEHDR)));
                if (rc != 0) throw new Exception("waveOutWrite gagal, MMRESULT=" + rc);
            }

            Thread.Sleep(durationMs + 400);

            // ---- Ambil hasil ----
            if (hwi != IntPtr.Zero) { waveInStop(hwi); }
            WAVEHDR done = (WAVEHDR)Marshal.PtrToStructure(pInHdr, typeof(WAVEHDR));
            int captured = (int)done.dwBytesRecorded;

            double sumSq = 0.0;
            int nSamples = captured / 2;
            double[] mono = new double[nSamples];
            int m = 0;
            for (int i = 0; i + 3 < captured; i += blockAlign)
            {
                short l = Marshal.ReadInt16(pInBuf, i);
                short r = Marshal.ReadInt16(pInBuf, i + 2);
                double v = ((double)l + (double)r) / 2.0 / 32768.0;
                mono[m++] = v;
                sumSq += v * v;
            }

            double rms = (m > 0) ? Math.Sqrt(sumSq / m) : 0.0;

            // Goertzel pada freq target (relatif terhadap energi total)
            double mag = 0.0;
            if (m > 100)
            {
                double w = 2.0 * Math.PI * freq / RATE;
                double cw = Math.Cos(w), sw = Math.Sin(w);
                double coeff = 2.0 * cw;
                double s0 = 0, s1 = 0, s2 = 0;
                for (int i = 0; i < m; i++)
                {
                    s0 = mono[i] + coeff * s1 - s2;
                    s2 = s1;
                    s1 = s0;
                }
                double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
                double magAbs = Math.Sqrt(Math.Max(power, 0.0)) * 2.0 / m;
                mag = magAbs;
            }

            return new double[] { rms, mag, (double)captured };
        }
        finally
        {
            try { if (hwo != IntPtr.Zero) { waveOutReset(hwo); } } catch { }
            try { if (pOutHdr != IntPtr.Zero && hwo != IntPtr.Zero) waveOutUnprepareHeader(hwo, pOutHdr, Marshal.SizeOf(typeof(WAVEHDR))); } catch { }
            try { if (hwo != IntPtr.Zero) waveOutClose(hwo); } catch { }
            try { if (hwi != IntPtr.Zero) { waveInReset(hwi); } } catch { }
            try { if (pInHdr != IntPtr.Zero && hwi != IntPtr.Zero) waveInUnprepareHeader(hwi, pInHdr, Marshal.SizeOf(typeof(WAVEHDR))); } catch { }
            try { if (hwi != IntPtr.Zero) waveInClose(hwi); } catch { }
            if (pInBuf != IntPtr.Zero) Marshal.FreeHGlobal(pInBuf);
            if (pOutBuf != IntPtr.Zero) Marshal.FreeHGlobal(pOutBuf);
            if (pInHdr != IntPtr.Zero) Marshal.FreeHGlobal(pInHdr);
            if (pOutHdr != IntPtr.Zero) Marshal.FreeHGlobal(pOutHdr);
        }
    }
}
'@

if (-not ('VcAudio' -as [type])) {
    Add-Type -TypeDefinition $csharp -Language CSharp -ErrorAction Stop
}

function Show-Endpoints {
    Write-Host 'PLAYBACK (waveOut):'
    $r = [VcAudio]::RenderNames()
    for ($i = 0; $i -lt $r.Length; $i++) {
        $mark = if ($r[$i] -match [regex]::Escape($Pattern)) { ' <== cocok' } else { '' }
        Write-Host ("  [{0}] {1}{2}" -f $i, $r[$i], $mark)
    }
    Write-Host ''
    Write-Host 'RECORDING (waveIn):'
    $c = [VcAudio]::CaptureNames()
    for ($i = 0; $i -lt $c.Length; $i++) {
        $mark = if ($c[$i] -match [regex]::Escape($Pattern)) { ' <== cocok' } else { '' }
        Write-Host ("  [{0}] {1}{2}" -f $i, $c[$i], $mark)
    }
}

if ($List) {
    Show-Endpoints
    exit 0
}

$rPattern = if ($RenderPattern) { $RenderPattern } else { $Pattern }
$cPattern = if ($CapturePattern) { $CapturePattern } else { $Pattern }

$outDev = [VcAudio]::FindRender($rPattern)
$inDev  = [VcAudio]::FindCapture($cPattern)

Write-Host ("Pola playback  : '{0}'" -f $rPattern)
Write-Host ("Pola recording : '{0}'" -f $cPattern)
Write-Host ("Playback idx   : {0}" -f $(if ($outDev -ge 0) { $outDev } else { 'TIDAK DITEMUKAN' }))
Write-Host ("Recording idx  : {0}" -f $(if ($inDev -ge 0) { $inDev } else { 'TIDAK DITEMUKAN' }))
Write-Host ''

if ($inDev -lt 0) {
    Write-Host '[FAIL] Endpoint recording tidak ditemukan. Jalankan -List untuk melihat nama yang tersedia.'
    exit 2
}

try {
    if ($Probe) {
        $res = [VcAudio]::Measure(-1, $inDev, $DurationMs, $Freq)
        Write-Host ('Capture probe  : {0} bytes terekam' -f $res[2])
        Write-Host ('RMS            : {0:F5}' -f $res[0])
        if ($res[2] -le 0) {
            Write-Host '[FAIL] Tidak ada data yang terekam.'
            exit 1
        }
        Write-Host '[ OK ] Jalur capture hidup (data mengalir).'
        exit 0
    }

    if ($outDev -lt 0) {
        Write-Host '[FAIL] Endpoint playback tidak ditemukan. Jalankan -List.'
        exit 2
    }

    Write-Host ("Menguji tone {0} Hz selama {1} ms ..." -f $Freq, $DurationMs)
    $res = [VcAudio]::Measure($outDev, $inDev, $DurationMs, $Freq)

    $rms = $res[0]; $mag = $res[1]; $bytes = $res[2]
    Write-Host ''
    Write-Host ('Bytes terekam  : {0}' -f $bytes)
    Write-Host ('RMS            : {0:F5}  (ambang {1:F3})' -f $rms, $MinRms)
    Write-Host ('Magnitudo {0} Hz: {1:F5}' -f $Freq, $mag)
    Write-Host ''

    if ($bytes -le 0) {
        Write-Host '[FAIL] Tidak ada audio terekam dari kabel.'
        exit 1
    }
    if ($rms -lt $MinRms) {
        Write-Host '[FAIL] Sinyal terlalu lemah / senyap. Routing render->capture belum bekerja.'
        exit 1
    }
    if ($mag -lt 0.005) {
        Write-Host '[WARN] Ada sinyal, tetapi komponen tone 1 kHz sangat lemah (kemungkinan noise atau resampling).'
        Write-Host '[ OK ] Secara level: audio mengalir.'
        exit 0
    }

    Write-Host '[ OK ] PASS - audio mengalir dari playback ke recording pada kabel yang sama.'
    exit 0
}
catch {
    Write-Host ("[FAIL] {0}" -f $_.Exception.Message)
    exit 1
}
