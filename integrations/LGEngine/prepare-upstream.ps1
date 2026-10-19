param([string]$SourceDirectory = (Join-Path $PSScriptRoot '../../build/LGEngine-2.7.12'))
$ErrorActionPreference = 'Stop'
$revision = 'dabe4cb3241a0cd079b254c83c9a06ff95885745'
if (-not (Test-Path -LiteralPath $SourceDirectory)) {
    & git clone https://github.com/domk6966/LGEngine.git $SourceDirectory
    if ($LASTEXITCODE -ne 0) { throw 'LGEngine download failed.' }
    & git -C $SourceDirectory checkout --detach $revision
    if ($LASTEXITCODE -ne 0) { throw 'Cannot select the pinned LGEngine revision.' }
}
$actual = & git -C $SourceDirectory rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $actual -ne $revision) { throw "Expected LGEngine $revision. Existing checkout was preserved." }
& git -C $SourceDirectory diff --quiet HEAD -- src host build.ps1
if ($LASTEXITCODE -ne 0) { throw 'LGEngine runtime sources have local edits; refusing to package an unverified host.' }
