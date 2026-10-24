# Application page, White / Dark review

This phase rebuilds only **应用玻璃** in C++ / Win32. The other four
requested pages await visual approval; their previous implementation is
retained. The application-material, system-material, LGEngine, configuration
and tray backends are not replaced.

`src/app_glass_ui.h` owns the native presentation adapter:

- Direct2D antialiased shapes, compositing and design-space layout.
- DirectWrite text using a private collection of Inter and Noto Sans SC.
- Shared geometry and separate White / Dark tokens, including the Dark
  application page's translucent cards and controls.
- Native Win32 dropdown menus, mouse capture, scrollable window rows, Tab /
  Shift+Tab focus, Enter / Space activation, row arrows and F5 refresh.
- Existing hidden native controls retain selection state and command handlers.
  Production window data and application icons come from the existing cache.
- Existing extra scope, border, title text and title-glass settings remain
  accessible through the native window system menu's advanced-settings command.

The material images and icons are exports from the supplied Figma file.
They are illustrations, not live renderings of target windows. Card alpha
is composited over the application background; this is not desktop backdrop
blur. The soft card shadow is an approximation. Native title-bar metrics and
outer window corners remain controlled by Windows and can differ from the
52px / 18px design values. A strict pixel-for-pixel match is not claimed.

Build separately from the deployed copy:

```powershell
./build.ps1 -OutputDirectory build/app-page-review -BuildTests
```

The portable directory includes `art/` and `fonts/`. Do not move only the EXE.
The original LGEngine runtime is also required for normal production features.
Fonts are loaded privately, not installed into Windows. Their OFL license files
are in `fonts/`.

Open the actual review window:

```powershell
build/app-page-review/LiquidGlassControl.exe --review-app-white
build/app-page-review/LiquidGlassControl.exe --review-app-dark
```

Review instances use their own mutexes and window class. They display the
reference's two **sample** applications and v0.12.0-beta.4 reference identity,
but still use the current v0.13.3 source backend. Apply / Restore in this mode
do not change any real window. No system effects, tray icon or saved preferences
are applied. The displayed connection status is real, so an unconnected review
does not claim that LGEngine is connected. Navigation now opens the existing
native pages in review mode, with effect-changing commands blocked. Use the native system menu to switch White / Dark; close exits the
review window normally.

After the window is visible, the executable saves its own running client and
full native frame to `app-white-running.bmp` / `app-white-window.bmp` (or Dark).
These proofs contain no captured pixels from other applications.

Run `LiquidGlassTests.exe --self-test` for backend regressions plus font-load,
Direct2D rendering and read-only review interaction checks. These tests write
only isolated fixtures; registry fixture tests need permission to create their
uniquely named HKCU test keys.

## Verification outcome

The earlier application-page build ran in both themes. Its private-font,
Direct2D rendering, keyboard review isolation and existing backend regression
tests passed. The two `app-*-window.png` images show that successful running
build, including the native Windows caption.

The final build compiled successfully, but Windows application control blocked
both the final test executable and the normal review executable from starting.
The historical `self-test.txt` is **not** a passing report for that final binary.
The added Direct2D DPI pointer-coordinate test is therefore not yet executed.
The exact binary hashes, report date and image dates are in `validation.json`.
No Windows security policy was changed, and no alternate loader was used.
The current deployed executables, user settings and desktop shortcut were
preserved. Trusted signing / application-control resolution is required before
the final candidate can be verified interactively on this host.
