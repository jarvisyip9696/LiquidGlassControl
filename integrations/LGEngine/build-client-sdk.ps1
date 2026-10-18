param([string]$CandidateDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine-native'), [string]$Compiler)
$ErrorActionPreference = 'Stop'
$candidate = [IO.Path]::GetFullPath($CandidateDirectory)
$out = Join-Path $candidate 'lgengine'
New-Item -ItemType Directory -Force -Path $out | Out-Null
if (-not $Compiler) {
    $found = Get-Command clang++.exe -ErrorAction SilentlyContinue
    $Compiler = if ($found) { $found.Source } else { 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe' }
}
$taskTmp = $env:TMP; $taskTemp = $env:TEMP
try {
    $env:TMP = Join-Path $candidate 'tmp'; $env:TEMP = $env:TMP
    New-Item -ItemType Directory -Force -Path $env:TMP | Out-Null
    & $compiler --target=x86_64-w64-mingw32 -std=c++17 -Os -Wall -Wextra -municode -static (Join-Path $PSScriptRoot 'client/example.cpp') -o (Join-Path $out 'LGEngineExample.exe') -luser32 -lgdi32 -lshell32 -ladvapi32 -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'Standalone C++ consumer compilation failed.' }
} finally { $env:TMP = $taskTmp; $env:TEMP = $taskTemp }
Write-Output "Built standalone consumer: $out/LGEngineExample.exe"
