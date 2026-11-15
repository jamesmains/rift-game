# Triffall 1.3.0 - remake the store screenshots and promo art from the current game (needs .\build.ps1 first and Python
# with Pillow). Runs src\tools\presskit_shots.txt with the headless game at 16:9, then src\tools\promo_kit.py:
#   press-kit\screenshots\16x9_1920x1080\   the store screenshots (combat first)
#   press-kit\art\                          key art, Steam capsules / hero / logo / icons, itch cover / banner / social
# Everything comes from the game's own pixels (no generated or painted art). Text for the pages: press-kit\STORE-TEXT.md.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "build\cmake\Release\triffall_headless.exe"
if (-not (Test-Path $exe)) { throw "Build first: .\build.ps1" }
$raw = Join-Path $env:TEMP "triffall-presskit-raw"
if (Test-Path $raw) { Remove-Item $raw -Recurse -Force }
New-Item -ItemType Directory $raw | Out-Null
# the headless game reads assets\ from its working folder and may write saves\: run it from a copy
$work = Join-Path $env:TEMP "triffall-presskit-work"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory $work | Out-Null
Copy-Item (Join-Path $root "src\assets") (Join-Path $work "assets") -Recurse
# leave out campaigns git ignores (the owner's local test campaigns): the desktop shot must show what players get
Get-ChildItem (Join-Path $work "assets\campaigns") -Directory | ForEach-Object {
    Push-Location $root; git check-ignore -q "src/assets/campaigns/$($_.Name)/campaign.txt" 2>$null; $ign = $LASTEXITCODE -eq 0; Pop-Location
    if ($ign) { Remove-Item $_.FullName -Recurse -Force }
}
Push-Location $work
try { & $exe (Join-Path $root "src\tools\presskit_shots.txt") "$raw\" | Select-String "^shot" } finally { Pop-Location }
$shots = Join-Path $root "press-kit\screenshots\16x9_1920x1080"
if (Test-Path $shots) { Remove-Item $shots -Recurse -Force }
python (Join-Path $root "src\tools\promo_kit.py") $raw (Join-Path $root "press-kit\art") $shots
Remove-Item $raw, $work -Recurse -Force
Write-Host "Press kit remade in press-kit\" -ForegroundColor Green
