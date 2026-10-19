param([switch]$SkipBuild, [string]$ExecutablePath, [string]$OutputDirectory, [string]$TestExecutablePath, [switch]$IncludeLGEngine)
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
# Print the report before judging it, so a failing check is named in the log (and as an annotation on GitHub Actions).
$report = Join-Path $stage 'self-test.txt'
if (Test-Path -LiteralPath $report) { foreach ($line in Get-Content -LiteralPath $report) { if ($env:GITHUB_ACTIONS -and $line.StartsWith('FAIL')) { "::error title=Release self-test::$line" } else { $line } } }
if ($test.ExitCode -ne 0) { throw "Release self-test failed: $($test.ExitCode)" }
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\Quick Start.txt') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\Create Desktop Shortcut.cmd') -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release\Third Party Notices.txt') -Destination $stage
foreach ($notice in @('LICENSING.md', 'CODE-SIGNING.md')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot ('docs/' + $notice)) -Destination $stage
}
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE') -Destination $stage
# Corresponding source accompanies the GPL engine, with no local settings/logs.
$sourceFiles = @('CMakeLists.txt','build.ps1','prepare-winrt.ps1','package.ps1','verify-localization.ps1','update-desktop.ps1','compact-workspace.py','README.md','CONTRIBUTING.md') | ForEach-Object { Join-Path $PSScriptRoot $_ }
$sourceFiles += @('src','docs','assets','third_party','release','tests','integrations','.github') | ForEach-Object { Join-Path $PSScriptRoot $_ }
$sourceFiles += Join-Path $PSScriptRoot 'LICENSE'
$sourceZip = Join-Path $stage 'LiquidGlass-engine-source.zip'
Compress-Archive -LiteralPath $sourceFiles -DestinationPath $sourceZip -CompressionLevel Optimal
# Explicit file list prevents shipping settings, test reports, personal data or recovery journals.
$files = @($exe, (Join-Path $stage 'Quick Start.txt'), (Join-Path $stage 'Create Desktop Shortcut.cmd'), (Join-Path $stage 'Third Party Notices.txt'), $sourceZip)
$files += @('LICENSE', 'LICENSING.md', 'CODE-SIGNING.md') | ForEach-Object { Join-Path $stage $_ }
$extraAssets = @()
if ($IncludeLGEngine) {
    $runtime = Join-Path (Split-Path -Parent $source) 'lgengine'
    $runtimeStage = Join-Path $stage 'lgengine'
    New-Item -ItemType Directory -Path $runtimeStage -Force | Out-Null
    $pointer = Join-Path $runtime 'host-current.ini'
    $hostLine = Get-Content -LiteralPath $pointer | Where-Object { $_ -match '^Executable=(LGEngineHost-[a-f0-9]{64}\.exe)$' } | Select-Object -First 1
    if (-not $hostLine) { throw 'Missing or invalid LGEngine host pointer.' }
    $hostName = $hostLine.Substring(11)
    foreach ($runtimeFile in @('LiquidGlass.LGPreview.exe', 'LGEngineExample.exe', 'host-current.ini', $hostName)) {
        Copy-Item -LiteralPath (Join-Path $runtime $runtimeFile) -Destination $runtimeStage
    }
    if ((Get-FileHash -LiteralPath (Join-Path $runtimeStage $hostName)).Hash.ToLowerInvariant() -ne $hostName.Substring(13,64)) { throw 'Packaged LGEngine host hash mismatch.' }
    $files += $runtimeStage
    $files += Join-Path $PSScriptRoot 'release/LGEngine Distribution.txt'
    $upstream = Join-Path $PSScriptRoot 'build/LGEngine-2.7.12'
    & (Join-Path $PSScriptRoot 'integrations/LGEngine/prepare-upstream.ps1') -SourceDirectory $upstream
    # LGEngine is MIT. The pinned revision predates its LICENSE file, so the same text ships from release\ until the pin has one.
    $pinnedLicense = [bool](& git -C $upstream ls-tree --name-only HEAD LICENSE)
    $engineLicense = if ($pinnedLicense) { Join-Path $upstream 'LICENSE' } else { Join-Path $PSScriptRoot 'release/LGEngine LICENSE.txt' }
    Copy-Item -LiteralPath $engineLicense -Destination (Join-Path $runtimeStage 'LICENSE-LGEngine.txt')
    $engineSourceZip = Join-Path $dist 'LGEngine-2.7.12.0-source.zip'
    & git -C $upstream archive --format=zip --output=$engineSourceZip HEAD
    if ($LASTEXITCODE -ne 0) { throw 'LGEngine source archive failed.' }
    if (-not $pinnedLicense) {
        Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
        $archive = [IO.Compression.ZipFile]::Open($engineSourceZip, [IO.Compression.ZipArchiveMode]::Update)
        try { [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $engineLicense, 'LICENSE') } finally { $archive.Dispose() }
    }
    $files += $engineSourceZip
    $extraAssets += $engineSourceZip
}
$publicSource = Join-Path $dist ("LiquidGlassControl-$version-source.zip")
Copy-Item -LiteralPath $sourceZip -Destination $publicSource -Force
$extraAssets += $publicSource
$zip = Join-Path $dist ($name + '.zip')
Compress-Archive -LiteralPath $files -DestinationPath $zip -CompressionLevel Optimal -Force
$standalone = Join-Path $dist ($name + '.exe')
Copy-Item -LiteralPath $exe -Destination $standalone -Force
$hashes = foreach ($file in @($zip, $standalone) + $extraAssets) {
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
