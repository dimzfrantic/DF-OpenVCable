import re, subprocess, os

path = r"E:\Hermes\Projects\VirtualAudioCable\tools\WatermarkPatcher.cs"
with open(path, "r", encoding="utf-8", errors="ignore") as f:
    c = f.read()

rep1 = r'Text = "Utilitas terpisah ini digunakan untuk menyembunyikan tulisan \'Test Mode\' di pojok kanan bawah desktop Windows.\r\n\r\nFile sistem asli otomatis dicadangkan (.bak) dan dapat dikembalikan kapan saja.",'
c = re.sub(r'Text\s*=\s*"Utilitas terpisah.*?kapan saja\.",', lambda m: rep1, c, flags=re.DOTALL)

rep2 = r'"Watermark \'Test Mode\' berhasil disembunyikan!\r\n\r\nWindows Explorer telah dimuat ulang untuk menerapkan perubahan tampilan desktop.",'
c = re.sub(r'"Watermark \'Test Mode\' berhasil disembunyikan!.*?tampilan desktop\.",', lambda m: rep2, c, flags=re.DOTALL)

rep3 = r'"Watermark asli berhasil dikembalikan!\r\n\r\nWindows Explorer telah dimuat ulang.",'
c = re.sub(r'"Watermark asli berhasil dikembalikan!.*?dimuat ulang\.",', lambda m: rep3, c, flags=re.DOTALL)

with open(path, "w", encoding="utf-8") as f:
    f.write(c)
print("Patch applied successfully.")

csc = r"C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
cwd = r"E:\Hermes\Projects\VirtualAudioCable\tools"
res = subprocess.run([
    csc, "/nologo", "/target:winexe", "/platform:x64", "/optimize+",
    "/r:System.Windows.Forms.dll", "/r:System.Drawing.dll",
    "/out:Remove-Watermark.exe", "WatermarkPatcher.cs"
], cwd=cwd, capture_output=True, text=True)
print("CSC STDOUT:", res.stdout)
print("CSC STDERR:", res.stderr)
print("CSC Return code:", res.returncode)

exe_path = os.path.join(cwd, "Remove-Watermark.exe")
if os.path.exists(exe_path):
    print(f"SUCCESS: {exe_path} exists, size = {os.path.getsize(exe_path)} bytes")
else:
    print("FAILURE: Remove-Watermark.exe was not created.")
