using System;
using System.Diagnostics;
using System.Collections.Generic;
using System.IO;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;
using System.Windows.Forms;
using LGEngine;

// An optional local adapter. References the user's separately built LGEngine;
// upstream sources and binaries are not bundled with LiquidGlassControl.
namespace LiquidGlassOptical
{
    internal static class AdapterLog
    {
        static readonly object gate = new object();
        public static void Write(string stage, string detail)
        {
            try
            {
                string file = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "preview.log");
                lock (gate)
                {
                    if (File.Exists(file) && new FileInfo(file).Length > 65536) File.WriteAllText(file, "");
                    File.AppendAllText(file, DateTime.UtcNow.ToString("o") + " " + stage + ": " + detail.Replace('\r', ' ').Replace('\n', ' ') + Environment.NewLine);
                }
            }
            catch (IOException) { }
            catch (UnauthorizedAccessException) { }
        }
    }
    internal sealed class Settings
    {
        public int Opacity = 35, Blur = 12, Radius = 12, Tint, Language;
        public bool SavePower = true, ReducedMotion;
        // Automatic tint follows the pixels behind the glass, not the Windows theme.
        public bool AdaptToBackground { get { return Tint == 0; } }
        public int Appearance { get { return Tint == 0 ? -1 : (Tint == 1 ? 1 : 0); } }
        public static Settings Read(int[] w)
        {
            if (w == null || w.Length != 8 || w[0] != 2 ||
                w[1] < 5 || w[1] > 90 || w[2] < 0 || w[2] > 40 ||
                w[3] < 0 || w[3] > 28 || w[4] < 0 || w[4] > 2 ||
                w[5] < 0 || w[5] > 1 || w[6] < 0 || w[6] > 1 || w[7] < 0 || w[7] > 4) return null;
            return new Settings { Opacity = w[1], Blur = w[2], Radius = w[3], Tint = w[4],
                SavePower = w[5] != 0, ReducedMotion = w[6] != 0, Language = w[7] };
        }
        public GlassLook Look(int refraction, int rim, int elevation)
        {
            return new GlassLook(Opacity, (Blur * 100 + 20) / 40, refraction, rim, 28, Radius, 100, elevation);
        }
        public uint Digest()
        {
            uint hash = 2166136261;
            int[] words = { 2, Opacity, Blur, Radius, Tint, SavePower ? 1 : 0, ReducedMotion ? 1 : 0, Language };
            unchecked
            {
                foreach (int word in words)
                    for (int shift = 0; shift < 32; shift += 8)
                    { hash ^= (byte)((uint)word >> shift); hash *= 16777619; }
            }
            return hash;
        }
    }

    internal sealed class OpticalPreferences
    {
        public int Refraction = 80, Rim = 60, Elevation = 50, Profile;
        public string Serialize()
        {
            return string.Format(System.Globalization.CultureInfo.InvariantCulture,
                "Refraction={0}\nRim={1}\nElevation={2}\nProfile={3}\n", Refraction, Rim, Elevation, Profile);
        }
        public static OpticalPreferences Parse(string text)
        {
            if (text == null || text.Length > 1024) return null;
            Dictionary<string, int> values = new Dictionary<string, int>(StringComparer.Ordinal);
            foreach (string line in text.Split(new char[] { '\n' }, StringSplitOptions.RemoveEmptyEntries))
            {
                string[] pair = line.Trim().Split('='); int number;
                if (pair.Length != 2 || values.ContainsKey(pair[0]) ||
                    !int.TryParse(pair[1], System.Globalization.NumberStyles.Integer, System.Globalization.CultureInfo.InvariantCulture, out number)) return null;
                values.Add(pair[0], number);
            }
            int refraction, rim, elevation, profile;
            if (values.Count != 4 || !values.TryGetValue("Refraction", out refraction) ||
                !values.TryGetValue("Rim", out rim) || !values.TryGetValue("Elevation", out elevation) || !values.TryGetValue("Profile", out profile) ||
                refraction < 0 || refraction > 100 || rim < 0 || rim > 100 || elevation < 0 || elevation > 100 || profile < 0 || profile > 4) return null;
            return new OpticalPreferences { Refraction = refraction, Rim = rim, Elevation = elevation, Profile = profile };
        }
        public static string FilePath { get { return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "optical-preview.ini"); } }
        public static OpticalPreferences Load()
        {
            try
            {
                if (File.Exists(FilePath))
                {
                    if (new FileInfo(FilePath).Length > 1024) throw new IOException("Optical settings exceed the size limit.");
                    OpticalPreferences parsed = Parse(File.ReadAllText(FilePath));
                    if (parsed != null) return parsed;
                    AdapterLog.Write("preferences", "Invalid optical settings; using defaults.");
                }
            }
            catch (IOException ex) { AdapterLog.Write("preferences", ex.Message); }
            catch (UnauthorizedAccessException ex) { AdapterLog.Write("preferences", ex.Message); }
            return new OpticalPreferences();
        }
        public bool Save() { return SaveTo(FilePath); }
        internal bool SaveTo(string path)
        {
            string temp = path + "." + Guid.NewGuid().ToString("N") + ".tmp";
            try
            {
                File.WriteAllText(temp, Serialize());
                if (File.Exists(path)) File.Replace(temp, path, null);
                else File.Move(temp, path);
                return true;
            }
            catch (IOException ex) { AdapterLog.Write("preferences", ex.Message); return false; }
            catch (UnauthorizedAccessException ex) { AdapterLog.Write("preferences", ex.Message); return false; }
            finally { try { if (File.Exists(temp)) File.Delete(temp); } catch (IOException) { } catch (UnauthorizedAccessException) { } }
        }
    }

    internal sealed class Backdrop : Panel
    {
        public Backdrop() { DoubleBuffered = true; }
        protected override void OnPaint(PaintEventArgs e)
        {
            if (Width < 1 || Height < 1) return;
            using (LinearGradientBrush b = new LinearGradientBrush(ClientRectangle,
                Color.FromArgb(83, 59, 166), Color.FromArgb(46, 199, 200), 25f)) e.Graphics.FillRectangle(b, ClientRectangle);
            using (Pen p = new Pen(Color.FromArgb(90, Color.White)))
                for (int x = -Height; x < Width; x += 30) e.Graphics.DrawLine(p, x, Height, x + Height, 0);
            using (Brush b = new SolidBrush(Color.FromArgb(180, 255, 158, 104))) e.Graphics.FillEllipse(b, Width / 2, -40, 240, 240);
            base.OnPaint(e);
        }
    }

    internal sealed class Preview : Form
    {
        const int CopyData = 0x004A, Event = 0x8010, ActivatePreview = 0x8011;
        const long WireTag = 0x4c475031;
        [StructLayout(LayoutKind.Sequential)]
        struct CopyPacket { public IntPtr Tag; public int Length; public IntPtr Data; }
        [DllImport("user32.dll")] static extern bool PostMessage(IntPtr hwnd, int msg, IntPtr w, IntPtr l);
        [DllImport("user32.dll")] static extern uint GetDpiForWindow(IntPtr hwnd);
        [DllImport("user32.dll")] static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int w, int h, uint flags);

        readonly IntPtr owner;
        readonly Process parent;
        readonly Backdrop backdrop = new Backdrop();
        readonly Label status = new Label();
        readonly TrackBar refraction = Slider(80), rim = Slider(60), elevation = Slider(50);
        readonly Label[] labels = { new Label(), new Label(), new Label() };
        readonly ComboBox profile = new ComboBox();
        readonly Button replay = new Button(), close = new Button();
        readonly Dictionary<Control, Rectangle> logicalBounds = new Dictionary<Control, Rectangle>();
        float uiScale = 1f;
        bool dpiPending, changingProfiles;
        int profileLanguage = -1;
        static readonly string[] Profiles = { "Today", "Tighter rim", "Thin lens", "Pillow", "Flat pane" };
        Settings settings = new Settings();
        GlassService service;
        GlassSurface surface;
        bool closing, initialising = true;
        readonly System.Windows.Forms.Timer debounce = new System.Windows.Forms.Timer { Interval = 120 };
        string failed;
        string savedOptics;
        static TrackBar Slider(int value) { return new TrackBar { Minimum = 0, Maximum = 100, Value = value, TickStyle = TickStyle.None }; }
        string TextFor(string en, string zh, string hant, string pl, string vi)
        { return new string[] { en, zh, hant, pl, vi }[settings.Language]; }

        public Preview(IntPtr ownerWindow, Process ownerProcess)
        {
            owner = ownerWindow; parent = ownerProcess;
            OpticalPreferences preferences = OpticalPreferences.Load();
            refraction.Value = preferences.Refraction; rim.Value = preferences.Rim; elevation.Value = preferences.Elevation;
            savedOptics = preferences.Serialize();
            Text = "LGEngine"; AutoScaleMode = AutoScaleMode.None; ClientSize = new Size(700, 530); MinimumSize = Size; MaximumSize = Size;
            StartPosition = FormStartPosition.CenterScreen; Font = new Font("Segoe UI", 10);
            BackColor = Color.FromArgb(242, 245, 250);
            backdrop.SetBounds(16, 16, 668, 235); Controls.Add(backdrop);
            TrackBar[] sliders = { refraction, rim, elevation };
            for (int i = 0; i < sliders.Length; i++)
            {
                labels[i].SetBounds(20, 267 + i * 44, 210, 30); Controls.Add(labels[i]);
                sliders[i].SetBounds(240, 260 + i * 44, 430, 36); Controls.Add(sliders[i]);
                sliders[i].ValueChanged += delegate { QueueLook(); };
            }
            profile.SetBounds(20, 402, 310, 30); profile.DropDownStyle = ComboBoxStyle.DropDownList;
            profile.Items.AddRange(Profiles);
            profile.SelectedIndex = preferences.Profile; profile.SelectedIndexChanged += delegate { if (!changingProfiles) QueueLook(); }; Controls.Add(profile);
            replay.SetBounds(345, 400, 165, 34); replay.Click += delegate { Recreate(); }; Controls.Add(replay);
            close.SetBounds(525, 400, 145, 34); close.Click += delegate { Close(); }; Controls.Add(close);
            status.SetBounds(20, 448, 650, 65); Controls.Add(status);
            foreach (Control control in Controls) logicalBounds.Add(control, control.Bounds);
            debounce.Tick += delegate { debounce.Stop(); ApplyLook(); SaveOptics(); };
            Move += delegate { Place(); }; Resize += delegate { Place(); };
            Shown += delegate
            {
                initialising = false;
                LayoutForDpi();
                if (parent != null)
                {
                    parent.EnableRaisingEvents = true;
                    parent.Exited += ParentExited;
                    if (parent.HasExited) { Close(); return; }
                    PostMessage(owner, Event, new IntPtr(1), Handle);
                }
                Recreate();
            };
            Localize();
        }

        int Px(int value) { return (int)Math.Round(value * uiScale); }
        Size CardSize { get { return new Size(Px(540), Px(145)); } }
        void LayoutForDpi()
        {
            uint dpi = 96;
            try { dpi = GetDpiForWindow(Handle); } catch (EntryPointNotFoundException) { }
            uiScale = Math.Max(1f, Math.Min(4f, dpi / 96f));
            MinimumSize = Size.Empty; MaximumSize = Size.Empty;
            ClientSize = new Size(Px(700), Px(530));
            Font previous = Font;
            Font = new Font("Segoe UI", 13.333333f * uiScale, GraphicsUnit.Pixel);
            previous.Dispose();
            foreach (KeyValuePair<Control, Rectangle> entry in logicalBounds)
            {
                Rectangle r = entry.Value;
                entry.Key.SetBounds(Px(r.X), Px(r.Y), Px(r.Width), Px(r.Height));
            }
            MinimumSize = Size; MaximumSize = Size;
            AdapterLog.Write("dpi", dpi.ToString());
        }

        void ParentExited(object sender, EventArgs e)
        {
            if (closing) return;
            try { BeginInvoke((Action)delegate { Close(); }); } catch (InvalidOperationException) { }
        }

        void Localize()
        {
            if (profileLanguage != settings.Language)
            {
                int selected = Math.Max(0, profile.SelectedIndex);
                changingProfiles = true;
                try
                {
                    profile.Items.Clear();
                    profile.Items.AddRange(new string[] {
                        TextFor("Original", "原始截面", "原始截面", "Oryginalny", "Mặc định"),
                        TextFor("Tighter rim", "紧致边缘", "緊緻邊緣", "Wąska krawędź", "Viền hẹp"),
                        TextFor("Thin lens", "薄透镜", "薄透鏡", "Cienka soczewka", "Thấu kính mỏng"),
                        TextFor("Pillow", "枕形", "枕形", "Poduszka", "Dạng gối"),
                        TextFor("Flat pane", "平面玻璃", "平面玻璃", "Płaska tafla", "Tấm kính phẳng") });
                    profile.SelectedIndex = selected;
                    profileLanguage = settings.Language;
                }
                finally { changingProfiles = false; }
            }
            Text = TextFor("LGEngine optical preview", "LGEngine 光学玻璃预览", "LGEngine 光學玻璃預覽", "Podgląd szkła LGEngine", "Xem trước kính LGEngine");
            string[] names = {
                TextFor("Refraction", "折射", "折射", "Załamanie", "Khúc xạ"),
                TextFor("Rim light", "边缘光", "邊緣光", "Światło krawędzi", "Ánh viền"),
                TextFor("Elevation", "悬浮高度", "懸浮高度", "Wysokość", "Độ nổi") };
            int[] values = { refraction.Value, rim.Value, elevation.Value };
            for (int i = 0; i < 3; i++) labels[i].Text = names[i] + ": " + values[i];
            replay.Text = TextFor("Replay", "重播动画", "重播動畫", "Odtwórz", "Phát lại");
            close.Text = TextFor("Close", "关闭", "關閉", "Zamknij", "Đóng");
            status.Text = failed ?? TextFor("Preview only. Automatic tint adapts to the background; Light/Dark locks the appearance. Blur and corners follow the controller.",
                "仅影响预览。自动色调根据背景调整明暗；浅色／深色锁定外观。模糊和圆角跟随主程序。",
                "僅影響預覽。自動色調根據背景調整明暗；淺色／深色鎖定外觀。模糊和圓角跟隨主程式。",
                "Tylko podgląd. Tryb automatyczny reaguje na tło; Jasny/Ciemny blokuje wygląd. Rozmycie i narożniki ze sterownika.",
                "Chỉ xem trước. Màu tự động theo nền; Sáng/Tối cố định giao diện. Độ mờ và góc theo ứng dụng chính.");
        }

        void QueueLook() { Localize(); debounce.Stop(); debounce.Start(); }
        void SaveOptics()
        {
            OpticalPreferences preferences = new OpticalPreferences { Refraction = refraction.Value, Rim = rim.Value, Elevation = elevation.Value, Profile = Math.Max(0, profile.SelectedIndex) };
            string serialized = preferences.Serialize();
            if (serialized != savedOptics && preferences.Save()) savedOptics = serialized;
        }
        bool Quiet() { return settings.ReducedMotion || SystemInformation.HighContrast; }
        void ApplyLook()
        {
            if (surface == null || closing) return;
            try
            {
                surface.SetLook(settings.Look(refraction.Value, rim.Value, elevation.Value), Profiles[Math.Max(0, profile.SelectedIndex)]);
                surface.SetAppearance(settings.Appearance);
                surface.SetUpdates(SystemInformation.HighContrast ? 0 : 15, settings.AdaptToBackground, settings.SavePower);
                surface.SetMotion(Quiet() ? 0 : 4, 380, 230);
                surface.SetInteraction(!Quiet());
                DrawContents(); Place();
            }
            catch (Exception ex) { Fail(ex.Message); }
        }
        void Recreate()
        {
            if (initialising || closing) return;
            debounce.Stop(); failed = null;
            if (surface != null) { surface.Dispose(); surface = null; }
            try
            {
                if (service == null) service = GlassService.Connect(new GlassConnectOptions { Mode = GlassMode.UiThread });
                GlassSpec spec = new GlassSpec { Label = "LiquidGlassControl.LGEngine.Preview", Size = CardSize,
                    Place = CardPlace(), Scale = uiScale, Adapt = settings.AdaptToBackground, InShots = false, SavePower = settings.SavePower,
                    AnimStyle = Quiet() ? 0 : 4, Hover = !Quiet(), Look = settings.Look(refraction.Value, rim.Value, elevation.Value) };
                surface = service.Create(spec);
                // UiThread creates its GlassLayer synchronously. Make it an
                // owned, ordinary window so it follows preview z-order rather
                // than covering unrelated applications as a topmost overlay.
                foreach (Form form in Application.OpenForms)
                    if (form is GlassLayer && form.Text == spec.Label)
                    {
                        form.Owner = this;
                        SetWindowPos(form.Handle, new IntPtr(-2), 0, 0, 0, 0, 0x0013);
                    }
                surface.Failed += delegate(object sender, GlassFailedEventArgs e) { Fail(e.Text); };
                surface.Shown += delegate { AdapterLog.Write("surface", "shown"); };
                ApplyLook();
                if (!SystemInformation.HighContrast) surface.Show(); Localize();
            }
            catch (Exception ex) { Fail(ex.Message); }
        }
        Point CardPlace() { return backdrop.PointToScreen(new Point((backdrop.Width - CardSize.Width) / 2, Px(45))); }
        void Place()
        {
            if (surface == null || closing) return;
            if (WindowState == FormWindowState.Minimized || SystemInformation.HighContrast) surface.Hide(false);
            else { surface.SetPlace(CardPlace(), false); surface.Show(); }
        }
        void DrawContents()
        {
            if (surface == null) return;
            Size size = CardSize;
            int[] pixels = new int[size.Width * size.Height];
            using (Bitmap image = new Bitmap(size.Width, size.Height, PixelFormat.Format32bppPArgb))
            {
                using (Graphics g = Graphics.FromImage(image))
                using (Font heading = new Font("Segoe UI", 22, FontStyle.Bold))
                using (Font detail = new Font("Segoe UI", 11))
                // LGEngine flips even-alpha grey ink for light glass itself.
                using (Brush ink = new SolidBrush(Color.FromArgb(254, 254, 254, 254)))
                {
                    g.ScaleTransform(uiScale, uiScale);
                    g.DrawString("Liquid Glass", heading, ink, 30, 24);
                    g.DrawString(TextFor("Refraction · dispersion · rim light", "折射 · 色散 · 边缘光", "折射 · 色散 · 邊緣光", "Załamanie · dyspersja · światło", "Khúc xạ · tán sắc · ánh viền"), detail, ink, 33, 80);
                }
                BitmapData data = image.LockBits(new Rectangle(Point.Empty, size), ImageLockMode.ReadOnly, PixelFormat.Format32bppPArgb);
                try { for (int y = 0; y < size.Height; y++) Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), pixels, y * size.Width, size.Width); }
                finally { image.UnlockBits(data); }
            }
            surface.SetContents(pixels, size.Width, size.Height, Rectangle.Empty);
        }
        void Fail(string text)
        {
            failed = "LGEngine: " + text; Localize();
            AdapterLog.Write("failure", text);
            if (owner != IntPtr.Zero) PostMessage(owner, Event, new IntPtr(2), Handle);
        }
        protected override void WndProc(ref Message m)
        {
            if (m.Msg == CopyData)
            {
                CopyPacket packet = (CopyPacket)Marshal.PtrToStructure(m.LParam, typeof(CopyPacket));
                if (owner != IntPtr.Zero && m.WParam == owner && packet.Tag.ToInt64() == WireTag && packet.Length == 32 && packet.Data != IntPtr.Zero)
                {
                    int[] words = new int[8]; Marshal.Copy(packet.Data, words, 0, 8);
                    Settings next = Settings.Read(words);
                    if (next != null) { settings = next; Localize(); QueueLook(); m.Result = new IntPtr((long)settings.Digest()); return; }
                }
                m.Result = IntPtr.Zero; return;
            }
            if (m.Msg == ActivatePreview) { if (WindowState == FormWindowState.Minimized) WindowState = FormWindowState.Normal; Activate(); return; }
            base.WndProc(ref m);
            if (m.Msg == 0x02E0 && !closing && !dpiPending)
            {
                dpiPending = true;
                BeginInvoke((Action)delegate {
                    dpiPending = false; if (closing) return;
                    LayoutForDpi(); Recreate();
                });
            }
        }
        protected override void OnFormClosing(FormClosingEventArgs e)
        {
            if (!closing)
            {
                closing = true; debounce.Dispose(); SaveOptics();
                AdapterLog.Write("lifecycle", "closing");
                if (parent != null) { parent.Exited -= ParentExited; parent.Dispose(); }
                // Dispose owned layers before WinForms propagates FormClosing:
                // upstream layers intentionally cancel ordinary close messages.
                if (surface != null) surface.Dispose(); if (service != null) service.Dispose();
            }
            base.OnFormClosing(e);
        }
    }

    internal static class Program
    {
        [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr value);
        [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
        [STAThread] static int Main(string[] args)
        {
            if (args.Length == 1 && args[0] == "--self-test") return SelfTest();
            Process parent = null; IntPtr owner = IntPtr.Zero;
            if (args.Length != 1 || args[0] != "--standalone")
            {
                long hwnd; int pid;
                if (args.Length != 4 || args[0] != "--owner" || args[2] != "--pid" ||
                    !long.TryParse(args[1], out hwnd) || !int.TryParse(args[3], out pid)) return 10;
                owner = new IntPtr(hwnd); uint actual;
                GetWindowThreadProcessId(owner, out actual); if (actual != (uint)pid) return 11;
                try { parent = Process.GetProcessById(pid); if (parent.HasExited) { parent.Dispose(); return 12; } }
                catch (ArgumentException) { return 12; }
            }
            try { SetProcessDpiAwarenessContext(new IntPtr(-4)); } catch (EntryPointNotFoundException) { }
            Application.EnableVisualStyles(); Application.SetCompatibleTextRenderingDefault(false);
            bool uiFault = false;
            Application.ThreadException += delegate(object sender, System.Threading.ThreadExceptionEventArgs e) { uiFault = true; AdapterLog.Write("ui exception", e.Exception.ToString()); Application.Exit(); };
            AdapterLog.Write("startup", typeof(GlassService).Assembly.FullName);
            try { Application.Run(new Preview(owner, parent)); return uiFault ? 1 : 0; }
            catch (Exception ex) { AdapterLog.Write("startup exception", ex.ToString()); if (parent != null) parent.Dispose(); return 1; }
        }
        static int SelfTest()
        {
            int bad = 0;
            int[] valid = { 2, 35, 12, 12, 0, 1, 0, 0 };
            if (Settings.Read(valid) == null) bad++;
            int[] low = { 0, 4, -1, -1, -1, -1, -1, -1 };
            int[] high = { 3, 91, 41, 29, 3, 2, 2, 5 };
            for (int i = 0; i < 8; i++)
            {
                int[] input = (int[])valid.Clone(); input[i] = low[i]; if (Settings.Read(input) != null) bad++;
                input[i] = high[i]; if (Settings.Read(input) != null) bad++;
            }
            if (Settings.Read(null) != null || Settings.Read(new int[7]) != null) bad++;
            Settings s = Settings.Read(valid); GlassLook look = s.Look(80, 60, 50);
            if (look.Blur != 30 || look.Dark != 35 || look.Round != 12 || look.Refraction != 80 || look.Rim != 60 || look.Elevation != 50) bad++;
            OpticalPreferences preferences = new OpticalPreferences { Refraction = 0, Rim = 100, Elevation = 70, Profile = 4 };
            OpticalPreferences roundTrip = OpticalPreferences.Parse(preferences.Serialize());
            if (roundTrip == null || roundTrip.Serialize() != preferences.Serialize()) bad++;
            if (OpticalPreferences.Parse(preferences.Serialize().Replace("Profile=4", "Profile=5")) != null) bad++;
            if (OpticalPreferences.Parse(preferences.Serialize().Replace("Rim=100", "Rim=-1")) != null) bad++;
            if (OpticalPreferences.Parse(preferences.Serialize() + "Rim=10\n") != null) bad++;
            if (OpticalPreferences.Parse("Refraction=80\n") != null || OpticalPreferences.Parse(new string('x', 1025)) != null) bad++;
            string fixture = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "optics-test-" + Guid.NewGuid().ToString("N") + ".ini");
            try
            {
                if (!preferences.SaveTo(fixture) || File.ReadAllText(fixture) != preferences.Serialize()) bad++;
                preferences.Refraction = 93;
                if (!preferences.SaveTo(fixture) || OpticalPreferences.Parse(File.ReadAllText(fixture)).Refraction != 93) bad++;
            }
            catch (Exception ex) { AdapterLog.Write("self-test", ex.Message); bad++; }
            finally { if (File.Exists(fixture)) File.Delete(fixture); }
            Console.WriteLine("adapter validation: " + (bad == 0 ? "PASS" : "FAIL " + bad));
            return bad == 0 ? 0 : 1;
        }
    }
}
