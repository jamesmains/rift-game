# Pacing runs of the main quest with the playthrough bot (PowerShell twin of src\tools\pacing.sh, for Windows).
#   .\tools\pacing.ps1 [-Class warrior] [-Seeds 1,2,3]
# For each seed: a god-mode run (always finishes: the level / gold / kills / time curve) and a survival run (no god mode:
# where, if anywhere, the party is wiped). Runs in parallel from a temp copy of the assets; prints the checkpoints.
# 1.5.0: there is one difficulty, so the route's difficulty number is ignored by the game.
param([string]$Class = "warrior", [int[]]$Seeds = @(1, 2, 3))
$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "build\cmake\Release\triffall_headless.exe"
if (-not (Test-Path $exe)) { Write-Host "Build first (.\build.ps1)." -ForegroundColor Red; exit 1 }
$work = Join-Path $env:TEMP "triffall-pacing"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory $work | Out-Null
Copy-Item (Join-Path $root "src\assets") (Join-Path $work "assets") -Recurse
$route = Get-Content (Join-Path $root "src\tools\pacing_route.txt")
$procs = @()
foreach ($s in $Seeds) {
    foreach ($mode in "god", "survival") {
        $d = Join-Path $work "$s-$mode"; New-Item -ItemType Directory $d | Out-Null
        $g = if ($mode -eq "god") { "god" } else { "#" }
        $route | ForEach-Object { $_ -replace 'CLASS', $Class -replace 'DIFF', '2' -replace 'SEED', "$s" -replace '^GODLINE$', $g } | Set-Content (Join-Path $d "route.txt")
        $procs += [pscustomobject]@{ Name = "$s-$mode"; Dir = $d; P = (Start-Process $exe -ArgumentList @((Join-Path $d "route.txt"), "$d\") -WorkingDirectory $work -RedirectStandardOutput (Join-Path $d "out.txt") -NoNewWindow -PassThru) }
    }
}
foreach ($p in $procs) { $p.P.WaitForExit() }
foreach ($p in $procs) {
    Write-Host "== seed $($p.Name)"
    Get-Content (Join-Path $p.Dir "out.txt") | Where-Object { $_ -match "CHECKPOINT|BOT FAIL|member " } | ForEach-Object { Write-Host $_ }
}
