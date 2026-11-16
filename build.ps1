# Triffall - build into build\game (Triffall.exe + assets\ + README.txt).
#   .\build.ps1                          build for testing (full game, with the Editor)
#   .\build.ps1 -NoEditor                the game without the Editor
#   .\build.ps1 -Demo                    the demo (no Editor, first chapter only)
#   .\build.ps1 -Dev                     debug build: full game plus the dev console (~), in build\game-dev; never released
#   .\build.ps1 -Release -Notes "..."    also publish to releases\ (zip + latest.json)
#   .\build.ps1 -Package                 also make a clean copy to hand out (no saves\) in build\dist\<full|noeditor|demo>
# Each variant builds into its own folder: build\game (full), build\game-noeditor, build\game-demo, build\game-dev.
# Needs Visual Studio (C++ workload) and CMake. raylib 5.5 is downloaded by CMake on the first build (needs internet once).
param([switch]$Release, [string]$Notes = "", [switch]$NoEditor, [switch]$Demo, [switch]$Package, [switch]$Dev)
$ErrorActionPreference = "Continue"   # native tools (cmake, MSBuild) write warnings to stderr
Set-Location $PSScriptRoot

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake is not installed (or not on PATH). Install CMake and Visual Studio (C++) to build Triffall." -ForegroundColor Red; exit 1
}

# Single version source: TRIFFALL_VERSION in src\src\core\version.h
$header = Get-Content "src\src\core\version.h" -Raw
if ($header -notmatch 'TRIFFALL_VERSION "([0-9.]+)"') { throw "TRIFFALL_VERSION not found in src\src\core\version.h" }
$version = $Matches[1]

$editor = "ON"; $demoFlag = "OFF"; $cmakeDir = "build\cmake"; $kind = "full"; $out = "build\game"; $dist = "build\dist\full"
if ($NoEditor) { $editor = "OFF"; $cmakeDir = "build\cmake-noeditor"; $kind = "no editor"; $out = "build\game-noeditor"; $dist = "build\dist\noeditor" }
if ($Demo)     { $editor = "OFF"; $demoFlag = "ON"; $cmakeDir = "build\cmake-demo"; $kind = "demo"; $out = "build\game-demo"; $dist = "build\dist\demo" }
$devFlag = "OFF"
if ($Dev) {   # the dev console only exists in this build; it must never be handed out
    if ($Release -or $Package -or $NoEditor -or $Demo) { Write-Host "-Dev is a debug build for testing: it can't be combined with -Release, -Package, -NoEditor or -Demo." -ForegroundColor Red; exit 1 }
    $devFlag = "ON"; $cmakeDir = "build\cmake-dev"; $kind = "debug, dev console"; $out = "build\game-dev"
}
Write-Host "Building Triffall v$version ($kind)..."

# 1.1.3: Editor edits saved in a built or installed copy go into src\assets first, so this build keeps them
& "$PSScriptRoot\tools\pull-editor-edits.ps1" | Out-Null
# 1.3.0: the licensed music isn't in git; unpack it from assets\packs when it's missing
& "$PSScriptRoot\tools\unpack-music.ps1"

# the built-in tile kinds are compiled from assets\data\tilekinds.txt (src\src\game\tilekinds_builtin.h is committed too)
if (Get-Command python -ErrorAction SilentlyContinue) { python src\tools\mktilekinds.py }
# 1.3.4: likewise the default sounds.txt (src\src\core\sounds_builtin.h is committed too)
if (Get-Command python -ErrorAction SilentlyContinue) { python src\tools\mksoundsdefault.py }

cmake -S src -B $cmakeDir -A x64 "-DTRIFFALL_EDITOR=$editor" "-DTRIFFALL_DEMO=$demoFlag" "-DTRIFFALL_DEV_CONSOLE=$devFlag"   # quoted: PowerShell leaves $vars unexpanded in a bare -arg
if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed. Is Visual Studio with the C++ workload installed?" -ForegroundColor Red; exit 1 }
cmake --build $cmakeDir --config Release --target Triffall triffall_headless
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }

# $out: the exe, its assets (without the art-source folder new-art) and the readme. saves\ is kept between builds.
New-Item -ItemType Directory $out -Force | Out-Null
Get-ChildItem $out -Force | Where-Object { $_.Name -ne "saves" } | Remove-Item -Recurse -Force
Copy-Item "$cmakeDir\Release\Triffall.exe" $out
Copy-Item "src\assets" "$out\assets" -Recurse
if (Test-Path "$out\assets\new-art") { Remove-Item "$out\assets\new-art" -Recurse -Force }
Copy-Item "src\README.md" "$out\README.txt"
& "$PSScriptRoot\tools\pull-editor-edits.ps1" -ManifestFor $out   # assets-manifest.txt: how assets\ looked as built, to spot Editor edits later
Write-Host "Built $out\Triffall.exe (v$version, $kind)" -ForegroundColor Green

# 1.3.0: campaigns that git ignores (the owner's own test campaigns, e.g. with a bought tileset) stay in build\game, but
# never go into a copy to hand out (-Package, the release zip, and since 1.7.0 the releases\<version> folder - so not
# into the installed copy either). None is ignored since 1.7.0 (#144 removed the local "test" campaign).
function Remove-LocalCampaigns([string]$assetsDir) {
    Get-ChildItem (Join-Path $assetsDir "campaigns") -Directory -ErrorAction SilentlyContinue | ForEach-Object {
        git check-ignore -q "src/assets/campaigns/$($_.Name)/campaign.txt" 2>$null
        if ($LASTEXITCODE -eq 0) { Remove-Item $_.FullName -Recurse -Force; Write-Host "  (left out the local campaign $($_.Name))" -ForegroundColor DarkGray }
    }
}

if ($Package) {   # clean copy to hand out: exe, assets, readme - no saves\
    if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
    New-Item -ItemType Directory $dist -Force | Out-Null
    Copy-Item "$out\Triffall.exe", "$out\README.txt" $dist
    Copy-Item "$out\assets" "$dist\assets" -Recurse
    Remove-LocalCampaigns "$dist\assets"
    Write-Host "Packaged $dist (v$version, $kind)" -ForegroundColor Green
}

if ($Release) {
    if (-not $Notes) { throw "Pass -Notes with -Release." }
    New-Item -ItemType Directory "releases" -Force | Out-Null
    $suffix = ""; if ($NoEditor) { $suffix = "-noeditor" }; if ($Demo) { $suffix = "-demo" }
    $zip = "releases\triffall-v$version$suffix-windows.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    $stage = Join-Path $env:TEMP "triffall-release-stage"   # the zip is a hand-out copy: without local campaigns
    if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
    New-Item -ItemType Directory $stage | Out-Null
    Copy-Item "$out\Triffall.exe", "$out\README.txt" $stage
    Copy-Item "$out\assets" "$stage\assets" -Recurse
    Remove-LocalCampaigns "$stage\assets"
    Compress-Archive -Path "$stage\Triffall.exe", "$stage\assets", "$stage\README.txt" -DestinationPath $zip
    Remove-Item $stage -Recurse -Force
    $dir = "releases\$version$suffix"
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory $dir | Out-Null
    Copy-Item "$out\Triffall.exe", "$out\README.txt", "$out\assets-manifest.txt" $dir
    Copy-Item "$out\assets" "$dir\assets" -Recurse
    Remove-LocalCampaigns "$dir\assets"   # 1.7.0 (#143): the release folder is a complete hand-out copy too
    if (-not $Demo -and -not $NoEditor) {   # latest.json always points at the full game
        # 1.7.0 (#143): "exe" names the release FOLDER (exe + assets). Idea Jotter's itch.io upload of "The release" pushes
        # what exe names, and "1.6.5/Triffall.exe" sent the exe alone, without the game's assets. install.ps1 and the
        # in-game updater go by "version" (releases\<version>\).
        $latest = [ordered]@{
            version  = $version
            exe      = "$version"
            zip      = "triffall-v$version-windows.zip"
            notes    = $Notes
            released = (Get-Date).ToString("s")
        }
        $json = $latest | ConvertTo-Json
        [System.IO.File]::WriteAllText((Join-Path $PSScriptRoot "releases\latest.json"), $json, (New-Object System.Text.UTF8Encoding $false))
    }
    Write-Host "Released v$version$suffix to releases\ ($zip)" -ForegroundColor Green
}

# 1.7.0 (#143): end with success on purpose. Without this the script's exit code was the last native command's: the
# final `git check-ignore` in Remove-LocalCampaigns returns 1 for a campaign that isn't ignored (the Tutorial), so every
# -Package build "failed" in Idea Jotter (Zip, the itch.io upload of "Full game") although it had built fine.
exit 0
