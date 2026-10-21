# LGEngine desktop integration

The C++17 client and adapter use the official LGEngine 2.7.15.0 host at
`4e24ca4e55e6b8c6b119a1b2325002a139deff63` (protocol 1.1).
`prepare-upstream.ps1` downloads/verifies the pinned checkout in
`build/LGEngine-2.7.15`; `build-native.ps1` compiles it and the adapter.
Release route: `stage-host.ps1`, `build-native.ps1` and CI build that pinned host
unpatched. Developer route: `build-candidate.ps1` applies `fixes.patch` (explicit
appearance mode) to `build/LGEngine-upstream` for local candidates only; none of
it ships in a release.

## User interface

**System glass > LGEngine** contains card arrival, independent indicator arrival,
slosh, knob, Replay, Sample and the volume/brightness OSD switch. Controls use the
main window's theme; the owned adapter has no floating control window. The card
and its three decorative hover-glow chips are visible only on this page.
Selections are saved in the `LGEngine` settings section.

When enabled, volume/brightness OSD also works on other pages and in the tray,
near the bottom of the active monitor. It has a glyph, track, fill, animated knob
and number. Volume uses endpoint callbacks and reattaches on device changes;
brightness uses Windows power notifications and depends on hardware support.
First readings initialize silently; display dimming/state changes rebaseline.
The sample changes only the rendered lens, never system levels or keys.
High contrast suppresses the optical surfaces. Reduced motion disables card
motion and slosh. Timers run only for animation, auto-hide and delayed replay or
one-shot owner visibility rechecks; idle clients wait on events.

## Runtime and distribution

The host owns capture, rendering, glass windows, hover and animation. This is not
a C++ engine port: the host uses Windows .NET Framework 4.x. Clients share a host
in the dedicated `lgcontrol11` namespace and never terminate a shared host. This
private host runs next to the default LGEngine host of the owner's Liquid Glass
app (separate pipe names), not instead of it, so both can run at the same time.
Hashed host names and `host-current.ini` allow safe desktop upgrades.

The full portable ZIP includes controller, adapter, host, example and source.
The separate controller EXE does not include the companion runtime. LGEngine
(source and host EXE) is MIT-licensed by DK, who confirmed redistribution; see
`docs/LICENSING.md` and `release/LGEngine Distribution.txt`. The MIT notice
ships next to the host, and upstream notices remain intact.

Windows toasts and the native volume flyout are not intercepted or suppressed.
This does not migrate taskbar, Start, Explorer or settings UI into the optical
renderer. No latency, energy savings or full optical parity result is claimed.

## Build and validation

```powershell
./integrations/LGEngine/build-native-candidate.ps1 -OutputDirectory build/candidates/release
./integrations/LGEngine/validate.ps1 -CandidateDirectory build/candidates/release -Integration
./integrations/LGEngine/test-host-client.ps1 -CandidateDirectory build/candidates/release
./package.ps1 -ExecutablePath build/candidates/release/LiquidGlassControl.exe -IncludeLGEngine
```

The adapter `--self-test` checks OSD springs, ink alpha, geometry and masks.
The isolated bridge tests verify page controls and background OSD state, including
hide, re-enable, invalid requests and owner shutdown. Host-client tests check
shared hosts, state updates, resizing, recovery and independent cleanup. Never
use an old validation report as evidence for a rebuilt binary.
