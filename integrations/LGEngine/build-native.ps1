param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine-native/lgengine'), [string]$Compiler)
$ErrorActionPreference = 'Stop'
if (-not $Compiler) {
    $found = Get-Command clang++.exe -ErrorAction SilentlyContinue
    if ($found) { $Compiler = $found.Source }
    elseif (Test-Path -LiteralPath 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe') { $Compiler = 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe' }
    else { throw 'Install MinGW clang++, or use the CMake LiquidGlassLGNative target.' }
}
$out = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out 'LiquidGlass.LGPreview.exe'
# The original standalone host contains the engine; the C++ client does not.
$source = Join-Path $PSScriptRoot '../../build/LGEngine-2.7.16'
& (Join-Path $PSScriptRoot 'prepare-upstream.ps1') -SourceDirectory $source
& (Join-Path $source 'build.ps1') -NoBench
if (-not $?) { throw 'Original LGEngine host build failed.' }
$builtHost = Join-Path $source 'bin/LGEngineHost.exe'
$hostHash = (Get-FileHash -LiteralPath $builtHost).Hash.ToLowerInvariant()
$hostName = "LGEngineHost-$hostHash.exe"
if (-not (Test-Path -LiteralPath (Join-Path $out $hostName))) { Copy-Item -LiteralPath $builtHost -Destination (Join-Path $out $hostName) }
[IO.File]::WriteAllText((Join-Path $out 'host-current.ini'), "[Host]`r`nExecutable=$hostName`r`n", [Text.Encoding]::Unicode)
# Retain the conventional name for older clients on a fresh candidate only.
if (-not (Test-Path -LiteralPath (Join-Path $out 'LGEngineHost.exe'))) { Copy-Item -LiteralPath $builtHost -Destination (Join-Path $out 'LGEngineHost.exe') }
$taskPreviousTmp = $env:TMP
$taskPreviousTemp = $env:TEMP
$taskTemp = Join-Path $out 'tmp'
New-Item -ItemType Directory -Force -Path $taskTemp | Out-Null
try {
    $env:TMP = $taskTemp
    $env:TEMP = $taskTemp
    & $Compiler --target=x86_64-w64-mingw32 -std=c++17 -O2 -Wall -Wextra -municode -mwindows -static (Join-Path $PSScriptRoot 'native/main.cpp') -o $exe -luser32 -lgdi32 -lshell32 -ladvapi32 -lbcrypt -lole32
    if ($LASTEXITCODE -ne 0) { throw 'Native LGEngine port compilation failed.' }
} finally { $env:TMP = $taskPreviousTmp; $env:TEMP = $taskPreviousTemp }
Write-Output "Built C++ optical adapter: $exe"
