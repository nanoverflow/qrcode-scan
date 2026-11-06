<#
.SYNOPSIS
  Package two release artifacts and publish a GitHub Release.

.DESCRIPTION
  Uploads:
    1) CimbarQtSender-windows-x64-vX.zip
    2) CimbarReceiver-arm64-v8a-vX.apk

  Requires: GitHub CLI (`gh`) logged in, or env GITHUB_TOKEN + remote origin.

.PARAMETER Version
  Release version without leading v (e.g. 0.1.0). Tag will be v$Version.

.PARAMETER Title
  Release title. Default: "Cimbar bar-code v$Version"

.PARAMETER Notes
  Release notes body. Default auto-generated.

.PARAMETER Draft
  Create as draft release.

.PARAMETER Repo
  owner/name. Default: detect from git remote origin.

.PARAMETER SkipPackage
  Use existing dist/release artifacts only.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Version,
    [string]$Title = "",
    [string]$Notes = "",
    [switch]$Draft,
    [string]$Repo = "",
    [switch]$SkipPackage
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$Version = $Version.TrimStart("v")
$Tag = "v$Version"

if (-not $SkipPackage) {
    & "$PSScriptRoot\package-release.ps1" -Version $Version | Out-Null
}

$OutDir = Join-Path $Root "dist\release"
$qtZip = Get-ChildItem $OutDir -Filter "CimbarQtSender-windows-x64-v$Version.zip" | Select-Object -First 1
$apk = Get-ChildItem $OutDir -Filter "CimbarReceiver-arm64-v8a-v$Version.apk" | Select-Object -First 1
if (-not $qtZip -or -not $apk) {
    throw "Missing packaged files in $OutDir. Run package-release.ps1 first."
}

if (-not $Title) {
    $Title = "Cimbar bar-code $Tag"
}
if (-not $Notes) {
    $Notes = @"
## Assets
- **CimbarQtSender-windows-x64**: Windows Qt 发送端（解压后运行 ``cimbar_qt_sender.exe``）
- **CimbarReceiver-arm64-v8a**: Android 接收端 APK（arm64）

## 用法
1. 电脑解压并运行 Qt 发送端，打开文件开始发送
2. 手机安装 APK，摄像头对准屏幕接收
"@
}

function Get-RepoSlug {
    if ($Repo) { return $Repo }
    Push-Location $Root
    try {
        $url = git remote get-url origin 2>$null
        if (-not $url) { return "" }
        if ($url -match 'github\.com[:/](.+?)(?:\.git)?$') {
            return $Matches[1]
        }
    } finally {
        Pop-Location
    }
    return ""
}

$repoSlug = Get-RepoSlug
if (-not $repoSlug) {
    throw "Cannot detect GitHub repo. Pass -Repo owner/name or set git remote origin."
}

$gh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $gh) {
    Write-Host "GitHub CLI (gh) not found. Trying winget install..."
    winget install --id GitHub.cli -e --accept-package-agreements --accept-source-agreements
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("Path", "User")
    $gh = Get-Command gh -ErrorAction SilentlyContinue
    if (-not $gh) {
        throw "Please install GitHub CLI: https://cli.github.com/ then run: gh auth login"
    }
}

# Ensure auth
$auth = & gh auth status 2>&1
if ($LASTEXITCODE -ne 0) {
    throw "gh is not authenticated. Run: gh auth login"
}

$draftArgs = @()
if ($Draft) { $draftArgs += "--draft" }

Write-Host "Publishing $Tag to $repoSlug ..."
& gh release create $Tag `
    $qtZip.FullName `
    $apk.FullName `
    --repo $repoSlug `
    --title $Title `
    --notes $Notes `
    @draftArgs `
    --verify-tag=false

if ($LASTEXITCODE -ne 0) {
    # Tag may not exist yet — create release without requiring tag, or create tag
    Write-Host "Retry: create tag locally and release..."
    Push-Location $Root
    try {
        git tag -f $Tag 2>$null
        git push origin $Tag 2>$null
    } catch {}
    Pop-Location

    & gh release create $Tag `
        $qtZip.FullName `
        $apk.FullName `
        --repo $repoSlug `
        --title $Title `
        --notes $Notes `
        @draftArgs
    if ($LASTEXITCODE -ne 0) {
        throw "gh release create failed"
    }
}

Write-Host "Done. Open: https://github.com/$repoSlug/releases/tag/$Tag"
