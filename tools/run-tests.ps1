# Runs headless tests in parallel, each from a fresh copy of src/assets. Usage: tools/run-tests.ps1 -Tests a,b,c [-Par 8] [-Tag name] [-Root C:/Apps/Triffall]
param([string]$Root = "C:\Apps\Triffall", [string[]]$Tests, [int]$Par = 8, [string]$Tag = "main")
$exe = Join-Path $Root "build\cmake\Release\triffall_headless.exe"
$work = Join-Path $env:TEMP "tri-tests-$Tag"
if (Test-Path $work) { Remove-Item $work -Recurse -Force }
New-Item -ItemType Directory $work | Out-Null
$results = @{}
$queue = [System.Collections.Queue]::new(); foreach ($t in $Tests) { $queue.Enqueue($t) }
$running = @()
while ($queue.Count -gt 0 -or $running.Count -gt 0) {
    while ($running.Count -lt $Par -and $queue.Count -gt 0) {
        $t = $queue.Dequeue()
        $dir = Join-Path $work $t; New-Item -ItemType Directory $dir | Out-Null
        Copy-Item (Join-Path $Root "src\assets") (Join-Path $dir "assets") -Recurse
        $out = Join-Path $dir "out"; New-Item -ItemType Directory $out | Out-Null
        $p = Start-Process -FilePath $exe -ArgumentList @((Join-Path $Root "src\tests\$t.txt"), "$out\") -WorkingDirectory $dir -RedirectStandardOutput (Join-Path $dir "stdout.txt") -RedirectStandardError (Join-Path $dir "stderr.txt") -NoNewWindow -PassThru
        $running += [pscustomobject]@{ Name = $t; Proc = $p; Dir = $dir; Start = Get-Date }
    }
    Start-Sleep -Milliseconds 500
    $still = @()
    foreach ($r in $running) {
        if ($r.Proc.HasExited) {
            $o = Get-Content (Join-Path $r.Dir "stdout.txt") -ErrorAction SilentlyContinue
            $fails = @($o | Where-Object { $_ -cmatch "EXPECT FAIL|BOT FAIL|CRASH|Assertion" })
            $secs = [int]((Get-Date) - $r.Start).TotalSeconds
            if ($fails.Count -gt 0) { Write-Output "FAIL $($r.Name) (${secs}s)"; $fails | Select-Object -First 8 | ForEach-Object { Write-Output "    $_" } }
            else { Write-Output "ok   $($r.Name) (${secs}s)" }
        } elseif (((Get-Date) - $r.Start).TotalMinutes -gt 20) { $r.Proc.Kill(); Write-Output "TIMEOUT $($r.Name)" }
        else { $still += $r }
    }
    $running = $still
}
