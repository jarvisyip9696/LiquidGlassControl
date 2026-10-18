# Imported Windhawk material code

Source: user-supplied snapshots of m417z's Windows 11 Taskbar Styler 1.10,
Start Menu Styler 1.7, Notification Center Styler 1.7 and File Explorer Styler 1.7.
Upstream: https://github.com/m417z/my-windhawk-mods
Author: m417z, https://github.com/m417z
License: GNU GPL version 3; full text in COPYING.

The four *-LiquidGlass.h files retain only static material assignments from
`g_themeLiquidGlass`: backgrounds, borders, strokes and corner radii. Layout,
sizing, visibility, text replacement and state-specific style assignments were
removed. Ancestor selectors are matched by the standalone adapter. Selectors
with property predicates are unsupported and are skipped.

xaml_subscription.h adapts the upstream RegOpenKeyExW_Hook and
RegQueryValueExW_Hook compatibility logic. Windhawk's hook installer is replaced
by statically linked MinHook 1.3.4. Only the subscription thread receives the
optional composition-diagnostics override; registry values are never written.

Notification host readiness follows the user-supplied Notification Center
Styler 1.7 snapshot: `ShellExperienceHost.exe` waits for
`Windows.UI.Core.CoreWindow`, and `ShellHost.exe` waits for
`ControlCenterWindow`. The standalone controller posts its bootstrap to that
window's queue after creation, including hidden host windows. It does not
probe notification worker threads. Window creation is observed externally
with WinEvent rather than importing Windhawk's CreateWindowInBand hooks.

windhawk_materials.h and the modified engine are distributed under GPL-3.0-only.
This does not import the Windhawk application/runtime. WindhawkBlur markup is
mapped to the existing standalone composition brush; user blur/tint/opacity
settings take precedence. This is a material-only port, not a complete Styler
or a full visual-state/theme-resource/private-symbol engine.

The corresponding engine source and build scripts are included in the portable
ZIP as LiquidGlass-engine-source.zip. This directory's license does not select
a separate license for controller UI code; see the docs/LICENSING.md for its authorization scope.
