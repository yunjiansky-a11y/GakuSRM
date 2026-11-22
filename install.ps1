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
    Write-Host '[-] 未找到游戏目录，请用 -GameRoot 指定，例如：' -ForegroundColor Red
    Write-Host '    powershell -ExecutionPolicy Bypass -File install.ps1 -GameRoot "D:\gakumas"'
    exit 1
}

$plugins = Join-Path $root 'GakuLoader\plugins'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$dll = Join-Path $here 'GakuSMM.dll'
if (-not (Test-Path $dll)) {
    Write-Host '[-] 缺少 GakuSMM.dll' -ForegroundColor Red
    exit 1
}

$targetDll = Join-Path $plugins 'GakuSMM.dll'
if (Test-Path $targetDll) {
    Copy-Item $targetDll ($targetDll + '.bak') -Force
    Write-Host '[i] 已备份旧版为 GakuSMM.dll.bak'
}
Copy-Item $dll $targetDll -Force
Write-Host '[+] 已安装 GakuSMM.dll'

$tsv = Join-Path $here 'GakuSMM_naming.tsv'
if (Test-Path $tsv) {
    Copy-Item $tsv (Join-Path $plugins 'GakuSMM_naming.tsv') -Force
    Write-Host '[+] 已安装可选命名词典'
}

Write-Host ''
Write-Host "[+] 完成：$root" -ForegroundColor Green
