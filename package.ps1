param([switch]$SkipBuild, [string]$ExecutablePath, [string]$OutputDirectory, [string]$TestExecutablePath)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$version = [regex]::Match([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'src/app_version.h')), '(?m)^#define LG_VERSION "([^"]+)"').Groups[1].Value
if (-not $version) { throw 'Version missing from app_version.h' }
$name = "LiquidGlassControl-$version-win11-x64"
$dist = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $PSScriptRoot 'dist' }
$stage = Join-Path $PSScriptRoot ('build\package-' + [guid]::NewGuid().ToString('N'))
if ($ExecutablePath) {
    $source = (Resolve-Path -LiteralPath $ExecutablePath).Path
} elseif (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot 'build.ps1') -OutputName LiquidGlassControl.release.exe -BuildTests
    $source = Join-Path $PSScriptRoot 'build\LiquidGlassControl.release.exe'
} else {
    $source = Join-Path $PSScriptRoot 'build\LiquidGlassControl.exe'
}
if (-not (Test-Path -LiteralPath $source)) { throw 'Build the application first.' }
New-Item -ItemType Directory -Path $stage -Force | Out-Null
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$exe = Join-Path $stage 'LiquidGlassControl.exe'
Copy-Item -LiteralPath $source -Destination $exe
# Use a clean folder: the test must not consume the developer's settings or backups.
$testSource = if ($TestExecutablePath) { $TestExecutablePath } else { Join-Path (Split-Path -Parent $source) 'LiquidGlassTests.exe' }
if (-not (Test-Path -LiteralPath $testSource)) { throw 'Build LiquidGlassTests.exe before packaging.' }
$testExe = Join-Path $stage 'LiquidGlassTests.exe'
Copy-Item -LiteralPath $testSource -Destination $testExe
$test = Start-Process -FilePath $testExe -ArgumentList '--self-test' -WindowStyle Hidden -PassThru
if (-not $test.WaitForExit(20000)) { throw 'Release self-test timed out.' }
$test.Refresh()
if ($test.ExitCode -ne 0) { throw "Release self-test failed: $($test.ExitCode)" }
Get-Content -LiteralPath (Join-Path $stage 'self-test.txt')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\Quick Start.txt') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\Create Desktop Shortcut.cmd') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\Third Party Notices.txt') -Destination $stage
foreach ($notice in @('LICENSING.md', 'CODE-SIGNING.md')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('docs/' + $notice)) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE') -Destination $stage
# Corresponding source accompanies the GPL engine, with no local settings/logs.
$sourceFiles = @(Get-ChildItem -LiteralPath $PSScriptRoot -File | Where-Object { $_.Extension -in '.cpp','.h','.def','.ps1','.md','.txt' } | ForEach-Object FullName)
$sourceFiles += @((Join-Path $PSScriptRoot 'src'),(Join-Path $PSScriptRoot 'docs'),(Join-Path $PSScriptRoot 'assets'),(Join-Path $PSScriptRoot 'third_party'),(Join-Path $PSScriptRoot 'release'),(Join-Path $PSScriptRoot 'tests'))
$sourceFiles += Join-Path $PSScriptRoot 'LICENSE'
$sourceZip = Join-Path $stage 'LiquidGlass-engine-source.zip'
Compress-Archive -LiteralPath $sourceFiles -DestinationPath $sourceZip -CompressionLevel Optimal
# Explicit file list prevents shipping settings, test reports, personal data or recovery journals.
$files = @($exe, (Join-Path $stage 'Quick Start.txt'), (Join-Path $stage 'Create Desktop Shortcut.cmd'), (Join-Path $stage 'Third Party Notices.txt'), $sourceZip)
$files += @('LICENSE', 'LICENSING.md', 'CODE-SIGNING.md') | ForEach-Object { Join-Path $stage $_ }
$zip = Join-Path $dist ($name + '.zip')
Compress-Archive -LiteralPath $files -DestinationPath $zip -CompressionLevel Optimal -Force
$standalone = Join-Path $dist ($name + '.exe')
Copy-Item -LiteralPath $exe -Destination $standalone -Force
$hashes = foreach ($file in @($zip, $standalone)) {
    $hash = Get-FileHash -LiteralPath $file -Algorithm SHA256
    '{0}  {1}' -f $hash.Hash.ToLowerInvariant(), [IO.Path]::GetFileName($file)
}
$hashes | Set-Content -LiteralPath (Join-Path $dist 'SHA256SUMS.txt') -Encoding ascii
Write-Host "Portable package: $zip"
Write-Host "Standalone EXE: $standalone"

# The archive and standalone output are complete; retain no duplicate staging build.
$resolvedStage = (Resolve-Path -LiteralPath $stage).Path
$buildRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'build')) + [IO.Path]::DirectorySeparatorChar
if (-not $resolvedStage.StartsWith($buildRoot, [StringComparison]::OrdinalIgnoreCase) -or -not (Split-Path -Leaf $resolvedStage).StartsWith('package-')) { throw 'Unexpected package staging path.' }
if (@(Get-ChildItem -LiteralPath $resolvedStage -Recurse -Force -Attributes ReparsePoint).Count) { throw 'Package staging contains a reparse point.' }
Remove-Item -LiteralPath $resolvedStage -Recurse -Force