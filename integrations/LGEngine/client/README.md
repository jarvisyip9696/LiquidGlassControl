# LGEngine C++ client

Windows, C++17. This header-only client uses the original LGEngine v1 pipe and
shared-memory protocol. The host remains C#/.NET; this is not a C++ engine port.
One `Client` retains one surface; multiple clients share the user's session host.

## Reference from another project

```cmake
add_subdirectory(path/to/client lgengine-client)
target_link_libraries(MyApp PRIVATE LGEngine::Client)
```

This directory configures independently of LiquidGlassControl, WinUI and Windhawk.
Set `LGENGINE_BUILD_EXAMPLE=ON` to build the console example (also links GDI).
Build with Visual Studio, or a Windows MinGW C++17 compiler. Ship a compatible
host separately; check upstream licensing before redistributing upstream code.

```cpp
#include <lg_client.h>
lghost::Options options;
options.hostPath = LR"(C:\MyApp\LGEngineHost.exe)"; // absolute path
options.notify = myWindow;
options.message = WM_APP + 60;
lghost::Spec spec;
spec.width = 420; spec.height = 180;
spec.x = 100; spec.y = 100;
spec.adapt = true;
lghost::Client surface(options, spec);
surface.setContents(premultipliedArgbPixels); // exactly width * height uint32_t
surface.show();
```

Keep the client alive while the surface is needed. Its destructor disconnects
and removes its surface; it does not stop the shared host. Call `setPlace` for
position and `setContents` only when your content changes. `setSpec` changes
look, size, motion and update rates in place (protocol 1.1; the surface id from
`surfaceId()` stays); only `kind`, `scale`, `label` and `inShots` (and, on a 1.0
host, `fps`/`adapt`/`savePower`/`zoom`/`liquid`/`bounce`) re-create the surface.
A size change clears old pixels: submit the new sized bitmap, or call
`resize(w, h, pixels, flow)` to change size and contents in one step.
`setContents(pixels, dirty)` takes the changed rect (empty = whole frame).

Protocol 1.1 (`hostMinor()` is 1; against a 1.0 host these calls are ignored and
`hide(false)` is a plain hide): `setSlosh(style, amount)` and `kick(way)`,
`setButtons(buttons, carry)`, `setPointerArea(rect)`, `setBusy(bool)`,
`hide(animate)` (false = Cut), `Spec::travel`/`indicator`, and the `LightMoved`
event (LPARAM = light * 1000; `light()` holds the value). `lghost::wire::*`
builds every payload, so `tests/lgengine_wire_test.cpp` (CMake option
`LGENGINE_BUILD_TESTS`, no host needed) checks the bytes. After a failed first
connect the client posts `Failed` after 3 attempts and stops (retrying a host
blocked by Smart App Control would raise a toast each time); later losses retry
with a 100 ms to 60 s backoff (reset by a CreateAck or a 10 s stable session). A host Error
on Create, SetLook or an extra posts Failed with the error code in LPARAM and keeps the pipe; a rejected
Create is not retried until setSpec changes the spec.
Keep the notification HWND alive until after client destruction. Posted events
may already be queued when destruction returns.

Only draw app-owned text/icons, with transparent pixels wherever glass should
show. Pixels are premultiplied ARGB (`0xAARRGGBB` on little-endian Windows).
The host owns screen capture, glass rendering, glass windows and hover animation.
No frame-by-frame calls from the app are required. No 10 ms latency is promised.

Handle `Connected`, `Lost`, `Restarted`, `Shown`, `Hidden`, `Appearance` and
`Failed` in the selected message. For `Appearance`, LPARAM is true for light
glass: redraw your own text with dark ink. On host loss the client retains
state and automatically attempts reconnection/recreation; use the events for
status. `connected()` means authenticated transport, whereas `Shown` confirms
the host presented the surface. Failure details: `lastError()`/`failureText()`.

`example.cpp` is a complete consumer with no controller dependency. It submits
text only, handles appearance changes, and exits after 30 seconds. Run:

```powershell
./LGEngineExample.exe 'C:\absolute\path\LGEngineHost.exe'
```

`--smoke-test` creates an offscreen surface, waits up to 10 seconds for `Shown`,
prints the host PID, and disconnects. It never terminates a shared host.

This API creates app-owned surfaces. Replacing taskbar/start/OSD contents also
requires system content, positioning, input and accessibility adapters; calling
this library alone does not implement them.
