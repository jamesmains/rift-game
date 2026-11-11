# Triffall - build (if needed) and start the game. Runs headless from Idea Jotter: no keyboard input.
#   .\run.ps1              full game with the Editor (build\game)
#   .\run.ps1 -NoEditor    player build without the Editor (build\game-noeditor)
#   .\run.ps1 -Demo        the demo (build\game-demo)
param([switch]$NoEditor, [switch]$Demo)
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$dir = "build\game"; $buildArgs = @{}
if ($NoEditor) { $dir = "build\game-noeditor"; $buildArgs = @{ NoEditor = $true } }
if ($Demo)     { $dir = "build\game-demo";     $buildArgs = @{ Demo = $true } }
$exe = "$dir\Triffall.exe"

$newest = Get-ChildItem "src\src", "src\assets", "src\CMakeLists.txt" -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not (Test-Path $exe) -or (Get-Item $exe).LastWriteTime -lt $newest.LastWriteTime) {
    & "$PSScriptRoot\build.ps1" @buildArgs
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
        Write-Host "Could not build Triffall (see above)." -ForegroundColor Red
        exit 1
    }
}
Write-Host "Starting Triffall ($dir)..."
Start-Process -FilePath (Resolve-Path $exe) -WorkingDirectory (Resolve-Path $dir)
