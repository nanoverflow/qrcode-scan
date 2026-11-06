<#
.SYNOPSIS
  Package Qt sender ZIP + Android APK for GitHub Release.

.PARAMETER Version
  Version string without leading v (e.g. 0.1.0).

.PARAMETER QtOnly
  Only package the Windows Qt ZIP.

.PARAMETER ApkOnly
  Only copy/rename the Android APK.
#>
[CmdletBinding()]
param(
    [string]$Version = "",
    [string]$QtReleaseDir = "",
    [string]$ApkPath = "",
    [string]$OutDir = "",
    [switch]$QtOnly,
    [switch]$ApkOnly
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot

if (-not $Version) {
    Push-Location $Root
    try {
        $Version = (git describe --tags --exact-match 2>$null)
        if (-not $Version) { $Version = (git describe --tags --always 2>$null) }
    } catch {}
    Pop-Location
    if (-not $Version) { $Version = Get-Date -Format "yyyy.M.d" }
}
$Version = $Version.TrimStart("v")

if (-not $QtReleaseDir) { $QtReleaseDir = Join-Path $Root "qt-sender\build\Release" }
if (-not $ApkPath) {
    $ApkPath = Join-Path $Root "cfc\app\build\outputs\apk\debug\CameraFileCopy-debug.apk"
    if (-not (Test-Path $ApkPath)) {
        $ApkPath = Join-Path $Root "cfc\app\build\outputs\apk\release\CameraFileCopy-release.apk"
    }
}
if (-not $OutDir) { $OutDir = Join-Path $Root "dist\release" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$qtZipName = "CimbarQtSender-windows-x64-v$Version.zip"
$apkName = "CimbarReceiver-arm64-v8a-v$Version.apk"
$qtZip = Join-Path $OutDir $qtZipName
$apkOut = Join-Path $OutDir $apkName
$files = @()

if (-not $ApkOnly) {
    $exe = Join-Path $QtReleaseDir "cimbar_qt_sender.exe"
    if (-not (Test-Path $exe)) {
        throw "Qt sender not found: $exe`nBuild: cmake --build qt-sender/build --config Release"
    }

    $stage = Join-Path $env:TEMP ("cimbar-qt-stage-" + [guid]::NewGuid().ToString("n"))
    New-Item -ItemType Directory -Force -Path $stage | Out-Null
    try {
        Copy-Item $exe $stage
        Get-ChildItem $QtReleaseDir -File -Filter "*.dll" | Copy-Item -Destination $stage
        foreach ($d in @("platforms", "styles", "imageformats", "generic", "iconengines", "networkinformation", "tls")) {
            $src = Join-Path $QtReleaseDir $d
            if (Test-Path $src) { Copy-Item $src (Join-Path $stage $d) -Recurse -Force }
        }
        if (Test-Path $qtZip) { Remove-Item $qtZip -Force }
        Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $qtZip -Force
    }
    finally {
        Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
    $files += @{ name = $qtZipName; bytes = (Get-Item $qtZip).Length; role = "qt-sender-windows" }
    Write-Host "OK $qtZip"
}

if (-not $QtOnly) {
    if (-not (Test-Path $ApkPath)) {
        throw "APK not found: $ApkPath`nBuild: cd cfc; .\gradlew.bat :app:assembleDebug"
    }
    Copy-Item $ApkPath $apkOut -Force
    $files += @{ name = $apkName; bytes = (Get-Item $apkOut).Length; role = "android-receiver" }
    Write-Host "OK $apkOut"
}

@{
    version = $Version
    created = (Get-Date).ToString("o")
    files = $files
} | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutDir "manifest.json") -Encoding UTF8

Write-Host "VERSION=$Version"
@{ Version = $Version; QtZip = $(if (Test-Path $qtZip) { $qtZip } else { $null }); Apk = $(if (Test-Path $apkOut) { $apkOut } else { $null }); OutDir = $OutDir }
