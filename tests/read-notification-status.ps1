param([Parameter(Mandatory)][int]$ProcessId)
$ErrorActionPreference = 'Stop'
$mapping = [IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting("Local\LiquidGlass.Engine.Status.v3.$ProcessId", [IO.MemoryMappedFiles.MemoryMappedFileRights]::Read)
try {
    $view = $mapping.CreateViewAccessor(0, 92, [IO.MemoryMappedFiles.MemoryMappedFileAccess]::Read)
    try {
        if ($view.ReadUInt32(0) -ne 0x4c474531 -or $view.ReadInt32(4) -ne 3 -or $view.ReadInt32(8) -ne $ProcessId) { throw 'Unexpected status mapping header.' }
        $brushNames = @('None', 'Acrylic', 'Composition', 'Solid', 'Other')
        $roots = @{}
        foreach ($entry in @(@('Notification',36), @('Calendar',64))) {
            $offset = [int]$entry[1]
            $kind = $view.ReadInt32($offset + 12)
            $roots[$entry[0]] = [ordered]@{
                Width = $view.ReadInt32($offset)
                Height = $view.ReadInt32($offset + 4)
                Visible = $view.ReadInt32($offset + 8) -ne 0
                Brush = if ($kind -ge 0 -and $kind -lt $brushNames.Length) { $brushNames[$kind] } else { 'Unknown' }
                Error = '0x{0:X8}' -f $view.ReadInt32($offset + 16)
                BrushWhenVisible = $view.ReadInt32($offset + 20)
                VisibleSamples = $view.ReadInt32($offset + 24)
            }
        }
        [ordered]@{
            Owner = $ProcessId
            Success = $view.ReadInt32(12)
            Fallback = $view.ReadInt32(16)
            Failed = $view.ReadInt32(20)
            Error = '0x{0:X8}' -f $view.ReadInt32(24)
            Connected = $view.ReadInt32(28)
            Subscribed = $view.ReadInt32(32)
            Roots = $roots
        } | ConvertTo-Json -Depth 5
    } finally { $view.Dispose() }
} finally { $mapping.Dispose() }
