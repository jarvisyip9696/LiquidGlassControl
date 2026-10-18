# C++ client → original LGEngineHost

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
This is not a no-.NET engine port. Evaluated protocol: domk6966/LGEngine commit
`b6e93053b3eb001b310273b1ece1e58ec448106d` (`GlassProtocol.cs`, `GlassSection.cs`,
`PipeTransport.cs`). Upstream has not chosen a license; the local candidate remains
outside normal release packaging pending permission to distribute its host.
