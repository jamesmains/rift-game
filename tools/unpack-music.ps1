# Triffall 1.3.0 - put the game's music (the Pixel Fantasy pack by AlkaKrab) into src\assets\audio\music\pixel_fantasy.
# The pack's license allows it in the game but not redistributing the files as-is, so they are not in git: they come
# from the purchased zip in assets\packs\ (also not in git). build.ps1 runs this; it does nothing when all files are there.
# File names: <nn>_<title>.ogg from the pack's Loops folder (made to loop), fx_1..3.ogg from its Tracks folder (stings).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root "src\assets\audio\music\pixel_fantasy"
$zip = Join-Path $root "assets\packs\Pixel Fantasy 30 Tracks and Loops Music Pack.zip"
if ((Test-Path $out) -and @(Get-ChildItem $out -Filter *.ogg).Count -ge 33) { return }
if (-not (Test-Path $zip)) { Write-Host "Music pack not found ($zip): the game will run without music." -ForegroundColor Yellow; return }
Add-Type -AssemblyName System.IO.Compression.FileSystem
New-Item -ItemType Directory $out -Force | Out-Null
$z = [System.IO.Compression.ZipFile]::OpenRead($zip)
try {
    foreach ($e in $z.Entries) {
        $name = $null
        if ($e.FullName -match '/Loops/(\d+)\. (.+)\.ogg$') {
            $slug = ($Matches[2].ToLower() -replace '[^a-z0-9]+', '_').Trim('_')
            $name = "{0:D2}_{1}.ogg" -f [int]$Matches[1], $slug
        } elseif ($e.FullName -match '/Tracks/Fx (\d)\.ogg$') { $name = "fx_$($Matches[1]).ogg" }
        if ($name) { [System.IO.Compression.ZipFileExtensions]::ExtractToFile($e, (Join-Path $out $name), $true) }
    }
} finally { $z.Dispose() }
Write-Host "Unpacked the music into $out" -ForegroundColor Cyan
