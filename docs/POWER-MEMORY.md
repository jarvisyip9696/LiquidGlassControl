# Power and memory changes in 0.12.0-beta.3

This version keeps the beta.2 material backend and appearance.

- A cold tray startup with all effects off does not load/extract the engine.
  The first enabled system effect initializes it and keeps normal error handling.
- The controller's one-second timer runs only while effective system features
  are enabled. Publishing settings refreshes the heartbeat before re-enabling
  hosts. Owner-exit observation and active-effect recovery remain intact.
- The application window list and copied icon handles are not built while hidden.
  Hiding the window clears the list, releases vector storage and destroys copied
  icons. Reopening reconstructs it and attempts to retain the selected window.
- Polling replays status only when success/fallback/failure changes, when an error
  changes, or after a deliberate publish/discovery. Unchanged status is suppressed.

## Local comparison, 2026-10-17

Windows 11 x64, local MinGW builds, beta.2 before and beta.3 after; separate fresh
settings, all effects disabled, `--background`. Each was warmed for three seconds,
then sampled eight times, two seconds apart. Runs were sequential on the same
desktop. No forced working-set trimming or memory compression was used.

| Metric | beta.2 | beta.3 |
| --- | ---: | ---: |
| Mean private bytes | 3,461,120 | 2,704,384 |
| Mean working-set bytes | 19,566,592 | 16,437,248 |
| Mean process handles | 198 | 174 |
| Process CPU during approximately 16 seconds | 0 ms reported | 0 ms reported |

Private bytes were approximately 21.9% lower and working set approximately 16.0%
lower in this single cold, effects-off comparison. These are process measurements,
not folder size or a guaranteed reduction in every configuration. Active effects,
Explorer's injected modules, GPU memory and multi-monitor scenarios were not
benchmarked here. CI/MSVC release binaries are separate builds from these samples.

The disabled-effects path removes one periodic controller timer. No hardware
energy/wattage measurement was made. Both CPU samples were below the timer's
reported resolution; they do not establish a measured CPU or energy percentage.

Regression coverage includes idle loading, copied-icon/list cleanup, status
transitions and retries, engine loading/COM/Composition/configuration, slider reset,
five languages, DPI layouts and restoration fixtures. Self-test mode does not
apply materials to the live desktop.
