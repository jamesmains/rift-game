# Triffall - build into build\game (Triffall.exe + assets\ + README.txt).
#   .\build.ps1                          build for testing (full game, with the Editor)
#   .\build.ps1 -NoEditor                player build without the Editor (what Steam would ship)
#   .\build.ps1 -Demo                    the demo (no Editor, first chapter only)
#   .\build.ps1 -Release -Notes "..."    also publish to releases\ (zip + latest.json)
# Needs Visual Studio (C++ workload) and CMake. raylib 5.5 is downloaded by CMake on the first build (needs internet once).
param([switch]$Release, [string]$Notes = "", [switch]$NoEditor, [switch]$Demo)
$ErrorActionPreference = "Continue"   # native tools (cmake, MSBuild) write warnings to stderr
Set-Location $PSScriptRoot

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Write-Host "CMake is not installed (or not on PATH). Install CMake and Visual Studio (C++) to build Triffall." -ForegroundColor Red; exit 1
}

# Single version source: TRIFFALL_VERSION in src\src\core\version.h
$header = Get-Content "src\src\core\version.h" -Raw
if ($header -notmatch 'TRIFFALL_VERSION "([0-9.]+)"') { throw "TRIFFALL_VERSION not found in src\src\core\version.h" }
$version = $Matches[1]

$editor = "ON"; $demoFlag = "OFF"; $cmakeDir = "build\cmake"; $kind = "full"
if ($NoEditor) { $editor = "OFF"; $cmakeDir = "build\cmake-noeditor"; $kind = "no editor" }
if ($Demo)     { $editor = "OFF"; $demoFlag = "ON"; $cmakeDir = "build\cmake-demo"; $kind = "demo" }
Write-Host "Building Triffall v$version ($kind)..."

cmake -S src -B $cmakeDir -A x64 -DTRIFFALL_EDITOR=$editor -DTRIFFALL_DEMO=$demoFlag
if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed. Is Visual Studio with the C++ workload installed?" -ForegroundColor Red; exit 1 }
cmake --build $cmakeDir --config Release --target Triffall triffall_headless
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }

# build\game: the exe, its assets (without the art-source folder new-art) and the readme. saves\ is kept between builds.
$out = "build\game"
New-Item -ItemType Directory $out -Force | Out-Null
Get-ChildItem $out -Force | Where-Object { $_.Name -ne "saves" } | Remove-Item -Recurse -Force
Copy-Item "$cmakeDir\Release\Triffall.exe" $out
Copy-Item "src\assets" "$out\assets" -Recurse
if (Test-Path "$out\assets\new-art") { Remove-Item "$out\assets\new-art" -Recurse -Force }
Copy-Item "src\README.md" "$out\README.txt"
Write-Host "Built $out\Triffall.exe (v$version, $kind)" -ForegroundColor Green

if ($Release) {
    if (-not $Notes) { throw "Pass -Notes with -Release." }
    New-Item -ItemType Directory "releases" -Force | Out-Null
    $suffix = ""; if ($NoEditor) { $suffix = "-noeditor" }; if ($Demo) { $suffix = "-demo" }
    $zip = "releases\triffall-v$version$suffix-windows.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path "$out\Triffall.exe", "$out\assets", "$out\README.txt" -DestinationPath $zip
    $dir = "releases\$version$suffix"
    if (Test-Path $dir) { Remove-Item $dir -Recurse -Force }
    New-Item -ItemType Directory $dir | Out-Null
    Copy-Item "$out\Triffall.exe", "$out\README.txt" $dir
    Copy-Item "$out\assets" "$dir\assets" -Recurse
    if (-not $Demo -and -not $NoEditor) {   # latest.json always points at the full game
        $latest = [ordered]@{
            version  = $version
            exe      = "$version/Triffall.exe"
            zip      = "triffall-v$version-windows.zip"
            notes    = $Notes
            released = (Get-Date).ToString("s")
        }
        $json = $latest | ConvertTo-Json
        [System.IO.File]::WriteAllText((Join-Path $PSScriptRoot "releases\latest.json"), $json, (New-Object System.Text.UTF8Encoding $false))
    }
    Write-Host "Released v$version$suffix to releases\ ($zip)" -ForegroundColor Green
}
