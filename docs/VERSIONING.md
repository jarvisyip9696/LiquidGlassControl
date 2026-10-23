# Version policy

`src/app_version.h` is the single version source for captions, About, packaging and
the release-tag check. Existing published tags are never renamed or replaced.

The compact application starts at **0.12.0-beta.1**. A minor increment identifies
feature or distribution changes; a patch increment identifies compatible fixes.
Incomplete validation uses `-beta.N`, incremented for each published candidate.
Publish `0.12.0` only after the live shell and restoration checks pass. Reserve
`1.0.0` for a documented, validated stable feature set.

The user download contains the controller with its embedded engine. Self-tests
are built as `LiquidGlassTests.exe` and are not included in the portable download.
Build both with `./build.ps1 -BuildTests`; run `LiquidGlassTests.exe --self-test`.
The complete source archive includes tests, licenses and third-party notices.

0.13.0-beta.1 adds the integrated LGEngine page and full portable runtime. Its
ZIP includes the adapter and pinned official host; a separate controller EXE does
not include those companions. Both controller and host source archives accompany
the prerelease, with the recorded host distribution authorization.

0.13.0-beta.2 fixes the LGEngine preview: the card no longer floats over other
windows, settings are written only when changed, and the preview recovers after a
host failure. Its text and icons are antialiased. LGEngine (source and host) is
MIT-licensed, and its license text ships next to the host.

0.13.0-beta.3 bundles the LGEngine host built from upstream `047aa55` (LGEngine
2.7.13.0), which keeps the volume and brightness indicator above maximized
windows. The controller code is unchanged.

0.13.0-beta.4 fixes the controller: App glass Restore reverts only App glass, the
background indicator switch works again, a host blocked by Smart App Control is
reported once (no retry toasts) instead of showing as connected, and the status
line stays on the page it belongs to and names the acrylic fallback.

0.13.0-beta.5 fixes the Explorer engine subscription: a failed subscription is
retried with backoff and shown on the affected items instead of repeating
silently, property writes recover from a stale index, and a slow Start or
notification host restart is re-attached. The controller tears down only when
Windows really ends the session, and the LGEngine preview gives Windows' own
volume pop-up back when the host refuses the lens.

0.13.0-beta.6 bundles the LGEngine 2.7.14.0 host, which is per-monitor DPI
aware: above 100% display scaling the volume and brightness glass was drawn
off-screen. The shell engine DLL is now extracted to
`%LOCALAPPDATA%\LiquidGlassControl\Engine` instead of the app folder, because
Explorer keeps it loaded until it restarts and that locked the folder; older
copies are deleted once nothing holds them.

0.13.0-beta.7 keeps the LGEngine 2.7.14.0 host. Fast brightness or volume
steps redraw the glass once per frame instead of once per step, the glass
hides before sleep and when the display turns off, and the LGEngine sample
card is placed outside the settings window. App glass gets two pages: Windows
backdrop (now saved and reapplied at startup, with border and title colours)
and Title-bar glass, an engine glass strip over the title bar of the chosen
apps with its own look.

0.13.0 is the first full release of the 0.13 line (staged as beta.8). Its
release page carries only the portable ZIP; the ZIP still includes both source
archives and the license texts, and GitHub adds the tagged source. It bundles the
LGEngine 2.7.15.0 host, whose glass edges are smoother. The settings window is simpler: three pages without sub-tabs, and
each System glass effect is one row with a switch and its own settings behind
an arrow. The volume and brightness glass moves into the Volume and Brightness
switches, the notification center and calendar glass work on their own, and
Title-bar glass and App glass hold up better.

0.13.1 fixes 0.13.0. Background blur now shows what is behind the surface
instead of a flat tint, the taskbar no longer rebuilds itself over and over,
File Explorer glass lasts twice as long and the log says when it stops, and
glass resumes as soon as an old theme has been restored. Old app folders can be
deleted again: the engine host now runs from its own copy under
%LOCALAPPDATA%\LGEngine\host instead of from the app folder. The volume and
brightness glass hands over cleanly when you switch between them and follows
the pointer without lag. It bundles the LGEngine 2.7.16.0 host, whose lens
reads the screen at full resolution.

0.13.2 fixes the taskbar capsule. Start and the Search box now move with the
app icons instead of staying put underneath them. The capsule used to skip any
wide taskbar button so that Widgets would stay out, and the Search box is just
as wide, so it now tells Widgets apart by its id.

0.13.3 fixes the taskbar capsule again. 0.13.2 still left Start and the Search
box behind on some PCs, because the capsule's search through the taskbar
missed them. It now takes them from the same row as the app icons and leaves
out only Widgets, by its id. The engine log gets a "capsule row" line that
lists what that row holds.
