# Contributing

Thank you for contributing to Liquid Glass Control. Keep changes focused, preserve native Windows behavior, and make restoration reliable.

## Before changing code

- Read the relevant implementation and its callers before editing.
- Keep source, Git history, user settings, and recovery records. Clean only identified generated artifacts.
- Build into a separate directory. Never overwrite an existing installation or desktop shortcut as part of a routine build.
- Do not add a Windhawk runtime dependency. The application uses its own embedded engine; imported material code retains its original license.
- File Explorer styling is no longer a supported feature. Do not reactivate it through old settings or application-wide controls.

## Build

Use Windows 11 x64 with either Visual Studio 2022 Build Tools and the Windows SDK, or a compatible MinGW clang++ toolchain. PowerShell is required for the project scripts.

### MinGW clang++

```powershell
./prepare-winrt.ps1
./build.ps1 -OutputDirectory build/dev -BuildTests
$test = Start-Process build/dev/LiquidGlassTests.exe -ArgumentList '--self-test' -WindowStyle Hidden -PassThru -Wait
if ($test.ExitCode -ne 0) { throw 'Self-test failed' }
Get-Content build/dev/self-test.txt
```

Pass `-Compiler` to select a toolchain explicitly. The script can also detect an already installed Windhawk compiler; that compiler is optional and does not introduce a runtime dependency.

### Visual Studio and CMake

```powershell
./prepare-winrt.ps1
cmake -S . -B build/cmake -A x64
cmake --build build/cmake --config Release
$test = Start-Process build/cmake/Release/LiquidGlassTests.exe -ArgumentList '--self-test' -WindowStyle Hidden -PassThru -Wait
if ($test.ExitCode -ne 0) { throw 'Self-test failed' }
```

The interface needs the generated build's `art/` and `fonts/` directories. LGEngine features also need the staged `lgengine/` runtime. See `integrations/LGEngine/stage-host.ps1` for the pinned host staging process.

## Validation

- Run `./verify-localization.ps1` and the controller self-test.
- Test the affected behavior and restoration path. Include reduced-motion behavior and rapid repeated input when changing animation.
- Keep English, Simplified Chinese, Traditional Chinese, Polish, and Vietnamese supported. Custom-rendered text must use the same translation path as native controls.
- Verify menus with mouse and keyboard input, multiple DPI scales, and both interface themes.
- Self-tests use isolated fixtures. Do not test recovery code against a user's real registry settings or recovery journals.
- Record live Windows testing separately from unit tests. Do not claim measured frame rates, complete transparency, or hardware energy savings without evidence.
- If Windows application control blocks a build, report the failure. Do not alter security policy, rename blocked executables, or change their hash to evade it.

## Pull requests

Create a focused branch, describe the resulting behavior, and include relevant validation and limitations. Preserve licenses for adapted or vendored code. New original contributions use GPL-3.0-only unless explicitly identified otherwise.

Do not commit `build/`, `dist/`, personal settings, local logs, or recovery backups. Use the repository's existing components and avoid unnecessary dependencies.

## Releases

`src/app_version.h` is the version source. Release tags must match it, for example `v0.14.0-beta.1`. Never replace a published tag.

The release workflow builds and packages a draft from the tagged commit. A portable package must include interface assets, fonts and their licenses, the staged LGEngine runtime when enabled, corresponding source, and SHA-256 checksums. User settings and recovery records must stay out of downloads.

Publish only the intended, reviewed assets. See [RELEASING.md](docs/RELEASING.md), [LICENSING.md](docs/LICENSING.md), and [CODE-SIGNING.md](docs/CODE-SIGNING.md).
