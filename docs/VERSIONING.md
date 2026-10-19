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
