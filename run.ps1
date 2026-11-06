# Triffall - build (if needed) and start the game.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$exe = "build\game\Triffall.exe"
$newest = Get-ChildItem "src\src", "src\assets", "src\CMakeLists.txt" -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
if (-not (Test-Path $exe) -or (Get-Item $exe).LastWriteTime -lt $newest.LastWriteTime) {
    & "$PSScriptRoot\build.ps1"
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path $exe)) {
        Write-Host "Could not build Triffall (see above)." -ForegroundColor Red
        Read-Host "Press Enter to close"; exit 1
    }
}
Write-Host "Starting Triffall..."
Start-Process -FilePath (Resolve-Path $exe) -WorkingDirectory (Resolve-Path "build\game")
