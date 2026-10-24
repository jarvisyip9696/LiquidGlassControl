## Motion Refresh

A redesigned native workspace with smoother controls and improved localization.

### Changes

- Redesigned App glass, System glass, LGEngine, and Settings pages.
- Smooth switch motion targeting 60 fps while active, with continuous reversal and reduced-motion support.
- Refined dropdown interactions. Arrival and Indicator entrance retain their original menu presentation.
- Custom-rendered interface text now follows the selected interface language.
- Improved keyboard-layout panel contrast over light backgrounds.
- Removed File Explorer styling, including its controls and legacy activation paths.
- Portable downloads now include the interface artwork, private fonts and font licenses required by the redesigned UI.

### Download

Use the complete **Windows 11 x64 portable ZIP**. Extract everything and keep `art/`, `fonts/`, and `lgengine/` beside `LiquidGlassControl.exe`. The controller EXE alone does not include all companion files.

The package includes LGEngine 2.7.16.0, licenses, corresponding controller source, and the pinned LGEngine source archive. SHA-256 checksums accompany the downloads. Personal settings, recovery journals, local logs, and test images are excluded.

### Validation and limitations

This is a beta release. Build checks and isolated self-tests cover configuration, restoration fixtures, UI rendering, language selection, menus, and motion calculations. They do not certify every live shell surface or a measured 60 fps presentation rate.

The application and embedded engine are unsigned. Windows application-control policy may block them. Use a trusted, approved build; do not disable security protections. Shell effects vary by Windows version and other styling tools. Brightness integration depends on hardware support. Arbitrary application content is not guaranteed to become transparent.

Before upgrading, restore managed effects and retain settings and recovery records until restoration succeeds.
