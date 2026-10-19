param([string]$Root = (Join-Path $PSScriptRoot '../../build/LGEngine-upstream'))
$ErrorActionPreference = 'Stop'
$path = Join-Path $Root 'src/GlassHostServer.cs'
$text = [IO.File]::ReadAllText($path)
if (-not $text.Contains('// LiquidGlassControl resize flow adapter.')) {
    $text = $text.Replace('public int W, H; }', 'public int W, H; public bool Flow; }')
    $text = $text.Replace('int w = r.I32(), h = r.I32();', 'int w = r.I32(), h = r.I32(); uint flags = r.Remaining >= 4 ? r.U32() : 0; bool flow = (flags & 1) != 0; // LiquidGlassControl resize flow adapter.')
    $text = $text.Replace('PostResize(s, w, h, seq);', 'PostResize(s, w, h, seq, flow);')
    $text = $text.Replace('d.H = h;', 'd.H = h; d.Flow = flow;')
    $text = $text.Replace('PostResize(HostSurface s, int w, int h, uint seq)', 'PostResize(HostSurface s, int w, int h, uint seq, bool flow)')
    $text = $text.Replace('Host.ResizeOnThread(s, w, h, seq)', 'Host.ResizeOnThread(s, w, h, seq, flow)')
    $text = $text.Replace('PostResize(s, d.W, d.H, d.Seq)', 'PostResize(s, d.W, d.H, d.Seq, d.Flow)')
    $text = $text.Replace('ResizeOnThread(HostSurface s, int w, int h, uint seq)', 'ResizeOnThread(HostSurface s, int w, int h, uint seq, bool flow)')
    $text = $text.Replace('n.Size = new Size(w, h);', 'if(flow && n.Size.Width > 0) { n.FlowFrom = Math.Max(0.86f, Math.Min(1.16f, n.Size.Width / (float)w)); n.FlowVer++; } n.Size = new Size(w, h);')
    $readerPath = Join-Path $Root 'src/GlassProtocol.cs'
    $reader = [IO.File]::ReadAllText($readerPath)
    if (-not $reader.Contains('public int Remaining')) { $reader = $reader.Replace('        bool Need(int n)', '        public int Remaining { get { return end - pos; } }' + "`r`n        bool Need(int n)") }
    [IO.File]::WriteAllText($readerPath, $reader)
    [IO.File]::WriteAllText($path, $text)
}
if ($text.Contains('// LiquidGlassControl protocol 1.1 adapter.')) { return }
if (-not $text.Contains('case Msg.Stats: OnStats(seq, r); break;')) { throw 'Unsupported upstream host source.' }
$text = $text.Replace('sealed class HostEvents : IGlassEvents', 'sealed class HostEvents : IGlassEvents, IGlassLightEvents')
$text = $text.Replace('public void Shown() {', 'public void LightMoved(float light) { PayloadWriter w = new PayloadWriter(); w.U32(s.Id); w.F32(light); Out(0x24, w); }' + "`r`n            public void Shown() {")
$text = $text.Replace('w.Str("LGEngine host");', 'w.Str("LGEngine host"); w.U16(1);')
$text = $text.Replace('case Msg.Stats: OnStats(seq, r); break;', 'case Msg.Stats: OnStats(seq, r); break;' + "`r`n                    case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: OnExtended(type, seq, r); break;")
$methods = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'host-v11-methods.cs'))
$text = $text.Replace('            void OnStats(uint seq, PayloadReader r)', $methods + "`r`n            void OnStats(uint seq, PayloadReader r)")
[IO.File]::WriteAllText($path, $text)
