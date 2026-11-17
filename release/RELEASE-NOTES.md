# Liquid Glass Control 0.14.2 — Chinese and English

- Keep English, Simplified Chinese and Traditional Chinese; remove Polish and Vietnamese.
- First launch continues to follow the Windows display language. Switching languages saves the choice automatically.
- Existing Polish and Vietnamese preferences fall back to English without changing other settings.
- Preserve Traditional Chinese conversion, fonts and legacy Chinese language codes.

Download and extract LiquidGlassControl-0.14.2-win11-x64.zip on Windows 11 x64. Keep the included art, fonts and LGEngine runtime beside the controller. Corresponding source, licenses and checksums accompany the downloads.

Validation: localization tests cover all three labels, saved preference round trips, legacy preferences and first-launch fallback. The release workflow builds the tagged source and runs the isolated controller self-test before packaging.

No new live shell or hardware validation was performed. Binaries remain unsigned. Restore managed effects and exit the previous version before upgrading into a separate folder; retain existing settings and recovery records.
