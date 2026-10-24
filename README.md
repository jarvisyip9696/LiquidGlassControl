# Liquid Glass Control

**Motion Refresh · v0.14.0-beta.1 · Windows 11 x64**

A native desktop utility for system glass materials, animated controls, and LGEngine volume and brightness indicators. No Windhawk installation or runtime is required.

[Download the complete portable ZIP](https://github.com/jarvisyip9696/LiquidGlassControl/releases/download/v0.14.0-beta.1/LiquidGlassControl-0.14.0-beta.1-win11-x64.zip) · [Release notes](https://github.com/jarvisyip9696/LiquidGlassControl/releases/tag/v0.14.0-beta.1) · [All releases](https://github.com/jarvisyip9696/LiquidGlassControl/releases)

## Features

- System materials for the taskbar, Start menu, notification center, calendar, and keyboard layout switcher.
- Adjustable blur, tint, corner radius, and power-saving fallback.
- Dock hover animation and a capsule-style taskbar appearance.
- Smooth switch motion and dropdown interaction, targeting 60 fps while animations are active. Idle controls stop requesting frames.
- LGEngine entrance styles, separate indicator entrance settings, knob styles, slosh, replay, and a sample indicator.
- English, Simplified Chinese, Traditional Chinese, Polish, and Vietnamese interfaces.
- Saved settings, tray operation, and restoration of managed effects.

**File Explorer styling has been removed.** This application does not make arbitrary application content transparent. The material illustrations are previews, not guarantees of transparency in other windows.

## Get started

1. Download the complete portable ZIP and extract it to a writable folder.
2. Run `LiquidGlassControl.exe`. Keep `art/`, `fonts/`, and `lgengine/` beside it.
3. Open **System glass** to select effects, adjust materials, or configure LGEngine.
4. Open **Settings** to choose the interface language and appearance. Changes save automatically.
5. Closing the window keeps the application in the tray. Use the tray menu to reopen it or restore effects and exit.

The full ZIP includes the required LGEngine 2.7.16.0 companions, licenses, and corresponding source archives. Moving only the controller EXE is insufficient for the complete interface and indicator features.

## Motion Refresh

This prerelease introduces the redesigned workspace, animated switches, localized custom-rendered pages, refined dropdown controls, and improved keyboard-panel contrast. It removes the File Explorer option and prevents legacy settings or the master switch from enabling it again.

Switch animation uses elapsed time and respects Windows reduced-motion preferences. Frame pacing targets 60 fps; actual presentation depends on the display, system scheduling, and load.

## Requirements and limitations

- Windows 11 x64. Brightness integration depends on supported hardware.
- LGEngine uses the Windows-provided .NET Framework runtime.
- Builds are currently unsigned. Windows application-control policy may block an executable or engine. Use an approved, trusted build; do not disable security protections to run it.
- Shell visuals can vary across Windows builds and with other styling tools. A passing self-test does not certify every live shell surface.
- Windows toast notifications and the native volume flyout are not replaced.
- Before upgrading or removing the application, restore managed effects and keep settings and recovery records until restoration succeeds.

## Development

See [CONTRIBUTING.md](CONTRIBUTING.md) for isolated builds, tests, and contribution guidance. [Release instructions](docs/RELEASING.md), [architecture](docs/ARCHITECTURE.md), and [signing policy](docs/CODE-SIGNING.md) provide additional detail.

## License and credits

Liquid Glass Control is licensed under [GPL-3.0-only](LICENSE). LGEngine by [DK](https://github.com/domk6966) remains MIT-licensed. Adapted Styler material code, MinHook, fonts, and other third-party components retain their respective licenses and notices. See [licensing details](docs/LICENSING.md) and the notices included with each release.
