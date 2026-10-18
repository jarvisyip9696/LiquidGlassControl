param([string]$CandidateDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine'), [switch]$Integration)
$ErrorActionPreference = 'Stop'
$candidate = (Resolve-Path -LiteralPath $CandidateDirectory).Path
$checks = [Collections.Generic.List[object]]::new()
function Invoke-Check([string]$Name, [string]$Executable, [string]$Report) {
    $started = [DateTime]::UtcNow
    try {
        $process = Start-Process -FilePath $Executable -ArgumentList '--self-test' -WindowStyle Hidden -PassThru
        if (-not $process.WaitForExit(30000)) {
            $process.Kill()
            $process.WaitForExit()
            $checks.Add([pscustomobject]@{name=$Name; status='timed_out'; startedUtc=$started.ToString('o')})
            return
        }
        $process.Refresh()
    } catch {
        $checks.Add([pscustomobject]@{ name=$Name; status='not_started'; error=$_.Exception.Message; startedUtc=$started.ToString('o') })
        return
    }
    $text = $null
    # Never present the result of an older build as the current run's evidence.
    if ($Report -and (Test-Path -LiteralPath $Report)) {
        $item = Get-Item -LiteralPath $Report
        if ($item.LastWriteTimeUtc -ge $started) { $text = [IO.File]::ReadAllText($Report) }
    }
    $checks.Add([pscustomobject]@{ name=$Name; status=$(if ($process.ExitCode -eq 0) {'passed'} else {'failed'}); exitCode=$process.ExitCode; report=$text; startedUtc=$started.ToString('o') })
}
Invoke-Check 'native' (Join-Path $candidate 'LiquidGlassTests.exe') (Join-Path $candidate 'self-test.txt')
Invoke-Check 'adapter' (Join-Path $candidate 'lgengine/LiquidGlass.LGPreview.exe') ''
if (Test-Path -LiteralPath (Join-Path $candidate 'lgengine/LGEngineExample.exe')) {
    $started = [DateTime]::UtcNow
    try {
        & (Join-Path $PSScriptRoot 'test-client-sdk.ps1') -CandidateDirectory $candidate
        $checks.Add([pscustomobject]@{name='external_cpp_consumers'; status='passed'; startedUtc=$started.ToString('o'); report=[IO.File]::ReadAllText((Join-Path $candidate 'sdk-test.txt'))})
    } catch {
        $checks.Add([pscustomobject]@{name='external_cpp_consumers'; status='failed'; startedUtc=$started.ToString('o'); error=$_.Exception.Message})
    }
}
if ($Integration) {
    $exe = Join-Path $candidate 'LGEngineBridgeTests.exe'
    $started = [DateTime]::UtcNow
    try {
        $stdout = Join-Path $candidate 'bridge-test.txt'
        $stderr = Join-Path $candidate 'bridge-test-errors.txt'
        $process = Start-Process -FilePath $exe -WindowStyle Hidden -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if (-not $process.WaitForExit(30000)) {
            $process.Kill(); $process.WaitForExit()
            $checks.Add([pscustomobject]@{name='bridge'; status='timed_out'; startedUtc=$started.ToString('o')})
        } else {
            $process.Refresh()
            $checks.Add([pscustomobject]@{name='bridge'; status=$(if($process.ExitCode -eq 0){'passed'}else{'failed'}); exitCode=$process.ExitCode; report=[IO.File]::ReadAllText($stdout); startedUtc=$started.ToString('o')})
        }
    } catch {
        $checks.Add([pscustomobject]@{name='bridge'; status='not_started'; error=$_.Exception.Message; startedUtc=$started.ToString('o')})
    }
}
$artifacts = @(Get-ChildItem -LiteralPath $candidate -File | Where-Object Extension -in '.exe','.dll') +
    @(Get-ChildItem -LiteralPath (Join-Path $candidate 'lgengine') -File | Where-Object Extension -in '.exe','.dll')
$hashes = @($artifacts | ForEach-Object { [pscustomobject]@{name=$_.FullName; sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash} })
$result = [pscustomobject]@{ createdUtc=[DateTime]::UtcNow.ToString('o'); checks=@($checks.ToArray()); artifacts=$hashes }
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $candidate 'validation.json') -Encoding utf8
$checks | Format-Table name,status,exitCode -AutoSize
if (@($checks | Where-Object status -ne 'passed').Count) { throw 'Validation incomplete or failed. See validation.json; security policy was not changed.' }
