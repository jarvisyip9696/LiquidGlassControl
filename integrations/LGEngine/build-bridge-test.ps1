param([string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine'), [string]$Compiler)
$ErrorActionPreference = 'Stop'
if (-not $Compiler) {
    $found = Get-Command clang++.exe -ErrorAction SilentlyContinue
    if ($found) { $Compiler = $found.Source }
    elseif (Test-Path -LiteralPath 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe') { $Compiler = 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe' }
    else { throw 'A MinGW clang++ compiler is required for the bridge integration test.' }
}
$out = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $out | Out-Null
$exe = Join-Path $out 'LGEngineBridgeTests.exe'
$source = Join-Path $PSScriptRoot '../../tests/lgengine_bridge_test.cpp'
$object = Join-Path $out 'lgengine-bridge-test.o'
& $Compiler --target=x86_64-w64-mingw32 -std=c++17 -Wall -Wextra -Os -c $source -o $object
if ($LASTEXITCODE -ne 0) { throw 'Bridge integration test compilation failed.' }
& $Compiler --target=x86_64-w64-mingw32 -static $object -o $exe -luser32
if ($LASTEXITCODE -ne 0) { throw 'Bridge integration test linking failed.' }
Write-Output "Built protocol integration test: $exe"
