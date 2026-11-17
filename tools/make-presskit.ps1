# Triffall - remake the image kit (press-kit\, listed in press-kit\README.md) from the current game. Needs .\build.ps1
# first and Python with Pillow. Runs src\tools\presskit_shots.txt with the headless game at 16:9 (960x540; the Editor
# 1280x720), then src\tools\promo_kit.py, which writes:
#   press-kit\cover.png                    itch.io cover (also Idea Jotter's project cover)
#   press-kit\art\                         banner, social card, avatar, background, key art, icons + app.ico, Steam art
#   press-kit\screenshots\native\          five shots at the size the game draws them
#   press-kit\screenshots\16x9_1920x1080\  every store screenshot at 1920x1080
# and the game's own icon (src\src\platform\app.ico, the favicon in src\web\shell.html).
# Everything comes from the game's own pixels and letters (no generated or painted art). Store text: press-kit\STORE-TEXT.md.
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
$kit = Join-Path $root "press-kit"
foreach ($d in "screenshots\16x9_1920x1080", "screenshots\native") { $p = Join-Path $kit $d; if (Test-Path $p) { Remove-Item $p -Recurse -Force } }
python (Join-Path $root "src\tools\promo_kit.py") $raw $kit
if ($LASTEXITCODE -ne 0) { throw "promo_kit.py failed" }
# 1.8.3 (#184): one picture set per UI theme (desktop, exploring, party, fight, settings) -> screenshots\themes, 2x
$rawT = Join-Path $raw "themes"; New-Item -ItemType Directory $rawT | Out-Null
Push-Location $work
try { & $exe (Join-Path $root "src\tools\presskit_themes.txt") "$rawT\" | Select-String "^shot" } finally { Pop-Location }
$themes = Join-Path $kit "screenshots\themes"; if (Test-Path $themes) { Remove-Item $themes -Recurse -Force }
New-Item -ItemType Directory $themes | Out-Null
python -c "import sys,glob,os; from PIL import Image; [Image.open(f).resize((Image.open(f).width*2, Image.open(f).height*2), Image.NEAREST).save(os.path.join(sys.argv[2], os.path.basename(f))) for f in glob.glob(os.path.join(sys.argv[1], 'theme_*.png'))]" $rawT $themes
if ($LASTEXITCODE -ne 0) { throw "scaling the theme shots failed" }
Remove-Item $raw, $work -Recurse -Force
Write-Host "Image kit remade in press-kit\ (see press-kit\README.md)" -ForegroundColor Green
