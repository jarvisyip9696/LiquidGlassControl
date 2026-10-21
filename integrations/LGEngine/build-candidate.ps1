param(
    [string]$SourceDirectory = (Join-Path $PSScriptRoot '../../build/LGEngine-upstream'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine'),
    [switch]$Validate
)
$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$out = [IO.Path]::GetFullPath($OutputDirectory)
$expected = '4e24ca4e55e6b8c6b119a1b2325002a139deff63'
$revision = & git -C $source rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $revision -ne $expected) { throw "Expected LGEngine commit $expected. Refusing to patch another revision." }
$patch = Join-Path $PSScriptRoot 'fixes.patch'
& git -C $source apply --reverse --check $patch 2>$null
if ($LASTEXITCODE -ne 0) {
    & git -C $source apply --check $patch
    if ($LASTEXITCODE -ne 0) { throw 'Cannot apply LGEngine patch without conflicts. Existing edits were preserved.' }
    & git -C $source apply $patch
    if ($LASTEXITCODE -ne 0) { throw 'LGEngine patch failed.' }
}
& (Join-Path $PSScriptRoot 'build-adapter.ps1') -SourceDirectory $source -OutputDirectory (Join-Path $out 'lgengine')
& (Join-Path $PSScriptRoot 'test.ps1') -SourceDirectory $source -BuildOnly
& (Join-Path $root 'verify-localization.ps1')
$taskOldTmp = $env:TMP
$taskOldTemp = $env:TEMP
$taskTemp = Join-Path $out 'tmp'
New-Item -ItemType Directory -Force -Path $taskTemp | Out-Null
try {
    # Keep compiler intermediates in the candidate's writable workspace.
    $env:TMP = $taskTemp
    $env:TEMP = $taskTemp
    & (Join-Path $root 'build.ps1') -BuildTests -OutputDirectory $out
    & (Join-Path $PSScriptRoot 'build-bridge-test.ps1') -OutputDirectory $out
} finally {
    $env:TMP = $taskOldTmp
    $env:TEMP = $taskOldTemp
}
if ($Validate) {
    & (Join-Path $PSScriptRoot 'validate.ps1') -CandidateDirectory $out
}
