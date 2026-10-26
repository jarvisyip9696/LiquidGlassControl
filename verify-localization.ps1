$ErrorActionPreference = 'Stop'
# Check translation coverage without launching the controller or touching settings.
$literal = '((?:[^"\\]|\\.)*)'
$version = [regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'src/app_version.h')), '(?m)^#define LG_VERSION "([^"]+)"').Groups[1].Value
function Expand-UiLiterals([string]$text) { return ($text.Replace('LG_VERSION_W', 'L"' + $version + '"') -replace '"\s+L"', '') }
$catalog = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'src/localization.h'))
$catalog = Expand-UiLiterals $catalog
$entries = [regex]::Matches($catalog, '\{L"' + $literal + '",L"' + $literal + '",L"' + $literal + '"\}')
$translations = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::Ordinal)
foreach ($entry in $entries) {
    $english = $entry.Groups[1].Value
    if ($translations.ContainsKey($english)) { throw "Duplicate translation key: $english" }
    if (-not $entry.Groups[2].Value -or -not $entry.Groups[3].Value) { throw "Empty translation: $english" }
    $translations.Add($english, $entry.Groups[2].Value)
}
$required = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($name in @('src/main.cpp', 'src/control_ui.h', 'src/ui_style.h', 'src/app_glass_ui.h', 'src/workspace_pages.h', 'src/glass_features.h')) {
    $source = [IO.File]::ReadAllText((Join-Path $PSScriptRoot $name))
    $source = Expand-UiLiterals $source
    $patterns = @(
        ('\btr\(L"' + $literal + '",\s*L"' + $literal + '"\)'),
        ('\{\w+,L"' + $literal + '",\s*L"' + $literal + '"\}')
    )
    foreach ($pattern in $patterns) {
        foreach ($match in [regex]::Matches($source, $pattern)) {
            if ($match.Groups[1].Value -ne $match.Groups[2].Value) { [void]$required.Add($match.Groups[2].Value) }
        }
    }
    foreach ($array in [regex]::Matches($source, 'const wchar_t\*en\[\]=\{([^}]+)\}')) {
        foreach ($match in [regex]::Matches($array.Groups[1].Value, 'L"' + $literal + '"')) { [void]$required.Add($match.Groups[1].Value) }
    }
}
$missing = @($required | Where-Object { -not $translations.ContainsKey($_) } | Sort-Object)
if ($missing.Count) { throw ('Missing Polish/Vietnamese translations: ' + ($missing -join '; ')) }
Write-Output "PASS: $($entries.Count) translation entries; $($required.Count) UI keys covered; no empty or duplicate entries."
