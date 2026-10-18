# Notification center / calendar investigation

Local investigation on 2026-10-17. No notification bodies were collected.

## Observed

- Initial controller logs matched `ControlCenterRegion` in `ShellHost.exe` PID 15468, but did not match `NotificationCenterGrid` or `CalendarCenterGrid`.
- The shell host subsequently changed to PID 15596 in the same user session. Its module inventory contained Windows XAML, but no project engine DLL. The controller was still running with Notifications enabled (`Features=16`).
- Hook attempts returned Win32 error 87. This does not, by itself, identify why Windows refused those individual hook requests.
- The user confirmed opening notification center/calendar. The read-only window inventory still did not expose a visible target panel; the log contains no proof that the requested panel was styled.

## Implemented safeguards

- A newly observed target window can retry a previously attempted thread; failed probes no longer permanently exclude a later UI queue.
- Quick Settings, toast and auxiliary surfaces no longer acknowledge the notification/calendar root feature as successfully applied.
- Current-session packaged shell hosts are watched using process waits. Exit notifications trigger at most six short discovery passes, with no idle scan loop. Wait registrations and handles are released on disable/shutdown.
- `--background` starts the controller in its tray so reconnecting does not intentionally activate the settings window and dismiss the target flyout.

The current recovery candidate is `build/candidates/notification-recovery`. Its isolated native self-test passed, including engine load, COM, composition and restoration fixtures. This is not a successful live notification styling result.

## Proposed connection probe — not executed

The compiled local probe `build/attach-notification-probe.exe` uses Microsoft's documented [InitializeXamlDiagnosticsEx](https://learn.microsoft.com/en-us/windows/win32/api/xamlom/nf-xamlom-initializexamldiagnosticsex) entry point with the existing engine DLL, current controller PID and explicitly selected shell host PID. The source is `build/attach-notification-probe.cpp`.

Automatic approval review rejected executing this probe against the two live shell processes. The stated concern was high-impact cross-process DLL loading with possible shell disruption, not explicitly authorized in this exact form. The probe has **not** been run against those processes, and no alternate execution route was used.

Before an approved test, refresh the controller and target PIDs, verify their executable identities and session, and keep the target list limited to `ShellHost.exe` and `ShellExperienceHost.exe`. Loading the existing DLL can close/restart or destabilize the panel host. Turning the controller off restores its material overrides; the pinned DLL itself remains until that host exits.

Live repair remains incomplete pending explicit approval for this connection test and subsequent evidence of correct surface writes and visible restoration.

## Explicitly authorized live probe result

The user explicitly authorized the described operation. Rechecked controller PID 14888 and target PIDs 15596/32112, executable paths and session 1 before execution. Both InitializeXamlDiagnosticsEx calls returned S_OK (0x00000000). A subsequent module inventory confirmed the existing engine DLL in ShellHost PID 15596; it did not confirm the DLL in ShellExperienceHost PID 32112. Both hosts remained running. No surface-match log was produced at this point, so API success is not evidence that notification/calendar styling works. Reopening the panel and checking surface writes remains pending.

After the user confirmed reopening, a fresh read-only shell window inventory still reported only a hidden ShellHost CoreWindow (visible=0, bounds 0,0,1,1), with no visible notification/calendar host. No engine surface log was created. The supported Computer Use window inventory returned only the Codex window, so it provided no targetable taskbar or notification panel. No further injection was attempted. Visible panel identity and appearance remain unverified; obtain a screenshot from the user's actual desktop before concluding that a selector or material fix is appropriate.

## User-supplied Styler 1.7 readiness port

Reference: user-pasted Windows 11 Notification Center Styler 1.7 by m417z, GPL-3.0-only; attachment SHA256 73920BBBFB7D1B1D2B69459A69449AE4DBE834B3360D64F4EC2D7DAFDC12AF5E. Compared InjectWindhawkTAP, OnWindowCreated, GetCoreWnds and deferred RunFromWindowThreadViaPostMessage with the standalone path.

- Match ShellExperienceHost CoreWindow and ShellHost ControlCenterWindow separately. Do not treat ShellHost's hidden CoreWindow stub as a ready notification panel.
- Stop arbitrary notification worker-thread bootstrap attempts; discover real host windows initially and on creation/show.
- Deliver the deferred bootstrap with PostMessage to the actual HWND, preserving delivery through window-filtered/modal message loops.
- Allow relevant hidden shell window creation events through the observer; retain visibility/root checks for ordinary application rules.
- Retain the already imported LiquidGlass notification/calendar material selectors and registry subscription compatibility. Windhawk's runtime, telemetry, settings system and unrelated UI/layout changes are not imported.

Candidate: build/candidates/notification-styler. Compile/link completed without warnings. Fresh isolated native self-test exited 0 (engine load, COM, composition S_OK, configuration, feature isolation, restoration and DPI); read-only launch preflight exited 0. Switched from recovery controller using its supported restore/exit command and launched the candidate in background with existing settings. This validates the readiness port, not a visible live styling result. The remaining live check is opening the real notification/calendar panel and observing its root match/material acknowledgement.

## AppContainer access failure and transport repair

Read-only token/access checks identified ShellExperienceHost as AppContainer, low integrity (4096), while the controller and ShellHost were medium integrity (8192). AccessCheck against the original objects denied the notification package all three required accesses: engine DLL read, configuration mapping read, and controller-process SYNCHRONIZE. ShellHost passed those checks. This explains why readiness changes alone could not make the restricted notification host work; an API return of S_OK was insufficient.

Implemented protocol v3:

- Discover the exact package SID from the same-session allowlisted target token. Grant that package read/execute on the extracted engine DLL only, read on the config mapping, and read/write on a separate disposable status mapping. No directory, system-policy or controller-process ACL changes. Only the status page receives a low integrity label; configuration remains read-only for the package.
- Connect restricted notification hosts using the documented InitializeXamlDiagnosticsEx entry point outside the controller UI thread. Keep ordinary ShellHost readiness filtering. A missing diagnostics endpoint retries at most fifteen times at two-second intervals, with no perpetual discovery loop.
- An AppContainer that cannot open the controller-process handle uses the read-only config heartbeat; expiration is five seconds and checked by the host's UI timer. Feature/sequence changes are also observed through this channel, allowing Off/On and slider updates without relying on lower-to-higher integrity window messages.
- The independent status mapping carries connection, subscription and actual material-write results. It cannot modify the controller config. Disabled features are masked out of status acknowledgements.

The first AppContainer candidate produced fresh live evidence against ShellExperienceHost PID 32112: new engine DLL loaded; connected host mask=4; subscribed host mask=4; success=16; fallback=0; failed=0; error=0. This proves a notification root received custom composition material, not merely that injection was requested. It does not establish a screenshot-verified appearance for both individual roots.

A later lifecycle candidate encountered ERROR_NOT_FOUND while the diagnostics endpoint was absent. Its attempted Off/On check had no connected subscription and is NOT counted as a successful lifecycle test (see its live-lifecycle.json). Bounded endpoint retry was then added.

Current candidate: build/candidates/notification-ready. Fresh isolated native self-test and read-only startup preflight exited 0; heartbeat expiration/wraparound and sequence propagation checks are included. Awaiting user-opened notification panel to validate the final candidate's live subscription and Off/On recovery. Earlier live-write evidence is not relabeled as evidence for this final binary.

Microsoft references: https://learn.microsoft.com/en-us/windows/apps/develop/communication/sharing-named-objects and https://learn.microsoft.com/en-us/windows/win32/api/xamlom/nf-xamlom-initializexamldiagnosticsex .

## Live lifecycle result and remaining visual mismatch

The notification-ready candidate later connected: connected/subscribed masks=4, success=16, fallback=0, failed/error=0. Its live-lifecycle.json records subscription 4 -> 0 -> 4 across temporary Off/On, with original features=16 restored. The user nevertheless confirmed the UI says Applied while appearance remains unchanged, so the visual issue is still open.

Compared the supplied Styler's CreateEffectBrush flood/source-over tint with the standalone ColorMatrix. The standalone tint previously preserved backdrop alpha while adding RGB; for transparent backdrop input, this did not produce a correctly composited visible tint. Fixed the matrix to alpha_out = alpha_in * (1-opacity) + opacity, matching source-over of a solid tint. Added a regression that decodes the actual serialized matrix and checks RGBA output over alpha 0/0.5/1, opacity 0/0.35/1 and light/dark tints. The notification-tint candidate's native self-test (including this regression) and startup preflight passed; it has not yet replaced the live candidate.

A temporary live diagnostic changed notification-ready's config mapping from opacity=35/blur=12 to opacity=5/blur=0, without changing persisted settings. Original values are saved in its temporary-probe-original.json; awaiting the user's observed difference before replacing the live build. This temporary diagnostic must be restored or terminated via the controller's supported restore/exit flow.

## Screenshots, root diagnostics and direct property candidate

The user supplied two screenshots showing ordinary dark acrylic in notification center and Quick Settings despite Applied status. The calendar was collapsed in the screenshot. The low-opacity/zero-blur probe did not have a confirmed positive visual result; its original opacity=35 and blur=12 were restored explicitly before proceeding.

notification-visible included the source-over alpha fix and per-root diagnostics. It passed isolated self-test/startup preflight and connected/subscribed with success=16. A sample after the user's reply reported notification dimensions 336x718 and calendar 336x1, both Background=Acrylic. Both samples had IsHostVisible=false, so these measurements alone do not prove what was painted while the flyout was open. Subsequent diagnostics preserve brush type during visible samples to avoid this ambiguity.

Prepared notification-direct following the supplied Styler's DependencyObject.SetValue approach for classic notification Background properties. It saves ReadLocalValue/UnsetValue for restoration, declines binding expressions, writes the actual Background dependency property, and checks GetValue against the requested brush before success. Restore uses SetValue or ClearValue on the same dependency property. Other properties continue on their existing path.

The direct candidate compiled, but its isolated self-test could not load its extracted engine DLL: Win32 4551 (ERROR_SYSTEM_INTEGRITY_POLICY_VIOLATION). Engine COM, composition and configuration checks were skipped for that binary; UI/config/restoration fixture groups passed. No live switch to notification-direct occurred, no security policy was changed, and no alternate load route was attempted. This blocks runtime validation of direct assignment/restoration. The existing notification-visible controller remains running; the user-visible issue is not declared solved. Continue only once the exact candidate is permitted through an appropriate trusted signing/application approval process.

Final runtime recheck found a different externally started instance: PID 8772, build/candidates/pill-radius-final/LiquidGlassControl.exe, with engine filename LiquidGlass.Engine-7089059020155821807.dll. This supersedes the earlier statement that notification-visible remained active. Do not attribute notification-visible's status to this different running build. No replacement/termination of PID 8772 was performed. Coordinate a single intended runtime before further live verification; preserve concurrent workspace edits (including OSD/capsule/start changes).

Blocked notification-direct engine SHA256: 0744C496B9762D11F4148C8B17441F85FCFFC7CD8599FE7F1E77A4B09BFEA642.
