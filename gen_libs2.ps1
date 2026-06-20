$ErrorActionPreference = "Stop"
$obsBin = "H:\obs29\obs-studio\bin\64bit"
$obsLib = "H:\obs29\obs-studio\lib"
$dumpbin = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\dumpbin.exe"
$libTool = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\lib.exe"

function Make-Lib
{
    param([string]$dllName, [string]$libName)
    $dllPath = Join-Path $obsBin $dllName
    $baseName = $dllName -replace '\.dll$',''
    $defPath = Join-Path $obsLib "$baseName.def"
    $libPath = Join-Path $obsLib $libName

    Write-Host "Processing $dllName..."
    if (-not (Test-Path $dllPath)) { Write-Host "  DLL not found"; return $false }

    $raw = & $dumpbin /exports $dllPath 2>&1
    $lines = @("LIBRARY $baseName", "EXPORTS")
    $capture = $false
    foreach ($ln in $raw)
    {
        if ($ln -match 'ordinal hint RVA') { $capture = $true; continue }
        if ($capture -and $ln -match '^\s*Summary') { break }
        if ($capture -and $ln -match '^\s+\d+\s+\w+\s+[0-9A-Fa-f]+\s+(\w+)') {
            $fn = $Matches[1]
            if ($fn -and $fn.Length -gt 0) { $lines += "    $fn" }
        }
    }

    $count = $lines.Count - 2
    Write-Host "  Exports found: $count"
    if ($count -le 0) { Write-Host "  No exports!"; return $false }

    [System.IO.File]::WriteAllLines($defPath, $lines, [System.Text.Encoding]::ASCII)

    & $libTool /def:$defPath /out:$libPath /machine:x64 2>&1 | Out-Null

    if (Test-Path $libPath) {
        $sz = (Get-Item $libPath).Length
        Write-Host "  OK: $libPath - $sz bytes"
        return $true
    }
    Write-Host "  Failed to create lib"
    return $false
}

Write-Host "=== Generating import libraries ==="
$a = Make-Lib -dllName "obs.dll" -libName "obs.lib"
$b = Make-Lib -dllName "obs-frontend-api.dll" -libName "obs-frontend-api.lib"
if ($a -and $b) { Write-Host "All done!" } else { Write-Host "Some failed." }
