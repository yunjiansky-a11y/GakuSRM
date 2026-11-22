param(
    [string]$GameRoot = ''
)

$ErrorActionPreference = 'Stop'

function Resolve-GameRoot {
    param([string]$Root)
    $candidates = @()
    if ($Root) { $candidates += $Root }
    if ($env:GAKUMAS_ROOT) { $candidates += $env:GAKUMAS_ROOT }
    $candidates += @('C:\gakumas', 'D:\gakumas', 'E:\gakumas')
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path (Join-Path $candidate 'GakuLoader\plugins'))) {
            return $candidate
        }
    }
    return ''
}

$root = Resolve-GameRoot $GameRoot
if (-not $root) {
    Write-Host '[-] 未找到游戏目录，请用 -GameRoot 指定。' -ForegroundColor Red
    exit 1
}

$plugins = Join-Path $root 'GakuLoader\plugins'
foreach ($name in @('GakuSMM.dll', 'GakuSMM_naming.tsv', 'GakuSMM.log')) {
    $path = Join-Path $plugins $name
    if (Test-Path $path) {
        Remove-Item $path -Force
        Write-Host "[+] 已删除 $name"
    }
}
Write-Host '[+] 卸载完成' -ForegroundColor Green
