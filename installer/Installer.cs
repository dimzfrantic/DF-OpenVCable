using System;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Security.Principal;
using System.Windows.Forms;
using Microsoft.Win32;

namespace DFOpenVCable
{
    public class InstallerForm : Form
    {
        private Label lblTitle;
        private GroupBox grpStatus;
        private Label lblSecureBoot;
        private Label lblTestMode;
        private Label lblDriverStatus;
        private Label lblLog;
        private Button btnInstall;
        private Button btnUninstall;
        private Button btnRestart;

        private bool isSecureBootActive = false;
        private bool isTestModeActive = false;
        private bool isDriverInstalled = false;

        private static readonly string AppInstallDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles),
            "DF-OpenVCable"
        );

        public InstallerForm()
        {
            InitializeComponent();
            RefreshStatus();
        }

        private void InitializeComponent()
        {
            this.Text = "DF-OpenVCable Setup & Driver Manager";
            this.Size = new Size(540, 340);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox = false;
            this.StartPosition = FormStartPosition.CenterScreen;
            this.Font = new Font("Segoe UI", 9F, FontStyle.Regular);

            lblTitle = new Label
            {
                Text = "DF-OpenVCable - Setup & Driver Manager",
                Font = new Font("Segoe UI", 12F, FontStyle.Bold),
                Location = new Point(20, 15),
                Size = new Size(480, 28)
            };

            grpStatus = new GroupBox
            {
                Text = " Status Sistem & Keamanan ",
                Location = new Point(20, 50),
                Size = new Size(485, 125)
            };

            lblSecureBoot = new Label
            {
                Text = "Memeriksa Secure Boot...",
                Location = new Point(15, 28),
                Size = new Size(455, 22),
                Font = new Font("Segoe UI", 9F, FontStyle.Regular)
            };

            lblTestMode = new Label
            {
                Text = "Memeriksa Test Signing...",
                Location = new Point(15, 56),
                Size = new Size(455, 22),
                Font = new Font("Segoe UI", 9F, FontStyle.Regular)
            };

            lblDriverStatus = new Label
            {
                Text = "Memeriksa Status Driver...",
                Location = new Point(15, 84),
                Size = new Size(455, 22),
                Font = new Font("Segoe UI", 9F, FontStyle.Regular)
            };

            grpStatus.Controls.Add(lblSecureBoot);
            grpStatus.Controls.Add(lblTestMode);
            grpStatus.Controls.Add(lblDriverStatus);

            lblLog = new Label
            {
                Text = "Siap untuk instalasi.",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                ForeColor = Color.FromArgb(40, 40, 40),
                Location = new Point(20, 185),
                Size = new Size(485, 26)
            };

            btnInstall = new Button
            {
                Text = "Pasang DF-OpenVCable",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                Location = new Point(20, 225),
                Size = new Size(180, 36)
            };
            btnInstall.Click += BtnInstall_Click;

            btnUninstall = new Button
            {
                Text = "Hapus Driver",
                Location = new Point(210, 225),
                Size = new Size(120, 36)
            };
            btnUninstall.Click += BtnUninstall_Click;

            btnRestart = new Button
            {
                Text = "Restart PC Sekarang",
                Font = new Font("Segoe UI", 9F, FontStyle.Bold),
                ForeColor = Color.DarkRed,
                Location = new Point(340, 225),
                Size = new Size(165, 36),
                Visible = false
            };
            btnRestart.Click += (s, e) => RunCommand("shutdown.exe", "/r /t 0");

            this.Controls.Add(lblTitle);
            this.Controls.Add(grpStatus);
            this.Controls.Add(lblLog);
            this.Controls.Add(btnInstall);
            this.Controls.Add(btnUninstall);
            this.Controls.Add(btnRestart);
        }

        private static bool IsAdmin()
        {
            var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }

        private static string RunCommand(string file, string args)
        {
            try
            {
                var psi = new ProcessStartInfo
                {
                    FileName = file,
                    Arguments = args,
                    CreateNoWindow = true,
                    UseShellExecute = false,
                    RedirectStandardOutput = true,
                    RedirectStandardError = true
                };
                using (var p = Process.Start(psi))
                {
                    string outText = p.StandardOutput.ReadToEnd();
                    p.WaitForExit();
                    return outText;
                }
            }
            catch (Exception ex)
            {
                return "ERROR: " + ex.Message;
            }
        }

        private static string FindComponentFile(string appDir, string fileName)
        {
            string[] searchPaths = new string[]
            {
                Path.Combine(appDir, fileName),
                Path.Combine(appDir, "Driver", fileName),
                Path.Combine(appDir, "package", fileName),
                Path.Combine(appDir, "Tools", fileName),
                Path.Combine(appDir, "..", fileName),
                Path.Combine(appDir, "..", "package", fileName),
                Path.Combine(appDir, "..", "AIR", fileName),
                Path.Combine(appDir, "..", "Driver", fileName),
                Path.Combine(appDir, "..", "control_panel", fileName)
            };

            foreach (var p in searchPaths)
            {
                try
                {
                    string full = Path.GetFullPath(p);
                    if (File.Exists(full)) return full;
                }
                catch { }
            }
            return Path.Combine(appDir, fileName);
        }

        private static void CloseControlPanelProcesses()
        {
            try
            {
                foreach (var proc in Process.GetProcessesByName("control_panel"))
                {
                    try
                    {
                        proc.Kill();
                        proc.WaitForExit(2000);
                    }
                    catch { }
                }
            }
            catch { }
        }

        private static void CreateShortcut(string shortcutPath, string targetPath, string workingDir, string description)
        {
            try
            {
                string dir = Path.GetDirectoryName(shortcutPath);
                if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir))
                {
                    Directory.CreateDirectory(dir);
                }

                Type shellType = Type.GetTypeFromProgID("WScript.Shell");
                if (shellType != null)
                {
                    object shell = Activator.CreateInstance(shellType);
                    object shortcut = shellType.InvokeMember("CreateShortcut",
                        System.Reflection.BindingFlags.InvokeMethod, null, shell, new object[] { shortcutPath });
                    Type scType = shortcut.GetType();
                    scType.InvokeMember("TargetPath", System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { targetPath });
                    scType.InvokeMember("WorkingDirectory", System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { workingDir });
                    scType.InvokeMember("Description", System.Reflection.BindingFlags.SetProperty, null, shortcut, new object[] { description });
                    scType.InvokeMember("Save", System.Reflection.BindingFlags.InvokeMethod, null, shortcut, null);
                    return;
                }
            }
            catch { }

            // Fallback via PowerShell
            try
            {
                string ps = string.Format(
                    "$s=(New-Object -COM WScript.Shell).CreateShortcut('{0}');$s.TargetPath='{1}';$s.WorkingDirectory='{2}';$s.Description='{3}';$s.Save()",
                    shortcutPath.Replace("'", "''"),
                    targetPath.Replace("'", "''"),
                    workingDir.Replace("'", "''"),
                    description.Replace("'", "''"));
                RunCommand("powershell.exe", string.Format("-NoProfile -ExecutionPolicy Bypass -Command \"{0}\"", ps));
            }
            catch { }
        }

        private static void InstallAppFilesAndShortcuts(string appDir)
        {
            CloseControlPanelProcesses();

            string srcExe = FindComponentFile(appDir, "control_panel.exe");
            if (File.Exists(srcExe))
            {
                if (!Directory.Exists(AppInstallDir))
                {
                    Directory.CreateDirectory(AppInstallDir);
                }
                string destExe = Path.Combine(AppInstallDir, "control_panel.exe");
                File.Copy(srcExe, destExe, true);

                string lnkName = "DF-OpenVCable Control Panel.lnk";

                // 1. Start Menu shortcut: C:\ProgramData\Microsoft\Windows\Start Menu\Programs\DF-OpenVCable\DF-OpenVCable Control Panel.lnk
                string commonPrograms = Environment.GetFolderPath(Environment.SpecialFolder.CommonPrograms);
                if (string.IsNullOrEmpty(commonPrograms)) commonPrograms = @"C:\ProgramData\Microsoft\Windows\Start Menu\Programs";
                string startMenuDir = Path.Combine(commonPrograms, "DF-OpenVCable");
                string startMenuLnk = Path.Combine(startMenuDir, lnkName);
                CreateShortcut(startMenuLnk, destExe, AppInstallDir, "DF-OpenVCable Control Panel");

                // 2. Desktop shortcuts (Public Desktop & User Desktop)
                string commonDesktop = Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory);
                if (!string.IsNullOrEmpty(commonDesktop) && Directory.Exists(commonDesktop))
                {
                    string pubLnk = Path.Combine(commonDesktop, lnkName);
                    CreateShortcut(pubLnk, destExe, AppInstallDir, "DF-OpenVCable Control Panel");
                }

                string userDesktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                if (!string.IsNullOrEmpty(userDesktop) && Directory.Exists(userDesktop) &&
                    !string.Equals(commonDesktop, userDesktop, StringComparison.OrdinalIgnoreCase))
                {
                    string usrLnk = Path.Combine(userDesktop, lnkName);
                    CreateShortcut(usrLnk, destExe, AppInstallDir, "DF-OpenVCable Control Panel");
                }
            }
        }

        private static void RemoveAppFilesAndShortcuts()
        {
            CloseControlPanelProcesses();

            string lnkName = "DF-OpenVCable Control Panel.lnk";

            // 1. Remove Start Menu shortcuts
            try
            {
                string commonPrograms = Environment.GetFolderPath(Environment.SpecialFolder.CommonPrograms);
                if (string.IsNullOrEmpty(commonPrograms)) commonPrograms = @"C:\ProgramData\Microsoft\Windows\Start Menu\Programs";
                string commonSmDir = Path.Combine(commonPrograms, "DF-OpenVCable");
                string commonLnk = Path.Combine(commonSmDir, lnkName);
                if (File.Exists(commonLnk)) File.Delete(commonLnk);
                if (Directory.Exists(commonSmDir) && Directory.GetFileSystemEntries(commonSmDir).Length == 0)
                {
                    Directory.Delete(commonSmDir);
                }
            }
            catch { }

            try
            {
                string userPrograms = Environment.GetFolderPath(Environment.SpecialFolder.Programs);
                if (!string.IsNullOrEmpty(userPrograms))
                {
                    string userSmDir = Path.Combine(userPrograms, "DF-OpenVCable");
                    string userLnk = Path.Combine(userSmDir, lnkName);
                    if (File.Exists(userLnk)) File.Delete(userLnk);
                    if (Directory.Exists(userSmDir) && Directory.GetFileSystemEntries(userSmDir).Length == 0)
                    {
                        Directory.Delete(userSmDir);
                    }
                }
            }
            catch { }

            // 2. Remove Desktop shortcuts
            try
            {
                string commonDesktop = Environment.GetFolderPath(Environment.SpecialFolder.CommonDesktopDirectory);
                if (!string.IsNullOrEmpty(commonDesktop))
                {
                    string pubLnk = Path.Combine(commonDesktop, lnkName);
                    if (File.Exists(pubLnk)) File.Delete(pubLnk);
                }
            }
            catch { }

            try
            {
                string userDesktop = Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
                if (!string.IsNullOrEmpty(userDesktop))
                {
                    string usrLnk = Path.Combine(userDesktop, lnkName);
                    if (File.Exists(usrLnk)) File.Delete(usrLnk);
                }
            }
            catch { }

            // 3. Remove Program Files directory
            try
            {
                if (Directory.Exists(AppInstallDir))
                {
                    Directory.Delete(AppInstallDir, true);
                }
            }
            catch { }
        }

        private void RefreshStatus()
        {
            // 1. Secure Boot
            isSecureBootActive = false;
            try
            {
                using (var k = Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Control\SecureBoot\State"))
                {
                    if (k != null)
                    {
                        var val = k.GetValue("UEFISecureBootEnabled");
                        if (val != null && Convert.ToInt32(val) == 1)
                        {
                            isSecureBootActive = true;
                        }
                    }
                }
            }
            catch { }

            if (isSecureBootActive)
            {
                lblSecureBoot.Text = "Secure Boot: [AKTIF] - Wajib dimatikan di BIOS sebelum instalasi!";
                lblSecureBoot.ForeColor = Color.Red;
            }
            else
            {
                lblSecureBoot.Text = "Secure Boot: [NONAKTIF] - Aman untuk memuat driver uji coba.";
                lblSecureBoot.ForeColor = Color.Green;
            }

            // 2. Test Signing
            string bcdOut = RunCommand("bcdedit.exe", "");
            isTestModeActive = bcdOut.IndexOf("testsigning             Yes", StringComparison.OrdinalIgnoreCase) >= 0 ||
                               bcdOut.IndexOf("testsigning             yes", StringComparison.OrdinalIgnoreCase) >= 0 ||
                               bcdOut.IndexOf("testsigning = Yes", StringComparison.OrdinalIgnoreCase) >= 0;

            if (isTestModeActive)
            {
                lblTestMode.Text = "Test Signing: [AKTIF] - Windows siap menjalankan driver.";
                lblTestMode.ForeColor = Color.Green;
            }
            else
            {
                lblTestMode.Text = "Test Signing: [NONAKTIF] - Akan diaktifkan otomatis oleh installer.";
                lblTestMode.ForeColor = Color.DarkGoldenrod;
            }

            // 3. Driver Status
            isDriverInstalled = false;
            try
            {
                using (var k = Registry.LocalMachine.OpenSubKey(@"SYSTEM\CurrentControlSet\Services\mcvad"))
                {
                    if (k != null) isDriverInstalled = true;
                }
            }
            catch { }

            if (isDriverInstalled)
            {
                lblDriverStatus.Text = "Status Driver: [TERPASANG] - 5 Pasang DF-OpenVCable terdaftar.";
                lblDriverStatus.ForeColor = Color.Green;
                btnInstall.Text = "Perbarui / Pasang Ulang";
                btnUninstall.Enabled = true;
            }
            else
            {
                lblDriverStatus.Text = "Status Driver: [BELUM TERPASANG]";
                lblDriverStatus.ForeColor = Color.DarkGray;
                btnInstall.Text = "Pasang DF-OpenVCable";
                btnUninstall.Enabled = false;
            }
        }

        private void BtnInstall_Click(object sender, EventArgs e)
        {
            string appDir = AppDomain.CurrentDomain.BaseDirectory;

            if (isSecureBootActive)
            {
                var res = MessageBox.Show(
                    "PERINGATAN: Fitur Secure Boot di BIOS komputer Anda terdeteksi AKTIF!\n\n" +
                    "Windows tidak mengizinkan pemuatan driver kernel uji coba (test-signed) selama Secure Boot menyala.\n\n" +
                    "Apakah Anda ingin tetap melanjutkan? (Disarankan untuk mematikan Secure Boot di BIOS terlebih dahulu).",
                    "Secure Boot Terdeteksi",
                    MessageBoxButtons.YesNo,
                    MessageBoxIcon.Warning);

                if (res != DialogResult.Yes) return;
            }

            btnInstall.Enabled = false;
            btnUninstall.Enabled = false;
            Cursor = Cursors.WaitCursor;

            try
            {
                // 1. Import Certificate
                lblLog.Text = "[1/5] Mengimpor sertifikat pengujian (MCVAD_TestCert.cer)...";
                lblLog.Update();
                string certPath = FindComponentFile(appDir, "MCVAD_TestCert.cer");
                if (File.Exists(certPath))
                {
                    RunCommand("certutil.exe", string.Format("-addstore -f Root \"{0}\"", certPath));
                    RunCommand("certutil.exe", string.Format("-addstore -f TrustedPublisher \"{0}\"", certPath));
                }

                // 2. Enable Test Signing
                lblLog.Text = "[2/5] Mengaktifkan Windows Test Signing (bcdedit)...";
                lblLog.Update();
                bool needRestart = false;
                if (!isTestModeActive)
                {
                    RunCommand("bcdedit.exe", "/set testsigning on");
                    needRestart = true;
                }

                // 3. Create Root Device Node
                lblLog.Text = "[3/5] Mendaftarkan Root Device Node audio di Windows...";
                lblLog.Update();
                string psScript = FindComponentFile(appDir, "create_device.ps1");
                if (File.Exists(psScript))
                {
                    RunCommand("powershell.exe", string.Format("-NoProfile -ExecutionPolicy Bypass -File \"{0}\"", psScript));
                }

                // 4. Install Driver Package via PnPUtil
                lblLog.Text = "[4/5] Memasang paket driver INF via PnPUtil...";
                lblLog.Update();
                string infPath = FindComponentFile(appDir, "mcvad.inf");
                if (File.Exists(infPath))
                {
                    RunCommand("pnputil.exe", string.Format("/add-driver \"{0}\" /install", infPath));
                }

                // 5. Install Control Panel & Shortcuts
                lblLog.Text = "[5/5] Menginstalasi Control Panel & membuat shortcut...";
                lblLog.Update();
                InstallAppFilesAndShortcuts(appDir);

                RefreshStatus();

                if (needRestart)
                {
                    lblLog.Text = "Instalasi selesai! Restart Windows diperlukan agar Test Signing aktif.";
                    lblLog.ForeColor = Color.DarkRed;
                    btnRestart.Visible = true;
                    var r = MessageBox.Show(
                        "Instalasi driver & Control Panel DF-OpenVCable telah selesai!\n\n" +
                        "Windows Test Signing telah diaktifkan, namun komputer HARUS di-restart agar driver dapat dimuat kernel.\n\n" +
                        "Apakah Anda ingin me-restart komputer sekarang?",
                        "Restart Diperlukan",
                        MessageBoxButtons.YesNo,
                        MessageBoxIcon.Question);

                    if (r == DialogResult.Yes)
                    {
                        RunCommand("shutdown.exe", "/r /t 0");
                    }
                }
                else
                {
                    lblLog.Text = "SUKSES: DF-OpenVCable & Control Panel berhasil dipasang!";
                    lblLog.ForeColor = Color.Green;
                    MessageBox.Show(
                        "Selamat! Driver DF-OpenVCable dan Control Panel berhasil dipasang.\n\n" +
                        "Ke-5 kabel virtual (A s.d. E) kini aktif dan shortcut Control Panel tersedia di Desktop dan Start Menu.",
                        "Instalasi Sukses",
                        MessageBoxButtons.OK,
                        MessageBoxIcon.Information);
                }
            }
            finally
            {
                btnInstall.Enabled = true;
                Cursor = Cursors.Default;
            }
        }

        private void BtnUninstall_Click(object sender, EventArgs e)
        {
            var res = MessageBox.Show(
                "Apakah Anda yakin ingin mencopot (uninstall) DF-OpenVCable beserta Control Panel dari komputer ini?",
                "Konfirmasi Uninstall",
                MessageBoxButtons.YesNo,
                MessageBoxIcon.Question);

            if (res != DialogResult.Yes) return;

            btnInstall.Enabled = false;
            btnUninstall.Enabled = false;
            Cursor = Cursors.WaitCursor;

            try
            {
                lblLog.Text = "Menghapus driver DF-OpenVCable dan Control Panel dari sistem...";
                lblLog.Update();

                // 1. Remove Control Panel & shortcuts & installation directory
                RemoveAppFilesAndShortcuts();

                // 2. Remove Root Device Node
                string appDir = AppDomain.CurrentDomain.BaseDirectory;
                string psScript = FindComponentFile(appDir, "create_device.ps1");
                if (File.Exists(psScript))
                {
                    RunCommand("powershell.exe", string.Format("-NoProfile -ExecutionPolicy Bypass -File \"{0}\" -Remove", psScript));
                }

                // 3. Uninstall and force delete driver from driver store
                string infPath = FindComponentFile(appDir, "mcvad.inf");
                RunCommand("pnputil.exe", string.Format("/delete-driver \"{0}\" /uninstall /force", infPath));

                RefreshStatus();
                lblLog.Text = "DF-OpenVCable dan Control Panel telah berhasil dicopot dari sistem.";
                lblLog.ForeColor = Color.DarkGray;

                MessageBox.Show("Driver DF-OpenVCable dan Control Panel telah berhasil dihapus dari sistem.", "Uninstall Selesai", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            finally
            {
                btnInstall.Enabled = true;
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
                    MessageBox.Show("Hak akses Administrator diperlukan untuk menjalankan Setup.", "Akses Ditolak", MessageBoxButtons.OK, MessageBoxIcon.Error);
                }
                return;
            }

            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new InstallerForm());
        }
    }
}