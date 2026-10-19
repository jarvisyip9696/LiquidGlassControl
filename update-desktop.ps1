param([string]$SourceDirectory = (Join-Path $PSScriptRoot 'build/candidates/lgengine-native'), [string]$ShortcutPath, [string]$TargetDirectory)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$target = if ($TargetDirectory) { (Resolve-Path -LiteralPath $TargetDirectory).Path } else { Join-Path $PSScriptRoot 'build' }
$hostSource = Join-Path $source 'lgengine/LGEngineHost.exe'
$sourcePointer = Join-Path $source 'lgengine/host-current.ini'
if (Test-Path -LiteralPath $sourcePointer) {
    $hostLine = Get-Content -LiteralPath $sourcePointer | Where-Object { $_ -match '^Executable=(LGEngineHost-[a-f0-9]{64}\.exe)$' } | Select-Object -First 1
    if (-not $hostLine) { throw 'Invalid candidate host pointer.' }
    $hostSource = Join-Path $source ('lgengine/' + $hostLine.Substring(11))
}
$files = @('LiquidGlassControl.exe', 'lgengine/LiquidGlass.LGPreview.exe')
if (Test-Path -LiteralPath (Join-Path $source 'lgengine/LGEngineExample.exe')) { $files += 'lgengine/LGEngineExample.exe' }
foreach ($file in $files) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $file) -PathType Leaf)) { throw "Missing build artifact: $file" }
}
if (-not (Test-Path -LiteralPath $hostSource -PathType Leaf)) { throw 'Candidate host is missing.' }
$desktop = [Environment]::GetFolderPath('Desktop')
if (-not $ShortcutPath) { $ShortcutPath = Join-Path $desktop 'Liquid Glass Control.lnk' }
$shell = New-Object -ComObject WScript.Shell
if (-not (Test-Path -LiteralPath $shortcutPath)) { throw 'Desktop shortcut not found; no shortcut was changed.' }
$shortcut = $shell.CreateShortcut($shortcutPath)
$expected = [IO.Path]::GetFullPath((Join-Path $target 'LiquidGlassControl.exe'))
if (-not [string]::Equals($shortcut.TargetPath, $expected, [StringComparison]::OrdinalIgnoreCase)) { throw "Desktop shortcut points elsewhere: $($shortcut.TargetPath)" }
if ([string]::Equals($source,$target,[StringComparison]::OrdinalIgnoreCase)) { throw 'Choose a candidate directory, not the deployed build directory.' }

# Use each executable's own restore-and-exit path. Never terminate the shared host.
foreach ($exe in @((Join-Path $target 'LiquidGlassControl.exe'))) {
    if (Test-Path -LiteralPath $exe) {
        $closing = Start-Process -FilePath $exe -ArgumentList '--close-running' -WindowStyle Hidden -PassThru
        if (-not $closing.WaitForExit(10000)) { throw 'Restore-and-exit timed out; desktop files were not replaced.' }
        $closing.Refresh()
        if ($closing.ExitCode -ne 0) { throw "Restore-and-exit failed: $($closing.ExitCode)" }
    }
}

$backup = Join-Path $target ('desktop-backups/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff'))
New-Item -ItemType Directory -Force -Path $backup | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $target 'lgengine') | Out-Null
$hostHash = (Get-FileHash -LiteralPath $hostSource -Algorithm SHA256).Hash.ToLowerInvariant()
$hostName = "LGEngineHost-$hostHash.exe"
$hostTarget = Join-Path $target "lgengine/$hostName"
# Immutable host filenames allow updates while a shared older host is serving
# other applications. It is allowed to exit on its own idle policy.
if (-not (Test-Path -LiteralPath $hostTarget)) { Copy-Item -LiteralPath $hostSource -Destination $hostTarget }
if ((Get-FileHash -LiteralPath $hostTarget -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hostHash) { throw 'Host verification failed.' }
$applied = [Collections.Generic.List[object]]::new()
try {
    foreach ($file in $files) {
        $from = Join-Path $source $file
        $to = Join-Path $target $file
        $saved = Join-Path $backup $file
        New-Item -ItemType Directory -Force -Path (Split-Path $saved) | Out-Null
        $existed = Test-Path -LiteralPath $to
        if ($existed) { Copy-Item -LiteralPath $to -Destination $saved }
        $applied.Add([pscustomobject]@{target=$to; backup=$saved; existed=$existed})
        $copied = $false
        foreach ($attempt in 1..20) {
            try { Copy-Item -LiteralPath $from -Destination $to -Force; $copied=$true; break }
            catch [IO.IOException] { Start-Sleep -Milliseconds 150 }
        }
        if (-not $copied) { throw "File is still in use: $to" }
        if ((Get-FileHash -LiteralPath $from).Hash -ne (Get-FileHash -LiteralPath $to).Hash) { throw "Hash mismatch: $file" }
    }
    $pointer = Join-Path $target 'lgengine/host-current.ini'
    $pending = Join-Path $target 'lgengine/host-current.pending.ini'
    [IO.File]::WriteAllText($pending, "[Host]`r`nExecutable=$hostName`r`n", [Text.Encoding]::Unicode)
    if (Test-Path -LiteralPath $pointer) { [IO.File]::Replace($pending,$pointer,(Join-Path $backup 'host-current.ini')) }
    else { [IO.File]::Move($pending,$pointer) }
} catch {
    foreach ($item in $applied) {
        if ($item.existed) { Copy-Item -LiteralPath $item.backup -Destination $item.target -Force }
        elseif (Test-Path -LiteralPath $item.target) { Remove-Item -LiteralPath $item.target -Force }
    }
    throw
}
$record = [pscustomobject]@{ updatedUtc=[DateTime]::UtcNow.ToString('o'); shortcut=$shortcutPath; target=$expected; source=$source; backup=$backup; exeSha256=(Get-FileHash -LiteralPath $expected).Hash; host=$hostTarget; hostSha256=$hostHash }
$record | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $target 'desktop-update.json') -Encoding utf8
$record | ConvertTo-Json
# Settings, personal rules and restoration journals are deliberately left in place.
