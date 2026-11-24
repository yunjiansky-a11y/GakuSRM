$ErrorActionPreference = 'Stop'
$plugins = 'E:\学偶\gakumas\GakuLoader\plugins'
foreach ($name in @('GakuSRM.dll','GakuSRM_naming.tsv','GakuSRM.log','GakuSRM.UI')) {
    $path = Join-Path $plugins $name
    if (Test-Path $path) { Remove-Item $path -Recurse -Force; Write-Host "[-] removed $name" }
}
Write-Host '[+] GakuSRM removed'
