param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine-native'), [switch]$Validate, [switch]$UpdateDesktop)
$ErrorActionPreference = 'Stop'
$out = [IO.Path]::GetFullPath($OutputDirectory)
& (Join-Path $PSScriptRoot 'build-native.ps1') -OutputDirectory (Join-Path $out 'lgengine')
New-Item -ItemType Directory -Force -Path (Join-Path $out 'tmp') | Out-Null
$taskPreviousTmp = $env:TMP
$taskPreviousTemp = $env:TEMP
try {
    $env:TMP = Join-Path $out 'tmp'
    $env:TEMP = $env:TMP
    & (Join-Path $PSScriptRoot '../../build.ps1') -BuildTests -OutputDirectory $out
    & (Join-Path $PSScriptRoot 'build-bridge-test.ps1') -OutputDirectory $out
    & (Join-Path $PSScriptRoot 'build-client-sdk.ps1') -CandidateDirectory $out
} finally { $env:TMP = $taskPreviousTmp; $env:TEMP = $taskPreviousTemp }
if ($Validate) { & (Join-Path $PSScriptRoot 'validate.ps1') -CandidateDirectory $out -Integration }
if ($UpdateDesktop) { & (Join-Path $PSScriptRoot '../../update-desktop.ps1') -SourceDirectory $out }
