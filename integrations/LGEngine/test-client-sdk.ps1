param([string]$CandidateDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine-native'))
$ErrorActionPreference = 'Stop'
$candidate = [IO.Path]::GetFullPath($CandidateDirectory)
$out = Join-Path $candidate 'lgengine'
$hostLine = Get-Content -LiteralPath (Join-Path $out 'host-current.ini') | Where-Object { $_ -match '^Executable=(LGEngineHost-[a-f0-9]{64}\.exe)$' } | Select-Object -First 1
if (-not $hostLine) { throw 'Invalid host pointer.' }
$hostFile = Join-Path $out $hostLine.Substring(11)
$runs = @()
try {
    foreach ($i in 1..2) {
        $log = Join-Path $candidate "sdk-consumer-$i.txt"
        $process = Start-Process -FilePath (Join-Path $out 'LGEngineExample.exe') -ArgumentList ('"' + $hostFile + '" --smoke-test') -WindowStyle Hidden -PassThru -RedirectStandardOutput $log -RedirectStandardError (Join-Path $candidate "sdk-consumer-$i-errors.txt")
        $runs += [pscustomobject]@{process=$process; log=$log}
    }
    $pids = @()
    foreach ($run in $runs) {
        if (-not $run.process.WaitForExit(15000)) { throw 'Independent C++ consumer timed out.' }
        $run.process.Refresh()
        if ($run.process.ExitCode -ne 0) { throw "Consumer failed: $($run.log)" }
        $match = [regex]::Match([IO.File]::ReadAllText($run.log), 'SHOWN host=(\d+)')
        if (-not $match.Success) { throw 'No presentation confirmation.' }
        $pids += $match.Groups[1].Value
    }
    if ($pids[0] -eq '0' -or $pids[0] -ne $pids[1]) { throw 'Consumers did not share the same host.' }
    "PASS: two independent C++ consumers presented through shared host PID $($pids[0]); both disconnected normally." | Set-Content -LiteralPath (Join-Path $candidate 'sdk-test.txt')
    Get-Content -LiteralPath (Join-Path $candidate 'sdk-test.txt')
} finally {
    # Only these bounded test consumers may be stopped. Never stop the host.
    foreach ($run in $runs) { if (-not $run.process.HasExited) { $run.process.Kill(); $run.process.WaitForExit() } }
}
