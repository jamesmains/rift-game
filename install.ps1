# Triffall - install the latest release to %LOCALAPPDATA%\Programs\Triffall with a Start Menu shortcut.
# Run .\build.ps1 -Release -Notes "..." first. Re-run this to update (the game has no Update button yet).
# The game keeps its saves in saves\ next to the exe: they are kept when updating.
$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$latestFile = "releases\latest.json"
if (-not (Test-Path $latestFile)) { Write-Host "No release yet. Run .\build.ps1 -Release -Notes ""..."" first." -ForegroundColor Red; exit 1 }
$latest = Get-Content $latestFile -Raw | ConvertFrom-Json
$src = Join-Path "releases" $latest.version
if (-not (Test-Path (Join-Path $src "Triffall.exe"))) { throw "Release folder $src is missing Triffall.exe." }

$dest = Join-Path $env:LOCALAPPDATA "Programs\Triffall"
if (Get-Process Triffall -ErrorAction SilentlyContinue) { throw "Close Triffall first." }
New-Item -ItemType Directory $dest -Force | Out-Null
Get-ChildItem $dest -Force | Where-Object { $_.Name -ne "saves" } | Remove-Item -Recurse -Force
Copy-Item "$src\*" $dest -Recurse
Set-Content (Join-Path $dest "update-source.txt") (Resolve-Path "releases").Path -Encoding UTF8
Set-Content (Join-Path $dest "version.txt") $latest.version -Encoding UTF8

$lnk = Join-Path ([Environment]::GetFolderPath("Programs")) "Triffall.lnk"
$sh = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$sh.TargetPath = Join-Path $dest "Triffall.exe"
$sh.WorkingDirectory = $dest
$sh.Save()
Write-Host "Installed Triffall v$($latest.version) to $dest" -ForegroundColor Green
