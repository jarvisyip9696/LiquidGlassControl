# Liquid Glass Control 0.14.0 — Performance Refresh

Windows 11 x64 portable release, the first full release of the 0.14 line. Download the ZIP and extract it before launching `LiquidGlassControl.exe`. Keep `art/`, `fonts/` and `lgengine/` beside the controller.

## Faster and lighter
- Shared high-resolution animation clock targets 60 Hz and stops when idle.
- Direct GPU window drawing avoids the previous per-frame GPU-to-GDI readback.
- All dropdowns use a 180 ms fade/8 DIP slide to open and a 120 ms continuous close, with a rotating arrow.
- Bounded text-layout caching avoids rebuilding layouts on warm frames. Hidden/minimized UI releases drawing resources; graphics and custom fonts initialize on first drawing.
- Shell material attachment responds earlier to surface creation. Keyboard layout switcher glass is disabled, including legacy saved flags.
- Duplicate embedded images and unused menu construction are removed; complete language fonts, required artwork and engine licenses are kept.

## Fixed since 0.14.0-beta.2
- The first launch follows the Windows display language instead of always starting in Chinese (settings kept from an older version keep Chinese), and Apply no longer switches the interface language.
- English, Polish and Vietnamese labels fit: buttons grow instead of cutting words, and long tab, dropdown and subtitle text ends in an ellipsis instead of overlapping. Latin text inside Chinese labels uses the Latin font.
- The window menu (Alt+Space), the tray tooltip and the System glass connection status are translated.
- Status messages appear on the page they belong to, and a long engine error wraps onto two lines instead of losing its remedy.
- The arrow keys move through the whole window list and scroll it.
- The combined Volume and brightness and Notifications and calendar switches read On when either half is on, and one click turns both off.
- Automatic tints follow a Windows light/dark switch straight away.
- Glass rules inside the calendar flyout follow the Calendar switch instead of Notifications.
- The shell engine no longer leaks element handles or re-applies glass to a replaced element after Explorer rebuilds a surface.
- Changing unrelated Windows settings no longer re-applies every effect. A second launch brings the running window to the front. A failed restore on exit shows the window instead of quitting silently.

## Verification and limits
The release build, the localization check and the controller and adapter self-tests run in CI. On 0.14.0-beta.2, local measurements found a 59.9 Hz animation clock and approximately 16 MiB tray working set with 2.6 MiB private committed memory, with effects disabled. Frame rate and memory vary with Windows, GPU load, enabled effects and the active page; 60 fps is a target, not a guarantee.

Live taskbar/Start/notification appearance and restoration have not been verified on every Windows build. Dock magnification follows the first taskbar only. Binaries are unsigned and may be blocked by Windows application-control policy. Do not disable protections to run them.

## Upgrade safely
Restore managed effects and exit the previous version before upgrading. Keep existing settings and recovery records until restoration succeeds. Extract the new portable package into a separate writable folder. No personal settings, recovery journals, test logs or test executable are included.

GPL-3.0-only controller/engine corresponding source and the pinned MIT LGEngine 2.7.16.0 source accompany this release. Font licenses and third-party notices are included.
