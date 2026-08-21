# Nebula Tide — trailer recorder.
# Drives the real app window with a scripted, eased cursor; captures timestamped
# PNG frames (~15 fps) with a drawn cursor + click ripples; writes stamps.txt.
# Run:  powershell -ExecutionPolicy Bypass -File record.ps1
$ErrorActionPreference = 'Stop'

$frames = "$PSScriptRoot\frames"
if (Test-Path $frames) { Remove-Item $frames -Recurse -Force }

# fresh app state so the walkthrough is deterministic (starts stopped/"drifting")
Stop-Process -Name "Nebula Tide" -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1
Remove-Item "$env:APPDATA\Nebula Tide" -Recurse -Force -ErrorAction SilentlyContinue
Start-Process "C:\Drone Pad\build\NebulaTide_artefacts\Release\Standalone\Nebula Tide.exe"
Start-Sleep -Seconds 8
$p = Get-Process "Nebula Tide"

Add-Type -ReferencedAssemblies System.Drawing @"
using System; using System.Collections.Generic; using System.Drawing;
using System.Drawing.Drawing2D; using System.Drawing.Imaging; using System.IO;
using System.Runtime.InteropServices; using System.Threading;

public class Trailer
{
    [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern void mouse_event(uint f, uint x, uint y, uint d, int e);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);

    static double cx, cy;                 // cursor, window-relative
    static int wx, wy, ww, wh;            // window rect
    static List<double> stamps = new List<double>();
    static List<double[]> clicks = new List<double[]>();   // t, x, y
    static int frame = 0; static string dir;
    static System.Diagnostics.Stopwatch sw = new System.Diagnostics.Stopwatch();

    static void Cap()
    {
        double now = sw.Elapsed.TotalMilliseconds;
        using (var bmp = new Bitmap(ww, wh))
        {
            using (var g = Graphics.FromImage(bmp))
            {
                g.CopyFromScreen(wx, wy, 0, 0, new Size(ww, wh));
                g.SmoothingMode = SmoothingMode.AntiAlias;

                // click ripples: opacity .9->0, scale .4->1.35 over 380ms
                foreach (var c in clicks)
                {
                    double dt = now - c[0]; if (dt < 0 || dt > 380) continue;
                    float p = (float)(dt / 380.0);
                    float r = 13f + 33f * p; int a = (int)(210 * (1 - p));
                    using (var pen = new Pen(Color.FromArgb(a, 255, 255, 255), 2.6f))
                        g.DrawEllipse(pen, (float)c[1] - r, (float)c[2] - r, r * 2, r * 2);
                }

                // white arrow cursor with dark outline
                PointF[] pts = {
                    new PointF(0,0), new PointF(0,24), new PointF(5.5f,19f),
                    new PointF(9.5f,27.5f), new PointF(13.5f,25.5f),
                    new PointF(9.5f,17.5f), new PointF(16.5f,17.5f) };
                for (int i = 0; i < pts.Length; i++) { pts[i].X += (float)cx; pts[i].Y += (float)cy; }
                using (var b2 = new SolidBrush(Color.White))
                using (var pen2 = new Pen(Color.FromArgb(235, 8, 18, 28), 1.8f))
                { g.FillPolygon(b2, pts); g.DrawPolygon(pen2, pts); }
            }
            bmp.Save(Path.Combine(dir, "f" + frame.ToString("D5") + ".png"), ImageFormat.Png);
        }
        stamps.Add(now); frame++;
    }

    static void Tick(double until)
    {
        while (sw.Elapsed.TotalMilliseconds < until)
        {
            Cap();
            double next = stamps[stamps.Count - 1] + 66;
            double wait = next - sw.Elapsed.TotalMilliseconds;
            if (wait > 0) Thread.Sleep((int)wait);
        }
    }

    static void Move(double tx, double ty, double dur)   // cubic ease-out tween
    {
        double sx = cx, sy = cy; double t0 = sw.Elapsed.TotalMilliseconds;
        while (true)
        {
            double t = (sw.Elapsed.TotalMilliseconds - t0) / dur; if (t > 1) t = 1;
            double e = 1 - Math.Pow(1 - t, 3);
            cx = sx + (tx - sx) * e; cy = sy + (ty - sy) * e;
            SetCursorPos(wx + (int)cx, wy + (int)cy);
            Cap();
            if (t >= 1) break;
            Thread.Sleep(33);
        }
    }

    static void Click() { clicks.Add(new double[]{ sw.Elapsed.TotalMilliseconds, cx, cy }); mouse_event(2,0,0,0,0); mouse_event(4,0,0,0,0); }
    static void Down()  { mouse_event(2,0,0,0,0); }
    static void Up()    { mouse_event(4,0,0,0,0); }
    static void Pause(double ms) { Tick(sw.Elapsed.TotalMilliseconds + ms); }

    public static void Run(IntPtr hwnd, string outDir)
    {
        dir = outDir; Directory.CreateDirectory(dir);
        MoveWindow(hwnd, 100, 60, 1102, 808, true);      // fixed size = stable coords
        SetForegroundWindow(hwnd); Thread.Sleep(900);
        RECT r; GetWindowRect(hwnd, out r); wx = r.L; wy = r.T; ww = r.R - r.L; wh = r.B - r.T;
        cx = 760; cy = 460; SetCursorPos(wx + (int)cx, wy + (int)cy);
        sw.Restart();

        // ── the tour (~46 s) ──
        Pause(1800);                                        // establish: drifting universe
        Move(551, 309, 700); Pause(300); Click(); Pause(3800);   // pad blooms to life
        Move(328, 575, 800); Pause(200); Click(); Pause(2800);   // planet Eb — key crossfade
        Move(621, 578, 700); Pause(200); Click(); Pause(2500);   // planet G
        Move(1018, 180, 900); Pause(250); Click(); Pause(3000);  // texture star
        Move(82, 180, 950); Pause(250); Click(); Pause(2500);    // fx star
        Move(276, 684, 800); Click(); Pause(1600);               // ROOM
        Move(399, 684, 420); Click(); Pause(1600);               // PLATE
        Move(522, 684, 420); Click(); Pause(1800);               // HALL
        Move(116, 715, 700); Pause(200);                         // volume knob…
        Down(); Move(116, 652, 650); Pause(250); Move(116, 748, 700); Up(); Pause(800);  // …drag up/down (live readout)
        Move(700, 714, 650); Down(); Move(822, 714, 550); Up(); Pause(900);              // crossfade slider drag
        Move(551, 309, 750); Pause(250); Click(); Pause(3600);   // stop — universe exhales
        // ──────────────────────

        var lines = new List<string>();
        foreach (var s in stamps) lines.Add(s.ToString(System.Globalization.CultureInfo.InvariantCulture));
        File.WriteAllLines(Path.Combine(dir, "stamps.txt"), lines.ToArray());
        Console.WriteLine("frames=" + frame + " duration_ms=" + (int)stamps[stamps.Count - 1]);
    }
}
"@

[Trailer]::Run($p.MainWindowHandle, $frames)
