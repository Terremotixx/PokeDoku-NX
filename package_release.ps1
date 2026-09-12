$ErrorActionPreference = "Stop"

$Version = "1.1.0"
$App = "PokeDoku-NX"
$Nro = "$App.nro"
$ReleaseRoot = "release"
$ReleaseName = "$App-v$Version"
$ReleaseDir = Join-Path $ReleaseRoot $ReleaseName
$InstallDir = Join-Path $ReleaseDir "switch\$App"
$Zip = Join-Path $ReleaseRoot "$ReleaseName.zip"

if (-not (Test-Path $Nro)) {
    Write-Host "ERROR: $Nro not found."
    Write-Host "Run 'make clean' and 'make' first from the PokeDoku-NX project folder."
    exit 1
}

if (Test-Path $ReleaseDir) {
    Remove-Item $ReleaseDir -Recurse -Force
}

if (Test-Path $Zip) {
    Remove-Item $Zip -Force
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Copy-Item $Nro (Join-Path $InstallDir $Nro)

if (Test-Path "README.md") {
    Copy-Item "README.md" (Join-Path $ReleaseDir "README.md")
}

Compress-Archive -Path $ReleaseDir -DestinationPath $Zip -CompressionLevel Optimal

Write-Host ""
Write-Host "DONE"
Write-Host "Created: $Zip"
Write-Host ""
Write-Host "GitHub Release assets:"
Write-Host "  1. $Nro"
Write-Host "  2. $Zip"
