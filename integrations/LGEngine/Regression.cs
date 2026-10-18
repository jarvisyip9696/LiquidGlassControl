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
        // Synthetic backdrop statistics need no desktop capture or window.
        {
            GlassPeek peek = new GlassPeek(delegate { });
            GlassPresenter adaptive = new GlassPresenter(peek);
            peek.Backdrop.MidLuma = 240;
            adaptive.Adapt(true, -1);
            Check(adaptive.LightGoal, "bright background selects light glass");
            peek.Backdrop.MidLuma = 135;
            adaptive.Adapt(true, -1);
            Check(adaptive.LightGoal, "neutral background retains light to avoid flicker");
            peek.Backdrop.MidLuma = 20;
            adaptive.Adapt(true, -1);
            Check(!adaptive.LightGoal, "dark background selects dark glass");
            peek.Backdrop.MidLuma = 135;
            adaptive.Adapt(true, -1);
            Check(!adaptive.LightGoal, "neutral background retains dark to avoid flicker");
            adaptive.Adapt(false, 1);
            Check(adaptive.LightGoal, "manual light overrides dark background");
            peek.Backdrop.MidLuma = 240;
            adaptive.Adapt(false, 0);
            Check(!adaptive.LightGoal, "manual dark overrides bright background");
            adaptive.Adapt(true, -1);
            Check(adaptive.LightGoal, "returning to automatic resumes background adaptation");
        }
        Console.WriteLine("regression: {0}/{1} checks OK", checks - failures, checks);
        return failures == 0 ? 0 : 1;
    }

    sealed class GlassServiceGuard : IDisposable
    {
        public GlassService Service;
        public void Dispose() { if (Service != null) Service.Dispose(); }
    }
}
