using System;
using System.Drawing;
using LGEngine;

// Compiled together with upstream sources so these checks need no shell hooks,
// desktop capture, host processes or test windows.
static class Regression
{
    static int checks, failures;
    static void Check(bool ok, string label)
    {
        checks++;
        Console.WriteLine((ok ? "PASS " : "FAIL ") + label);
        if (!ok) failures++;
    }

    sealed class HighJitter : Random
    {
        public override double NextDouble() { return 0.999999; }
    }

    static int Main()
    {
        GlassButton original = new GlassButton {
            Mask = new byte[] { 10, 20, 30, 40 },
            MaskBox = new Rectangle(0, 0, 2, 2)
        };
        GlassButton copy = original.Copy();
        Check(!object.ReferenceEquals(original.Mask, copy.Mask), "button mask has independent storage");
        original.Mask[0] = 255;
        Check(copy.Mask[0] == 10, "caller mutation does not alter published mask");
        copy.Mask[1] = 255;
        Check(original.Mask[1] == 20, "copied mask does not alter caller storage");
        Check(new GlassButton().Copy().Mask == null, "null mask preserved");

        string[] partial = { @"C:relative.exe", @"\relative.exe", "relative.exe" };
        foreach (string path in partial)
        {
            // Exercise Connect without starting transport threads or a host:
            // invalid paths must fail before dispatching by mode.
            bool rejected = false;
            using (GlassServiceGuard guard = new GlassServiceGuard())
            {
                try { guard.Service = GlassService.Connect(new GlassConnectOptions {
                    Mode = GlassMode.UiThread, HostPath = path }); }
                catch (ArgumentException) { rejected = true; }
            }
            Check(rejected, "reject partial host path " + path);
        }
        foreach (string path in new string[] { @"C:\apps\LGEngineHost.exe", @"\\server\share\LGEngineHost.exe", null })
        {
            using (GlassService service = GlassService.Connect(new GlassConnectOptions {
                Mode = GlassMode.UiThread, HostPath = path }))
                Check(service.ModeUsed == GlassMode.UiThread, "accept full/default host path " + path);
        }

        Random rng = new HighJitter();
        Check(PipeTransport.StartDelayMs(2, rng) == 0, "no early start delay");
        Check(PipeTransport.StartDelayMs(3, rng) >= 4000, "initial delay retains jitter");
        Check(PipeTransport.StartDelayMs(7, rng) <= 60000, "delay including jitter capped at one minute");
        Check(PipeTransport.StartDelayMs(1000, rng) <= 60000, "long failure streak stays capped");
        GlassPresenter presenter = new GlassPresenter(new GlassPeek(delegate { }));
        presenter.Adapt(false, 1);
        Check(presenter.LightGoal, "explicit light overrides disabled adaptation");
        presenter.Adapt(false, 0);
        Check(!presenter.LightGoal, "explicit dark overrides light");
        presenter.Adapt(false);
        Check(!presenter.LightGoal, "default appearance preserves original dark fallback");
        Console.WriteLine("regression: {0}/{1} checks OK", checks - failures, checks);
        return failures == 0 ? 0 : 1;
    }

    sealed class GlassServiceGuard : IDisposable
    {
        public GlassService Service;
        public void Dispose() { if (Service != null) Service.Dispose(); }
    }
}
