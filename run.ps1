# Triffall - build (if needed) and start the game. Runs headless from Idea Jotter: no keyboard input.
#   .\run.ps1              full game with the Editor (build\game)
#   .\run.ps1 -NoEditor    the game without the Editor (build\game-noeditor)
#   .\run.ps1 -Demo        the demo (build\game-demo)
# Fresh runs: when Idea Jotter sets IDEAJOTTER_RUN_DATA (a new empty folder per run), the game gets --saves
# "$env:IDEAJOTTER_RUN_DATA\saves", so saves, settings and the desktop layout start empty. Unset = saves\ next to the exe.
param([switch]$NoEditor, [switch]$Demo)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$dir = "build\game"; $buildArgs = @{}
if ($NoEditor) { $dir = "build\game-noeditor"; $buildArgs = @{ NoEditor = $true } }
if ($Demo)     { $dir = "build\game-demo";     $buildArgs = @{ Demo = $true } }
$exe = "$dir\Triffall.exe"

$newest = Get-ChildItem "src\src", "src\assets", "src\CMakeLists.txt", "build.ps1" -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not (Test-Path $exe) -or (Get-Item $exe).LastWriteTime -lt $newest.LastWriteTime) {
    & "$PSScriptRoot\build.ps1" @buildArgs
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
        Write-Host "Could not build Triffall (see above)." -ForegroundColor Red
        exit 1
    }
}
$gameArgs = @()
if ($env:IDEAJOTTER_RUN_DATA) {
    $saves = Join-Path $env:IDEAJOTTER_RUN_DATA "saves"
    New-Item -ItemType Directory $saves -Force | Out-Null
    $gameArgs = @("--saves", "`"$saves`"")
    Write-Host "Fresh run: saves in $saves"
}
Write-Host "Starting Triffall ($dir)..."
if ($gameArgs) { Start-Process -FilePath (Resolve-Path $exe) -WorkingDirectory (Resolve-Path $dir) -ArgumentList $gameArgs }
else { Start-Process -FilePath (Resolve-Path $exe) -WorkingDirectory (Resolve-Path $dir) }
