# Figma UI candidate (v0.13.3)

The UI uses the supplied Figma file `lfSI4TPqkSv2AkKAa2mZGW`:
sidebar navigation, light blue cards, a material study column, and the
Effects / Materials / LGEngine sections. Dark and White are additional
Interface color choices; the previous seven saved color values retain
their meaning. The OS continues to provide the native title bar.

The v0.13.3 controls and handlers remain in use. Materials has a surface
selector so Taskbar, Start, Notifications, Calendar, Explorer and Dock
retain their independent settings. Effects rows can also expand inline.
The right-hand material images are exported design illustrations, not
live renderings of slider values.

Build without replacing the deployed executable:

```powershell
./build.ps1 -OutputDirectory build/figma-ui-candidate -BuildTests
```

For LGEngine functionality the candidate needs the complete `lgengine`
runtime directory alongside its executable, as with the existing release.
The prepared candidate includes the runtime from the local v0.13.3 package.
It reads settings next to its own executable, not from the deployed app.
Exit the old instance before running the candidate: normal builds share
the existing single-instance mutex. No shortcut is changed automatically.

Validation:

```powershell
build/figma-ui-candidate/LiquidGlassTests.exe --self-test
build/figma-ui-candidate/LiquidGlassControl.exe --ui-preview
```

`--ui-preview` uses a separate window class and mutex, renders the app's own
hidden controls at 96 DPI, and writes fifteen BMP files next to the candidate.
It does not apply system effects. Regression tests exercise 96/144/192 DPI,
five languages, scrolling, secondary navigation and isolated recovery fixtures.
Registry fixture tests require a process permitted to create their uniquely
named keys under `HKCU/Software/LiquidGlassWindow`; sandbox denial is a test
failure, not a product failure. Live material and LGEngine animation behavior
should still be checked on the target desktop before deployment.
