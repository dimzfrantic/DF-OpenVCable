using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Security.Principal;
using System.Text;
using System.Windows.Forms;
using Microsoft.Win32;

namespace DFOpenVCable
{
    public class WatermarkPatcherForm : Form
    {
        private Label lblTitle;
        private Label lblDesc;
        private Label lblStatus;
        private Button btnHide;
        private Button btnRestore;

        public WatermarkPatcherForm()
        {
            InitializeComponent();
            UpdateStatus();
        }

        private void InitializeComponent()
        {
            this.Text = "DF-OpenVCable Watermark Manager";
            this.Size = new Size(520, 270);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Font = new Font("Segoe UI", 9F, FontStyle.Regular);

            lblTitle = new Label
            {
                Text = "DF-OpenVCable - Watermark Manager (Optional)",
                Font = new Font("Segoe UI", 11F, FontStyle.Bold),
                Location = new Point(20, 15),
                Size = new Size(470, 28)
            };

            lblDesc = new Label
            {
                Text = "Utilitas terpisah ini digunakan untuk menyembunyikan tulisan \'Test Mode\' di pojok kanan bawah desktop Windows.\r\n\r\nFile sistem asli otomatis dicadangkan (.bak) dan dapat dikembalikan kapan saja.",
                Location = new Point(20, 48),
                Size = new Size(470, 58),
                Font = new Font("Segoe UI", 9F, FontStyle.Regular)
            };

            lblStatus = new Label
            {
                Text = "Status: Memeriksa...",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Location = new Point(20, 115),
                Size = new Size(470, 24)
            };

            btnHide = new Button
            {
                Text = "Sembunyikan Watermark",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Location = new Point(20, 155),
                Size = new Size(210, 38)
            };
            btnHide.Click += BtnHide_Click;

            btnRestore = new Button
            {
                Text = "Kembalikan Asli",
                Location = new Point(245, 155),
                Size = new Size(160, 38)
            };
            btnRestore.Click += BtnRestore_Click;

            this.Controls.Add(lblTitle);
            this.Controls.Add(lblDesc);
            this.Controls.Add(lblStatus);
            this.Controls.Add(btnHide);
            this.Controls.Add(btnRestore);
        }

        private static bool IsAdmin()
        {
            var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }

        private static void RunCommand(string file, string args)
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = file,
                    Arguments = args,
                    CreateNoWindow = true,
                    UseShellExecute = false
                };
                using (var p = Process.Start(psi))
                {
                    p.WaitForExit();
                }
            }
            catch { }
        }

        private List<string> GetTargetMuiFiles()
        {
            var results = new List<string>();
            string sysDir = Environment.GetFolderPath(Environment.SpecialFolder.System);
            string winDir = Path.GetDirectoryName(sysDir);
            string brandingDir = Path.Combine(winDir, "Branding", "Basebrd");

            try
            {
                foreach (var dir in Directory.GetDirectories(sysDir))
                {
                    string candidate = Path.Combine(dir, "shell32.dll.mui");
                    if (File.Exists(candidate) && !results.Contains(candidate))
                    {
                        results.Add(candidate);
                    }
                }
            }
            catch { }

            try
            {
                if (Directory.Exists(brandingDir))
                {
                    foreach (var dir in Directory.GetDirectories(brandingDir))
                    {
                        string candidate = Path.Combine(dir, "basebrd.dll.mui");
                        if (File.Exists(candidate) && !results.Contains(candidate))
                        {
                            results.Add(candidate);
                        }
                    }
                }
            }
            catch { }

            return results;
        }

        private bool HasBackup()
        {
            foreach (var file in GetTargetMuiFiles())
            {
                if (File.Exists(file + ".bak")) return true;
            }
            return false;
        }

        private void UpdateStatus()
        {
            if (HasBackup())
            {
                lblStatus.Text = "Status: Watermark telah disembunyikan (Backup .bak tersedia)";
                lblStatus.ForeColor = Color.Green;
                btnRestore.Enabled = true;
            }
            else
            {
                lblStatus.Text = "Status: Watermark sistem asli aktif (Belum di-patch)";
                lblStatus.ForeColor = Color.FromArgb(60, 60, 60);
                btnRestore.Enabled = false;
            }
        }

        private void StopExplorer()
        {
            RunCommand("taskkill.exe", "/f /im explorer.exe");
            System.Threading.Thread.Sleep(600);
        }

        private void StartExplorer()
        {
            System.Threading.Thread.Sleep(300);
            Process.Start("explorer.exe");
        }

        private static bool ReplaceStringWithSpaces(byte[] bytes, string target)
        {
            byte[] pattern = Encoding.Unicode.GetBytes(target);
            if (pattern.Length == 0 || bytes.Length < pattern.Length) return false;

            bool found = false;
            byte[] spaceReplacement = Encoding.Unicode.GetBytes(new string(' ', target.Length));

            for (int i = 0; i <= bytes.Length - pattern.Length; i += 2)
            {
                bool match = true;
                for (int k = 0; k < pattern.Length; k++)
                {
                    if (bytes[i + k] != pattern[k]) { match = false; break; }
                }
                if (match)
                {
                    Array.Copy(spaceReplacement, 0, bytes, i, spaceReplacement.Length);
                    found = true;
                }
            }
            return found;
        }

        private bool PatchSingleFile(string filePath)
        {
            string bakPath = filePath + ".bak";

            RunCommand("takeown.exe", string.Format("/f \"{0}\"", filePath));
            RunCommand("icacls.exe", string.Format("\"{0}\" /grant administrators:F", filePath));

            if (!File.Exists(bakPath))
            {
                File.Copy(filePath, bakPath, true);
            }

            byte[] bytes = File.ReadAllBytes(filePath);
            bool modified = false;
            string fileName = Path.GetFileName(filePath).ToLowerInvariant();

            if (fileName == "shell32.dll.mui")
            {
                string[] shellTargets = new string[]
                {
                    "Test Mode",
                    "Mode Uji",
                    "Evaluation Copy",
                    "Salinan Evaluasi",
                    "%wsMicrosoft (R) Windows (R) (Build %ws%0.0ws)  %ws",
                    "%wsMicrosoft (R) Windows (R) (Build %ws: %ws)  %ws",
                    "Microsoft (R) Windows (R) (Build %ws%0.0ws)",
                    "Microsoft (R) Windows (R) (Build %ws: %ws)"
                };

                foreach (var t in shellTargets)
                {
                    if (ReplaceStringWithSpaces(bytes, t)) modified = true;
                }
            }
            else if (fileName == "basebrd.dll.mui")
            {
                string[] basebrdTargets = new string[]
                {
                    "Windows 10 Pro",
                    "Windows 10 Home",
                    "Windows 10 Enterprise",
                    "Windows 10 Education",
                    "Windows 11 Pro",
                    "Windows 11 Home",
                    "Windows 11 Enterprise",
                    "Windows 11 Education"
                };

                try
                {
                    using (var k = Registry.LocalMachine.OpenSubKey(@"SOFTWARE\Microsoft\Windows NT\CurrentVersion"))
                    {
                        if (k != null)
                        {
                            string prodName = k.GetValue("ProductName") as string;
                            if (!string.IsNullOrEmpty(prodName))
                            {
                                if (ReplaceStringWithSpaces(bytes, prodName)) modified = true;
                            }
                        }
                    }
                }
                catch { }

                foreach (var t in basebrdTargets)
                {
                    if (ReplaceStringWithSpaces(bytes, t)) modified = true;
                }
            }

            if (modified)
            {
                File.WriteAllBytes(filePath, bytes);
            }
            return modified;
        }

        private bool RestoreSingleFile(string filePath)
        {
            string bakPath = filePath + ".bak";
            if (!File.Exists(bakPath)) return false;

            RunCommand("takeown.exe", string.Format("/f \"{0}\"", filePath));
            RunCommand("icacls.exe", string.Format("\"{0}\" /grant administrators:F", filePath));

            File.Copy(bakPath, filePath, true);
            File.Delete(bakPath);
            return true;
        }

        private void BtnHide_Click(object sender, EventArgs e)
        {
            var files = GetTargetMuiFiles();
            if (files.Count == 0)
            {
                MessageBox.Show("File sistem (shell32.dll.mui / basebrd.dll.mui) tidak ditemukan.", "Gagal", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return;
            }

            Cursor = Cursors.WaitCursor;
            btnHide.Enabled = false;
            btnRestore.Enabled = false;

            try
            {
                StopExplorer();

                int patchedCount = 0;
                foreach (var f in files)
                {
                    if (PatchSingleFile(f)) patchedCount++;
                }

                StartExplorer();
                UpdateStatus();

                MessageBox.Show(
                    "Watermark \'Test Mode\' berhasil disembunyikan!\r\n\r\nWindows Explorer telah dimuat ulang untuk menerapkan perubahan tampilan desktop.",
                    "Sukses", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                StartExplorer();
                MessageBox.Show("Gagal memodifikasi watermark: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                btnHide.Enabled = true;
                Cursor = Cursors.Default;
            }
        }

        private void BtnRestore_Click(object sender, EventArgs e)
        {
            var files = GetTargetMuiFiles();
            Cursor = Cursors.WaitCursor;
            btnHide.Enabled = false;
            btnRestore.Enabled = false;

            try
            {
                StopExplorer();

                foreach (var f in files)
                {
                    RestoreSingleFile(f);
                }

                StartExplorer();
                UpdateStatus();

                MessageBox.Show(
                    "Watermark asli berhasil dikembalikan!\r\n\r\nWindows Explorer telah dimuat ulang.",
                    "Sukses", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            catch (Exception ex)
            {
                StartExplorer();
                MessageBox.Show("Gagal mengembalikan watermark: " + ex.Message, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                btnHide.Enabled = true;
                Cursor = Cursors.Default;
            }
        }

        [STAThread]
        public static void Main()
        {
            if (!IsAdmin())
            {
                var psi = new ProcessStartInfo
                {
                    FileName = Application.ExecutablePath,
                    Verb = "runas",
                    UseShellExecute = true
                };
                try
                {
                    Process.Start(psi);
                }
                catch
                {
                    MessageBox.Show("Hak akses Administrator diperlukan untuk menjalankan Patch Watermark.", "Akses Ditolak", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                return;
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new WatermarkPatcherForm());
        }
    }
}