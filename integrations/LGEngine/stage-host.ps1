param([Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../../build/LGEngine-2.7.16'
& (Join-Path $PSScriptRoot 'prepare-upstream.ps1') -SourceDirectory $source
& (Join-Path $source 'build.ps1') -HostOnly
if (-not $?) { throw 'LGEngine host compilation failed.' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$built = Join-Path $source 'bin/LGEngineHost.exe'
$hash = (Get-FileHash -LiteralPath $built).Hash.ToLowerInvariant()
$name = "LGEngineHost-$hash.exe"
Copy-Item -LiteralPath $built -Destination (Join-Path $OutputDirectory $name)
[IO.File]::WriteAllText((Join-Path ([IO.Path]::GetFullPath($OutputDirectory)) 'host-current.ini'), "[Host]`r`nExecutable=$name`r`n", [Text.Encoding]::Unicode)
