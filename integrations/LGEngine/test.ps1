param([string]$SourceDirectory = (Join-Path $PSScriptRoot '../../build/LGEngine-upstream'), [switch]$BuildOnly)
$ErrorActionPreference = 'Stop'
$source = (Resolve-Path -LiteralPath $SourceDirectory).Path
$out = Join-Path $source 'bin'
New-Item -ItemType Directory -Force -Path $out | Out-Null
$csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
if (-not (Test-Path -LiteralPath $csc)) {
    $csc = Join-Path $env:WINDIR 'Microsoft.NET/Framework/v4.0.30319/csc.exe'
}
$fx = Split-Path $csc
$refs = 'System.dll', 'System.Core.dll', 'System.Drawing.dll', 'System.Windows.Forms.dll' |
    ForEach-Object { '/r:' + (Join-Path $fx $_) }
$exe = Join-Path $out 'LGRegression.exe'
& $csc /nologo /noconfig /target:exe /optimize+ /platform:anycpu /warn:4 "/out:$exe" $refs (Join-Path $source 'src/*.cs') (Join-Path $PSScriptRoot 'Regression.cs')
if ($LASTEXITCODE -ne 0) { throw 'LGEngine regression build failed.' }
if ($BuildOnly) { Write-Output "Compiled regression checks: $exe"; return }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'LGEngine regressions failed.' }
