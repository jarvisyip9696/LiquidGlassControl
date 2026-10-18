# 0.12.0-beta.4

System material controls now live under System glass > Material, including
background-adaptive tint. The control panel uses shared, demand-driven smooth
animation updates and antialiased compact switches. Application icons are
released when leaving the application page or minimizing. Enabled-effect
controller checks run every two seconds; effects-off mode has no heartbeat timer.
The original small Dock animation is retained.

The source includes an experimental standalone C++ LGEngine protocol client,
consumer example and isolated native-content bridge tests. These do not replace
the taskbar, Start menu or notification center with the original optical host.
The upstream C# host is not included in the release downloads.

The user EXE embeds its system material engine. Download assets contain no
personal settings, recovery journals, compiler caches or test executables.
The portable ZIP includes corresponding source and third-party notices.

Validation: full native application self-test; 12/12 isolated shared-host client
checks; native XAML content, cancellation and restoration probe; translation
coverage and whitespace checks. Live system coverage remains incomplete.
This is an unsigned beta; no measured active-effect power saving is claimed.
