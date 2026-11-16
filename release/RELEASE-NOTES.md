# Liquid Glass Control 0.14.1 — Maintenance Release

This release improves source readability and keeps the existing application behavior and settings format. It introduces no new glass effects or performance claims.

## Changes

- Expand compressed C++ and C# functions and control flow into readable blocks across the controller, engine, adapter, and tests.
- Clarify variable names in spring animation, capsule placement, and Traditional Chinese translation lookup.
- Remove duplicate self-test preprocessor guards.
- Add a shared clang-format configuration and contributor guidance.
- Make the translation coverage checker accept ordinary whitespace and line breaks without changing the translation catalog.

## Downloads

For Windows 11 x64, download `LiquidGlassControl-0.14.1-win11-x64.zip` and extract it before launching `LiquidGlassControl.exe`. Keep `art/`, `fonts/`, and `lgengine/` beside the controller. The standalone EXE does not contain the LGEngine adapter and host; use the ZIP for the complete application.

The portable ZIP includes the corresponding controller/engine source, pinned MIT LGEngine 2.7.16.0 source, third-party notices, and font licenses. Separate source downloads and SHA-256 checksums are also provided. Personal settings, recovery records, test reports, and the test executable are excluded.

## Validation and limits

Before the version update, the local isolated build and full controller self-test passed, along with all 56 wire-protocol checks, animation/capture checks, and localization tests. Translation coverage remains 253 catalog entries and 228 UI keys. The release workflow builds and tests the tagged version before producing these downloads.

This release does not add new live shell or hardware validation. Appearance and restoration can vary across Windows builds. Dock magnification follows the first taskbar only. Binaries remain unsigned and may be blocked by Windows application-control policy; do not disable protections to run them.

## Upgrading

Restore managed effects and exit the previous version first. Extract this version into a separate writable folder, and retain existing settings and recovery records until restoration succeeds.
