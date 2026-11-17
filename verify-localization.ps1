$ErrorActionPreference = 'Stop'
# Check the three supported languages without launching the controller or touching settings.
$source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'src/localization.h'))
foreach ($pattern in @(
    'enum Language \{ English, Simplified, Traditional, Count \}',
    'codes\[\] = \{L"en", L"zh", L"zh-Hant"\}',
    'names\[\] = \{L"English", L"简体中文", L"繁體中文"\}',
    'if \(current == Traditional\)'
)) {
    if ($source -notmatch $pattern) { throw "Unexpected bilingual localization configuration: $pattern" }
}
$controls = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'src/control_ui.h'))
$options = [regex]::Matches($controls, '\{l10n::names\[0\], l10n::names\[1\], l10n::names\[2\]\}')
if ($options.Count -ne 2 -or $controls -match 'l10n::names\[[3-9]') {
    throw 'Both language controls must offer only Chinese and English.'
}
Write-Output 'PASS: only Chinese and English are configured in localization and both language controls.'
