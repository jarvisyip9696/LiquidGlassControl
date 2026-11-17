# Releasing / 发布

The repository and v0.9 preview downloads are public. The owner's and DK's original code uses GPL-3.0-only; see LICENSING.md for authorization scope and contributor confirmation. Imported Styler material data and the modified engine carry GPLv3; preserve the supplied licenses and corresponding engine source in portable packages. See CODE-SIGNING.md for signing status. Workflows create drafts for maintainer review; publishing a release makes its attached downloads public.

1. Merge the intended changes to `main`; confirm Windows CI is green.
2. Update `LG_VERSION` in `src/app_version.h`; captions/About, packaging and CI use it. Follow VERSIONING.md and do not reuse a tag.
3. Build and verify on Windows 11. Test all three UI languages and live effect restoration. CI factory/unit tests do not verify live XAML styling. Complete the validation in ARCHITECTURE.md before release.
4. Commit version changes, then push a tag matching the package version, for example `v0.9`:

   ```powershell
   git switch main
   git pull --ff-only
   git tag -a v0.9 -m "Liquid Glass Control 0.9"
   git push origin v0.9
   ```

5. The Release workflow builds and tests the tagged source, packages only allowlisted files and creates a **draft** release with EXE, ZIP and SHA256SUMS.txt attached.
6. Open the draft, download/extract the ZIP, and inspect release notes, version, dependencies and limitations. Publish when ready. Downloads remain restricted while the repository is private.

For a manual package:

```powershell
./package.ps1
# Or use a CMake-built executable:
./package.ps1 -ExecutablePath build/cmake/Release/LiquidGlassControl.exe
```

Never upload your personal `settings.ini`, original-style backups or test logs as release assets. The standalone engine is embedded; Windhawk is not required. Preserve the guide's dependency and compatibility notes. Packages are currently unsigned.

The self-test program is separate from the user EXE. When passing an existing
build to package.ps1, keep LiquidGlassTests.exe in the same build directory, or
pass -TestExecutablePath explicitly. Only the user EXE is included in downloads.
