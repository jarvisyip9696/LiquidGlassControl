param(
    [string]$SourceDirectory = (Join-Path $PSScriptRoot '../../build/LGEngine-upstream'),
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../../build/candidates/lgengine/lgengine')
)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$out = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath (Join-Path $source 'src/GlassService.cs'))) { throw 'Select an LGEngine source checkout.' }
# Local builds only: this script does not download, package or publish upstream.
& (Join-Path $source 'build.ps1') -NoBench
if (-not $?) { throw 'LGEngine build failed.' }
New-Item -ItemType Directory -Force -Path $out | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'bin/LGEngine.dll') -Destination (Join-Path $out 'LGEngine.dll')
$csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $csc)) { $csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe' }
$fx = Split-Path $csc
$refs = 'System.dll', 'System.Core.dll', 'System.Drawing.dll', 'System.Windows.Forms.dll' |
    ForEach-Object { '/r:' + (Join-Path $fx $_) }
$exe = Join-Path $out 'LiquidGlass.LGPreview.exe'
& $csc /nologo /noconfig /target:winexe /optimize+ /platform:x64 /warn:4 "/out:$exe" $refs "/r:$(Join-Path $out 'LGEngine.dll')" (Join-Path $PSScriptRoot 'Preview.cs')
if ($LASTEXITCODE -ne 0) { throw 'LGEngine adapter build failed.' }
Write-Output "Built optional preview: $exe"
