# Liquid Glass Control 0.14.0-beta.2 — Performance Refresh

Windows 11 x64 prerelease. Download the complete portable ZIP and extract it before launching `LiquidGlassControl.exe`. Keep `art/`, `fonts/` and `lgengine/` beside the controller. The separate EXE alone does not include these companions.

## Changes
- Shared high-resolution animation clock targets 60 Hz and stops when idle.
- Direct GPU window drawing avoids the previous per-frame GPU-to-GDI readback.
- All dropdowns use a 180 ms fade/8 DIP slide to open and a 120 ms continuous close, with a rotating arrow. Optical centering applies across all five languages.
- Bounded text-layout caching avoids rebuilding layouts on warm frames. Hidden/minimized UI releases drawing resources; graphics and custom fonts initialize on first drawing.
- Shell material attachment responds earlier to surface creation. Keyboard layout switcher glass is disabled, including legacy saved flags.
- Remove duplicate embedded images and unused menu construction; retain complete language fonts, required artwork and engine licenses.

## Verification and limits
The isolated controller regression suite, multi-DPI/five-language rendering and native adapter self-tests pass. Local measurements found a 59.9 Hz animation clock and approximately 16 MiB tray working set with 2.6 MiB private committed memory, with effects disabled. Working set, private commit and total system/graphics cost are different measures. Frame rate and memory vary with Windows, GPU load, enabled effects and the active page; 60 fps is a target, not a guarantee.

Live taskbar/Start/notification appearance and restoration have not been exhaustively verified on every Windows build. This remains a prerelease. Binaries are unsigned and may be blocked by Windows application-control policy. Do not disable protections to run them.

## Upgrade safely
Restore managed effects and exit the previous version before upgrading. Keep existing settings and recovery records until restoration succeeds. Extract the new portable package into a separate writable folder. No personal settings, recovery journals, test logs or test executable are included.

GPL-3.0-only controller/engine corresponding source and the pinned MIT LGEngine 2.7.16.0 source accompany this release. Font licenses and third-party notices are included.
