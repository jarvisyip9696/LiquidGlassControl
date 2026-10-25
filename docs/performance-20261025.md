# App performance verification — 2026-10-25

Independent optimized build: `build/releases/app-optimized-20261024/LiquidGlassControl.exe`.
This build has NOT replaced the deployed application or desktop shortcut, following AGENTS.md.

## Changes
- Shared high-resolution 60 Hz animation scheduler; stops when no animations are active.
- Direct GPU window presentation removes the previous per-frame GPU-to-GDI readback. Software rendering remains available for screenshots and fallback.
- Text layout cache is bounded to 64 entries. Warm frames reuse layouts. Hidden/minimized UI releases owned fonts, layouts, images and render targets; resources recreate on demand.
- Avoid repeated window-list scans on settings/system pages, unused menu construction, and duplicate embedded PNG assets.
- Preserve full Inter and Noto Sans SC fonts and all five languages. Keep required runtime artwork, engine host, and license notices.
- Generated BMP proofs converted losslessly to verified PNG; reproducible header cache removed after exact archive validation. No source, Git history, backups, settings or recovery journals removed.

## Validation
Full self-test exited 0. Tests use isolated temporary HKCU fixture keys, not live user settings.
- Shared animation clock: 59.9 Hz.
- Entrance, Knob and Glass tint: native open/close loops each rendered 11 opening and 8 closing frames, first frame approximately 17 ms, alpha 0–255, 8 DIP displacement, zero presentation errors.
- Direct GPU window rendering: 0.662 ms/frame average over the test loop.
- Software full-page render average: 7.388 ms uncached, 5.721 ms cached; warmed frames create zero text layouts.
- Resource pressure test: private memory 112.273 → 58.645 MiB after idle resource release; working set 95.984 → 59.930 MiB. These are test-process measurements, not a universal steady-state claim.
- Eight light/dark reference images compared: mean RGB channel difference 0.0365–0.0570 on a 0–255 scale. Representative rendered image visually checked. Screenshot path now rejects entirely black frames.
- Existing five-language and multi-DPI optical alignment checks passed.

## Size
- Main EXE: 1,691,136 → 1,428,480 bytes (15.5% reduction).
- Complete independent runtime: 20.95 MiB logical, 13.00 MiB allocated with per-file LZX compression.
- Build directory: approximately 2,564 → 627 MiB; retained historical packages, source recovery records and lossless proof images.
- Per-file compression preserves file content hashes; this does not change Windows system compression settings.

## Limits
The scheduler targets 60 Hz; average draw time does not guarantee every displayed frame under GPU saturation, other applications, or Windows shell scheduling. External taskbar/Start/notification rendering is controlled by Windows. No sustained external-shell/game-load benchmark was performed. No deployment was performed.

Raw evidence: `build/candidates/app-performance-20261024/self-test.txt`, `ui-performance.json`, `dropdown-motion.log`, `runtime-size.json` and generated PNG proofs. Cleanup manifests remain under `build/workspace-compaction-*.json`.
## Follow-up: tray memory — 2026-10-25
User reported approximately 17 MB for the main app. Read-only process inspection found no running instance. Both versions were then launched with `--background` for three seconds using the existing effects-disabled settings, measured, and closed normally.

- Previous optimized version: working set 15.953 MiB; private committed memory 2.574 MiB.
- Lazy renderer version: working set 15.914 MiB; private committed memory 2.555 MiB.
- Difference is small and within normal launch-to-launch variation; no substantial reduction from this change is claimed. Working set and private commit measure different things; neither value represents total graphics/system cost.

Renderer construction now allocates no Direct2D, DirectWrite, custom-font or WIC resources until actual drawing. Idle trim additionally releases WIC. Regression checks cover unused cache operations, full resource release, redraw after release, existing animations, and all languages. Full suite exited 0; shared animation clock 59.9 Hz. The new executable was hash-verified and updated at the existing desktop shortcut target; the prior optimized executable was backed up. Original root application and source recovery records remain preserved.

No forced working-set emptying or periodic memory purge was introduced. The remaining baseline includes Win32 controls and loaded Windows libraries. No sub-10-MB guarantee is supported by these measurements. Raw tray samples are `tray-memory-before.json` and `tray-memory-after.json` in the candidate directory.
