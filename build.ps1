param(
    [string]$Output = 'dist\GakuSMM.dll',
    [string]$Gxx = 'g++.exe'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$outPath = Join-Path $root $Output
$outDir = Split-Path -Parent $outPath
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

$sources = @(
    (Join-Path $root 'src\GakuSMM.cpp'),
    (Join-Path $root 'deps\imgui\imgui.cpp'),
    (Join-Path $root 'deps\imgui\imgui_draw.cpp'),
    (Join-Path $root 'deps\imgui\imgui_widgets.cpp'),
    (Join-Path $root 'deps\imgui\imgui_tables.cpp')
)

$tempOutput = Join-Path $env:TEMP 'GakuSMM_build.dll'
$arguments = @(
    '-std=c++20', '-O2', '-shared', '-static', '-fpermissive',
    '-I' + (Join-Path $root 'include'),
    '-I' + (Join-Path $root 'deps\imgui'),
    '-o', $tempOutput
) + $sources + @(
    '-lkernel32', '-luser32', '-lgdi32', '-lcomctl32', '-lole32', '-loleaut32'
)

Write-Host "[+] $Gxx $($arguments -join ' ')"
& $Gxx @arguments
if ($LASTEXITCODE -ne 0) { throw "build failed: $LASTEXITCODE" }
Copy-Item $tempOutput $outPath -Force
Remove-Item $tempOutput -Force
Write-Host "[+] built $outPath" -ForegroundColor Green
