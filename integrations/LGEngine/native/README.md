# C++ client → original LGEngineHost

## Desktop integration

The desktop controller now exposes the animation, indicator arrival, slosh, knob,
Replay and Sample controls in **System glass > LGEngine**. Owned adapters have no
visible control window. The card and its three glow chips appear only on this page.
The enabled volume/brightness lens continues working on other pages and in the
tray, appearing near the bottom of the active monitor. High contrast disables it.
Animation selections and the OSD switch are saved in the `LGEngine` settings section.
The sample never changes system volume or brightness. Brightness events depend on
the display and Windows reporting brightness notifications.

`enable-host-v11.ps1` applies the local protocol adapter to the upstream checkout;
`host-v11-methods.cs` exposes the engine's existing motion, buttons, pointer, slosh,
busy and cut controls. This is a local extension, not an upstream 1.1 release.
The dedicated `lgcontrol11` host namespace avoids attaching to an older host used
by another application. Ordinary Windows toast notifications are not intercepted
or replaced, and the Windows volume flyout is not suppressed.

The candidate now uses the original LGEngine host through its version-1 named
pipe and shared-memory protocol. The previous partial C++ renderer was removed.
The app and adapter are C++; the original host remains C#/.NET Framework.
The client never captures the desktop, renders glass or creates glass windows.
It supplies a spec, premultiplied content pixels and state only.

Reusable header: `../client/lg_client.h` (`lghost::Client`). Features:

- SID/session-based discovery, on-demand startup, authenticated same-user/session
  peer checks, version negotiation and bounded frame/section validation.
- State seqlocks, double-buffered full content publication and host wake events.
- Pipe I/O on a worker thread; setters coalesce. Connected idle workers wait on
  events without polling. Host loss triggers reconnect and retained surface recovery.
- HWND messages for connection, loss, restart, shown/hidden, appearance and failure.

One retained surface per connection; multiple clients share the host. This is
an initial protocol-v1 client, not full C# API parity. Content copies the full
bitmap. `setSpec` recreates the surface; position/visibility/content use shared
memory. Resize clears old contents. Initial failure reports Failed and stays
offline; create a new client to retry. Restarted reports the new handshake;
Shown confirms a restored visible surface. The caller owns/pumps its notify HWND.

Build and validate (normal user permissions):

```powershell
./integrations/LGEngine/build-native-candidate.ps1
./integrations/LGEngine/test-host-client.ps1
./integrations/LGEngine/validate.ps1 -CandidateDirectory build/candidates/lgengine-native -Integration
```

The build compiles the local original engine checkout and copies its standalone
`LGEngineHost.exe` beside the C++ adapter. The client needs no LGEngine DLL.
CMake can build `LiquidGlassLGNative`; supply the original host beside its output.
Consumers can link the `LGEngineCppClient` interface target, or include the header
and link `user32`, `advapi32`, `bcrypt`. Use Windows 10/11 and C++17.

Run `build/candidates/lgengine-native/LiquidGlassControl.exe` → Settings → Glass →
LGEngine optical preview, or `lgengine/LiquidGlass.LGPreview.exe --standalone`.
Move the ordinary app window to reposition the separate host-owned optical card
over the desktop. Closing the app disconnects its surface. This does not migrate
taskbar, Explorer, the main settings UI or other applications' windows.

The host handles background adaptation, rendering, hover and animations. Protocol
v1 has no forced-light appearance interface; Light/Dark in the main controller
does not override host adaptation. Blur, tint strength, corners, language, reduced
motion and power saving are forwarded. Glass z-order/input remain host behavior.

The controller job permits explicit breakaway. Its adapter dies with it; the shared
host starts outside the job and can keep serving other apps. The client never
kills that host. A sandbox job can forbid breakaway, requiring normal user execution.
The dedicated test uses a unique suffix and offscreen surfaces; only its isolated
test host is terminated to verify restart. Two clients, shared PID, loss/recovery,
content size, rapid replacement and independent cleanup checks passed 12/12 with
normal permissions. The final controller, adapter and 20/20 bridge checks also
pass; see candidate validation.json for the tested binary hashes.

No 10 ms latency, CPU speedup, power or visual-parity result is claimed here.
This is not a no-.NET engine port. Evaluated protocol: LGEngine commit
`b6e93053b3eb001b310273b1ece1e58ec448106d` (`GlassProtocol.cs`, `GlassSection.cs`,
`PipeTransport.cs`). Upstream has not chosen a license; the local candidate remains
outside normal release packaging pending permission to distribute its host.

## Preview process (LiquidGlass.LGPreview.exe)

- No polling timer: parent exit is a registered wait (posts WM_CLOSE), high contrast is WM_SETTINGCHANGE, its own
  minimize/restore is WM_SIZE, and the owner window's show/hide/minimize come from out-of-context WinEvent hooks on the owner pid.
  The only timers left run while the OSD knob animates (16 ms), the OSD auto-hide (1.8 s) and the Replay delay; all are one-shot or
  killed at rest.
- Win32 controls: card arrival (none, fade, slide, zoom, slide+zoom, drop, pop, melt, ripple), indicator arrival (same as card or
  the same list), slosh (lean, bounce, pulse, none), knob (round, pill, lens), Replay, Sample indicator, Indicator on/off.
  Reduced motion forces motion 0 and slosh none and disables those combos.
- The card shows three decorative glass chips (SetButtons glow + label masks; hover on unless reduced motion).
- OSD lens (`osd.h`): a second client (kind 1, 300x64 at 100%) under the card with a glyph, track, fill, knob and level number.
  Volume comes from the default render endpoint's IAudioEndpointVolume callback (re-registered on default-device change),
  brightness from RegisterPowerSettingNotification. The first reading of each source only initialises. Sample indicator
  sweeps the lens only; the app never sets the system volume or brightness and never sends keys.
- Display dimming and display-state changes re-baseline the brightness silently (also for ~3 s after); ambient-light adaptive brightness
  can still show the OSD. Owner show/hide/minimize events also arm one 400 ms re-check, since they can arrive before the change finishes.
- Needs a host that answers protocol 1.1 (`hostMinor() >= 1`); on an older host the chips and the lens are not shown.
- `--self-test` also checks the springs, the alpha convention (slider odd, ink even), the lens spec and the chip masks.
