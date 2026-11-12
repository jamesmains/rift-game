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
# 1.1.3: Editor edits made in the installed copy are kept. They go into src\assets (so the next build has them), and
# any the new release doesn't have yet are put back after the copy - unless the release changed that file itself.
$edits = @(& "$PSScriptRoot\tools\pull-editor-edits.ps1" | Where-Object { $_.Copy -eq "installed" })
$stash = Join-Path $env:TEMP "triffall-editor-edits"
if (Test-Path $stash) { Remove-Item $stash -Recurse -Force }
foreach ($e in $edits) { $to = Join-Path $stash $e.Rel; New-Item -ItemType Directory (Split-Path $to -Parent) -Force | Out-Null; Copy-Item $e.File $to }
Get-ChildItem $dest -Force | Where-Object { $_.Name -ne "saves" } | Remove-Item -Recurse -Force
Copy-Item "$src\*" $dest -Recurse
$restored = 0
foreach ($e in $edits) {
    $inst = Join-Path $dest "assets\$($e.Rel)"; $mine = Join-Path $stash $e.Rel
    $now = $null; if (Test-Path $inst) { $now = (Get-FileHash $inst -Algorithm SHA256).Hash }
    if ($now -eq (Get-FileHash $mine -Algorithm SHA256).Hash) { continue }   # the release already has it
    if ($null -eq $now -or $now -eq $e.BaseHash) { New-Item -ItemType Directory (Split-Path $inst -Parent) -Force | Out-Null; Copy-Item $mine $inst -Force; $restored++ }
    else { Write-Host "  The new release changed $($e.Rel) too; it replaces your Editor version (that one is in src\assets or editor-edits\)." -ForegroundColor Yellow }
}
if (Test-Path $stash) { Remove-Item $stash -Recurse -Force }
if ($restored) { & "$PSScriptRoot\tools\pull-editor-edits.ps1" -ManifestFor $dest; Write-Host "Kept $restored Editor edit(s) made in the installed copy." -ForegroundColor Cyan }
Set-Content (Join-Path $dest "update-source.txt") (Resolve-Path "releases").Path -Encoding UTF8
Set-Content (Join-Path $dest "version.txt") $latest.version -Encoding UTF8

$lnk = Join-Path ([Environment]::GetFolderPath("Programs")) "Triffall.lnk"
$sh = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
$sh.TargetPath = Join-Path $dest "Triffall.exe"
$sh.WorkingDirectory = $dest
$sh.Save()
Write-Host "Installed Triffall v$($latest.version) to $dest" -ForegroundColor Green
