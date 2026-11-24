$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$plugins = 'E:\学偶\gakumas\GakuLoader\plugins'
foreach ($old in @('GakuSMM.dll','GakuSMM.UI','GakuSMM_naming.tsv')) {
    $path = Join-Path $plugins $old
    if (Test-Path $path) { Remove-Item $path -Recurse -Force }
}
try {
    Copy-Item (Join-Path $here 'GakuSRM.dll') (Join-Path $plugins 'GakuSRM.dll') -Force
    Write-Host '[+] GakuSRM.dll updated'
} catch {
    Write-Host ('[i] GakuSRM.dll is in use, UI will still be updated: ' + $_.Exception.Message)
}
try { Copy-Item (Join-Path $here 'GakuSRM_naming.tsv') (Join-Path $plugins 'GakuSRM_naming.tsv') -Force } catch {}
$target = Join-Path $plugins 'GakuSRM.UI'
if (Test-Path $target) { Remove-Item $target -Recurse -Force }
New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item (Join-Path $here 'GakuSRM.UI\*') $target -Recurse -Force
Write-Host '[+] GakuSRM.UI updated'
