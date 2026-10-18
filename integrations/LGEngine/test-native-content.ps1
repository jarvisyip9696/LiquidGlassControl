param([string]$CandidateDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine-native'), [string]$Compiler)
$ErrorActionPreference = 'Stop'
$candidate = [IO.Path]::GetFullPath($CandidateDirectory)
if (-not $Compiler) {
    $found = Get-Command clang++.exe -ErrorAction SilentlyContinue
    $Compiler = if ($found) { $found.Source } else { 'C:/Program Files/Windhawk/Compiler/bin/clang++.exe' }
}
$tmp = Join-Path $candidate 'content-probe-tmp'
New-Item -ItemType Directory -Force -Path $tmp | Out-Null
$resource = Join-Path $tmp 'manifest.res'
$manifest = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot '../../tests/lgengine_xaml_content_probe.manifest'))
$writer = [IO.BinaryWriter]::new([IO.File]::Create($resource))
try {
    foreach ($record in @(0,1)) {
        $writer.Write([uint32]$(if ($record) { $manifest.Length } else { 0 }))
        $writer.Write([uint32]32); $writer.Write([uint16]65535); $writer.Write([uint16]$(if ($record) { 24 } else { 0 }))
        $writer.Write([uint16]65535); $writer.Write([uint16]$record)
        $writer.Write([uint32]0); $writer.Write([uint16]0); $writer.Write([uint16]0); $writer.Write([uint32]0); $writer.Write([uint32]0)
        if ($record) { $writer.Write($manifest); while ($writer.BaseStream.Position % 4) { $writer.Write([byte]0) } }
    }
} finally { $writer.Dispose() }
$exe = Join-Path $candidate 'LGEngineContentProbe.exe'
$previousTmp = $env:TMP; $previousTemp = $env:TEMP
try {
    $env:TMP = $tmp; $env:TEMP = $tmp
    # This test uses only system XAML, so use the toolchain's consistent Windows
    # projections rather than mixing them with the generated WinUI header cache.
    & $Compiler --target=x86_64-w64-mingw32 -std=c++20 -Os -Wall -Wextra -static -municode (Join-Path $PSScriptRoot '../../tests/lgengine_xaml_content_probe.cpp') $resource -o $exe -lole32 -loleaut32 -lruntimeobject -luuid -luser32 -lbcrypt -ladvapi32
    if ($LASTEXITCODE -ne 0) { throw 'Native content probe compilation failed.' }
} finally { $env:TMP = $previousTmp; $env:TEMP = $previousTemp }
$line = Get-Content -LiteralPath (Join-Path $candidate 'lgengine/host-current.ini') | Where-Object { $_ -match '^Executable=(LGEngineHost-[a-f0-9]{64}\.exe)$' } | Select-Object -First 1
if (-not $line) { throw 'Invalid host pointer.' }
$hostFile = Join-Path $candidate ('lgengine/' + $line.Substring(11))
$report = Join-Path $candidate 'content-probe.txt'
$result = [ordered]@{startedUtc=[DateTime]::UtcNow.ToString('o'); status='not_started'; exeSha256=(Get-FileHash -LiteralPath $exe).Hash; scope='Self-created offscreen Windows XAML island; no system windows modified'}
try {
    $process = Start-Process -FilePath $exe -ArgumentList ('"' + $hostFile + '"') -WindowStyle Hidden -PassThru -RedirectStandardOutput $report -RedirectStandardError (Join-Path $candidate 'content-probe-errors.txt')
    if (-not $process.WaitForExit(30000)) { $process.Kill(); $process.WaitForExit(); $result.status='timed_out'; throw 'Native content probe timed out.' }
    $process.Refresh(); $result.exitCode=$process.ExitCode
    $result.status=if ($process.ExitCode -eq 0) { 'passed' } else { 'failed' }
    $result.report=[IO.File]::ReadAllText($report);Write-Output $result.report
    if ($process.ExitCode -ne 0) { throw "Native content probe failed: $($process.ExitCode)" }
} catch { $result.error=$_.Exception.Message; throw }
finally { $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $candidate 'content-probe.json') -Encoding utf8 }
