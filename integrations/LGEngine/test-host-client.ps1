param([string]$CandidateDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine-native'))
$ErrorActionPreference = 'Stop'
$candidate = [IO.Path]::GetFullPath($CandidateDirectory)
$compiler = (Get-Command clang++.exe -ErrorAction SilentlyContinue).Source
if (-not $compiler) { $compiler = 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe' }
$exe = Join-Path $candidate 'LGEngineHostClientTests.exe'
$taskTmp = $env:TMP
$taskTemp = $env:TEMP
try {
    $env:TMP = Join-Path $candidate 'tmp'
    $env:TEMP = $env:TMP
    New-Item -ItemType Directory -Force -Path $env:TMP | Out-Null
    & $compiler --target=x86_64-w64-mingw32 -std=c++17 -O2 -Wall -Wextra -static (Join-Path $PSScriptRoot '../../tests/lgengine_host_client_test.cpp') -o $exe -luser32 -ladvapi32 -lbcrypt
    if ($LASTEXITCODE -ne 0) { throw 'C++ host client test build failed.' }
} finally { $env:TMP = $taskTmp; $env:TEMP = $taskTemp }
$hostFile = Join-Path $candidate 'lgengine/LGEngineHost.exe'
$hostPointer = Join-Path $candidate 'lgengine/host-current.ini'
if (Test-Path -LiteralPath $hostPointer) {
    $hostLine = Get-Content -LiteralPath $hostPointer | Where-Object { $_ -match '^Executable=(LGEngineHost-[a-f0-9]{64}\.exe)$' } | Select-Object -First 1
    if (-not $hostLine) { throw 'Invalid host pointer.' }
    $hostFile = Join-Path $candidate ('lgengine/' + $hostLine.Substring(11))
}
$test = Start-Process -FilePath $exe -ArgumentList ('"' + $hostFile + '"') -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $candidate 'host-client-test.txt')
if (-not $test.WaitForExit(60000)) { $test.Kill(); $test.WaitForExit(); throw 'Host client tests timed out.' }
Get-Content (Join-Path $candidate 'host-client-test.txt')
if ($test.ExitCode -ne 0) { throw "Host client tests failed: $($test.ExitCode)" }
