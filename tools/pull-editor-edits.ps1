# Triffall - bring changes made with the in-game Editor back into src\assets (1.1.3).
# The Editor saves into the assets\ folder next to the exe it runs from: build\game* (Run) or the installed copy in
# %LOCALAPPDATA%\Programs\Triffall. A rebuild or an update replaces that folder, so without this the owner's edits were
# lost every time. build.ps1 and install.ps1 run this first; it can also be run by hand: .\tools\pull-editor-edits.ps1
#
# How it tells an Editor edit from an older build: every copy has assets-manifest.txt (SHA-256 of each asset file as it
# was copied there). A file whose hash differs from the manifest was changed in that copy:
#   - src\assets still has the manifest's version (or doesn't have the file) -> the edit is copied into src\assets
#   - src\assets already has the same content                               -> nothing to do
#   - src\assets changed it too                                             -> both kept: the copy's version is saved
#     to editor-edits\<time>\<copy>\... and reported, to be merged by hand
# Files deleted in a copy are only reported. Afterwards each copy's manifest is rewritten, so an edit is pulled once.
# A copy without a manifest (installed before 1.1.3) is compared with releases\<its version.txt>\assets instead.
# Output: one object per changed file (Copy, Rel, File, BaseHash, Action = pulled | same | conflict).
# -ManifestFor <dir>: only (re)write <dir>\assets-manifest.txt for the assets\ in it (build.ps1 / install.ps1 use it).
param([switch]$Quiet, [string]$ManifestFor = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$srcAssets = Join-Path $root "src\assets"
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"

function Get-AssetHashes([string]$assetsDir) {
    $h = @{}
    if (-not (Test-Path $assetsDir)) { return $h }
    $base = (Resolve-Path $assetsDir).Path.TrimEnd('\') + '\'
    Get-ChildItem $assetsDir -Recurse -File | Where-Object { $_.FullName.Substring($base.Length) -notlike "new-art\*" } | ForEach-Object {
        $h[$_.FullName.Substring($base.Length)] = (Get-FileHash $_.FullName -Algorithm SHA256).Hash
    }
    return $h
}
function Read-Manifest([string]$path) {
    $h = @{}
    foreach ($line in [System.IO.File]::ReadAllLines($path)) { if ($line -match '^([0-9A-F]{64})  (.+)$') { $h[$Matches[2]] = $Matches[1] } }
    return $h
}
function Write-Manifest([string]$copyDir, $hashes) {
    $lines = $hashes.Keys | Sort-Object | ForEach-Object { "$($hashes[$_])  $_" }
    [System.IO.File]::WriteAllLines((Join-Path (Resolve-Path $copyDir).Path "assets-manifest.txt"), [string[]]$lines)
}
if ($ManifestFor) { Write-Manifest $ManifestFor (Get-AssetHashes (Join-Path $ManifestFor "assets")); return }

$copies = @(
    @{ Name = "build-game";          Dir = Join-Path $root "build\game" },
    @{ Name = "build-game-noeditor"; Dir = Join-Path $root "build\game-noeditor" },
    @{ Name = "build-game-demo";     Dir = Join-Path $root "build\game-demo" },
    @{ Name = "build-game-dev";      Dir = Join-Path $root "build\game-dev" },
    @{ Name = "installed";           Dir = Join-Path $env:LOCALAPPDATA "Programs\Triffall" }
)
$results = @()
foreach ($c in $copies) {
    $assets = Join-Path $c.Dir "assets"
    if (-not (Test-Path $assets)) { continue }
    $manifest = Join-Path $c.Dir "assets-manifest.txt"
    $base = $null
    if (Test-Path $manifest) { $base = Read-Manifest $manifest }
    elseif (Test-Path (Join-Path $c.Dir "version.txt")) {
        $v = (Get-Content (Join-Path $c.Dir "version.txt") -Raw).Trim().TrimStart([char]0xFEFF)
        $rel = Join-Path $root "releases\$v\assets"
        if (Test-Path $rel) { $base = Get-AssetHashes $rel }
    }
    $now = Get-AssetHashes $assets
    if ($null -eq $base) {   # nothing to compare with: start tracking from here
        if (-not $Quiet) { Write-Host "  $($c.Name): no manifest yet, tracking Editor changes from now on" }
        Write-Manifest $c.Dir $now; continue
    }
    foreach ($rel in $now.Keys) {
        $h = $now[$rel]; $b = $base[$rel]
        if ($h -eq $b) { continue }
        $srcFile = Join-Path $srcAssets $rel
        $s = $null; if (Test-Path $srcFile) { $s = (Get-FileHash $srcFile -Algorithm SHA256).Hash }
        $file = Join-Path $assets $rel
        if ($s -eq $h) { $action = "same" }
        elseif ($s -eq $b) {   # src untouched since that copy was made (or a new file): take the edit
            New-Item -ItemType Directory (Split-Path $srcFile -Parent) -Force | Out-Null
            Copy-Item $file $srcFile -Force; $action = "pulled"
            Write-Host "  Editor edit kept: $rel (from $($c.Name))" -ForegroundColor Cyan
        } else {
            $keep = Join-Path $root "editor-edits\$stamp\$($c.Name)\$rel"
            New-Item -ItemType Directory (Split-Path $keep -Parent) -Force | Out-Null
            Copy-Item $file $keep -Force; $action = "conflict"
            Write-Host "  Editor edit to $rel in $($c.Name) clashes with a newer src\assets version: saved to $keep - merge it by hand" -ForegroundColor Yellow
        }
        $results += [pscustomobject]@{ Copy = $c.Name; Rel = $rel; File = $file; BaseHash = $b; Action = $action }
    }
    # 1.7.0 (#145): a whole campaign deleted in that copy (the launcher's or the Editor's Delete) goes from src\assets too,
    # when src still has it exactly as that copy had it (so nothing newer is lost). Never the Tutorial.
    $goneCamps = @($base.Keys | Where-Object { $_ -match '^campaigns\\([^\\]+)\\campaign\.txt$' -and -not $now.ContainsKey($_) } | ForEach-Object { ($_ -split '\\')[1] })
    $srcRoot = (Resolve-Path $srcAssets).Path.TrimEnd('\') + '\'
    $removedCamps = @()
    foreach ($id in $goneCamps) {
        $dir = Join-Path $srcAssets "campaigns\$id"
        if ($id -eq "tutorial" -or -not (Test-Path $dir)) { continue }
        $same = $true
        foreach ($f in Get-ChildItem $dir -Recurse -File) { if ($base[$f.FullName.Substring($srcRoot.Length)] -ne (Get-FileHash $f.FullName -Algorithm SHA256).Hash) { $same = $false; break } }
        if ($same) { Remove-Item $dir -Recurse -Force; $removedCamps += "campaigns" + [char]92 + $id + [char]92; Write-Host "  Campaign $id was deleted in $($c.Name): removed from src\assets too" -ForegroundColor Cyan }
        else { Write-Host "  Campaign $id was deleted in $($c.Name), but src\assets has newer files of it: left in src\assets" -ForegroundColor Yellow }
    }
    foreach ($rel in $base.Keys) { if (-not $now.ContainsKey($rel) -and -not $Quiet -and -not ($removedCamps | Where-Object { $rel.StartsWith($_) })) { Write-Host "  (deleted in $($c.Name), left in src\assets: $rel)" -ForegroundColor DarkGray } }
    Write-Manifest $c.Dir $now
}
$results
