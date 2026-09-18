$ErrorActionPreference = "Stop"

try {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
} catch {
}

$ProjectRoot = $PSScriptRoot
$SfxDir = Join-Path $ProjectRoot "romfs\sfx"
$TempDir = Join-Path $ProjectRoot ".sfx_temp"

New-Item -ItemType Directory -Force -Path $SfxDir | Out-Null
New-Item -ItemType Directory -Force -Path $TempDir | Out-Null

function Download-File {
    param(
        [Parameter(Mandatory = $true)][string]$Url,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    Write-Host "Downloading $(Split-Path $Destination -Leaf)..."
    Invoke-WebRequest -UseBasicParsing -Uri $Url -OutFile $Destination
}

$KenneyBase = "https://raw.githubusercontent.com/Calinou/kenney-interface-sounds/master/addons/kenney_interface_sounds"
$JothBase = "https://raw.githubusercontent.com/novincode/atomcut-library/main/packs/opengameart-7-assorted-sound-effects-menu-level-up/audio"

# Core UI sounds: Kenney Interface Sounds (CC0)
$KenneySounds = @{
    "move.wav"   = "tick_001.wav"
    "select.wav" = "confirmation_001.wav"
    "back.wav"   = "back_001.wav"
    "open.wav"   = "open_001.wav"
    "close.wav"  = "close_001.wav"
    "toggle.wav" = "toggle_001.wav"
}

foreach ($TargetName in $KenneySounds.Keys) {
    $SourceName = $KenneySounds[$TargetName]
    $Url = "$KenneyBase/$SourceName"
    $Destination = Join-Path $SfxDir $TargetName
    Download-File -Url $Url -Destination $Destination
}

$UsedJoth = $false
$Ffmpeg = Get-Command ffmpeg -ErrorAction SilentlyContinue

if ($null -ne $Ffmpeg) {
    Write-Host ""
    Write-Host "ffmpeg found. Using the Joth CC0 pack for correct / wrong / win."

    $JothSounds = @{
        "correct.wav" = "item-pickup.m4a"
        "wrong.wav"   = "menu-error.m4a"
        "win.wav"     = "level-up.m4a"
    }

    try {
        foreach ($TargetName in $JothSounds.Keys) {
            $SourceName = $JothSounds[$TargetName]
            $TempFile = Join-Path $TempDir $SourceName
            $Destination = Join-Path $SfxDir $TargetName

            Download-File -Url "$JothBase/$SourceName" -Destination $TempFile

            Write-Host "Converting $SourceName -> $TargetName..."
            & $Ffmpeg.Source -y -loglevel error -i $TempFile -ac 2 -ar 48000 -c:a pcm_s16le $Destination

            if ($LASTEXITCODE -ne 0) {
                throw "ffmpeg failed while converting $SourceName"
            }
        }

        $UsedJoth = $true
    }
    catch {
        Write-Warning "Joth conversion failed: $($_.Exception.Message)"
        Write-Host "Using Kenney CC0 fallback sounds for correct / wrong / win."
    }
}
else {
    Write-Host ""
    Write-Host "ffmpeg was not found. Using Kenney CC0 fallback sounds for correct / wrong / win."
    Write-Host "Install ffmpeg and run this script again later if you want the Joth gameplay sounds."
}

if (-not $UsedJoth) {
    $KenneyFallback = @{
        "correct.wav" = "pluck_001.wav"
        "wrong.wav"   = "error_004.wav"
        "win.wav"     = "confirmation_004.wav"
    }

    foreach ($TargetName in $KenneyFallback.Keys) {
        $SourceName = $KenneyFallback[$TargetName]
        $Url = "$KenneyBase/$SourceName"
        $Destination = Join-Path $SfxDir $TargetName
        Download-File -Url $Url -Destination $Destination
    }
}

if (Test-Path $TempDir) {
    Remove-Item -Recurse -Force $TempDir
}

$Expected = @(
    "move.wav",
    "select.wav",
    "back.wav",
    "open.wav",
    "close.wav",
    "toggle.wav",
    "correct.wav",
    "wrong.wav",
    "win.wav"
)

$Missing = @()
foreach ($Name in $Expected) {
    $Path = Join-Path $SfxDir $Name
    if (-not (Test-Path $Path)) {
        $Missing += $Name
    }
}

Write-Host ""
if ($Missing.Count -eq 0) {
    Write-Host "SFX setup complete." -ForegroundColor Green
    Write-Host "Files created in: $SfxDir"

    if ($UsedJoth) {
        Write-Host "Gameplay set: Joth (correct / wrong / win)"
    }
    else {
        Write-Host "Gameplay set: Kenney fallback (correct / wrong / win)"
    }

    Write-Host ""
    Write-Host "Now run:"
    Write-Host "  make clean"
    Write-Host "  make"
}
else {
    Write-Host "SFX setup is incomplete." -ForegroundColor Red
    Write-Host "Missing: $($Missing -join ', ')"
    exit 1
}
