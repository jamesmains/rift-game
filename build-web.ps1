# Triffall 1.3.0 (#44) - the browser demo for itch.io (play in the browser).
#   .\build-web.ps1           build it: build\dist\web (index.html/.js/.wasm/.data) + releases\triffall-v<ver>-web-demo.zip with -Release
#   .\build-web.ps1 -Release  also put the upload zip in releases\
#   .\build-web.ps1 -Serve    build if needed, then serve build\dist\web on http://localhost:8642 and open the browser
# Needs Emscripten (emsdk) and Ninja (both installed on this PC since 1.3.0, emsdk in %USERPROFILE%\emsdk). On another
# PC, one time, in a terminal:
#   git clone https://github.com/emscripten-core/emsdk.git $env:USERPROFILE\emsdk
#   cd $env:USERPROFILE\emsdk; .\emsdk.bat install latest; .\emsdk.bat activate latest
# This script finds emsdk in $env:EMSDK or %USERPROFILE%\emsdk and loads its environment itself.
# The demo variant (TRIFFALL_DEMO, no Editor) with the assets trimmed by src\tools\web_assets.py (~36 MB).
# On itch.io: upload the zip as "HTML", tick "This file will be played in the browser", viewport 960 x 540 (it fills
# any shape), tick "Fullscreen button". Saves live in the player's browser (IndexedDB).
# Fresh runs (-Serve from Idea Jotter): IDEAJOTTER_RUN_DATA set = the browser gets --user-data-dir there, so its
# saves (IndexedDB) start empty.
param([switch]$Release, [switch]$Serve, [int]$Port = 8642)
$ErrorActionPreference = "Continue"
Set-Location $PSScriptRoot
$header = Get-Content "src\src\core\version.h" -Raw
if ($header -notmatch 'TRIFFALL_VERSION "([0-9.]+)"') { throw "TRIFFALL_VERSION not found" }
$version = $Matches[1]
$dist = "build\dist\web"

function Find-Emsdk {
    foreach ($d in @($env:EMSDK, (Join-Path $env:USERPROFILE "emsdk"))) { if ($d -and (Test-Path (Join-Path $d "emsdk_env.ps1"))) { return $d } }
    return $null
}

$newest = Get-ChildItem "src\src", "src\assets", "src\web", "src\CMakeLists.txt" -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$needBuild = -not (Test-Path "$dist\index.html") -or (Get-Item "$dist\index.html").LastWriteTime -lt $newest.LastWriteTime
if (-not $Serve -or $needBuild) {
    $emsdk = Find-Emsdk
    if (-not $emsdk) {
        Write-Host "Emscripten (emsdk) is not installed, so the browser demo can't be built here." -ForegroundColor Red
        Write-Host "Install it once (about 1.5 GB), then run this again:"
        Write-Host "  git clone https://github.com/emscripten-core/emsdk.git `$env:USERPROFILE\emsdk"
        Write-Host "  cd `$env:USERPROFILE\emsdk; .\emsdk.bat install latest; .\emsdk.bat activate latest"
        exit 1
    }
    . (Join-Path $emsdk "emsdk_env.ps1") | Out-Null
    & "$PSScriptRoot\tools\unpack-music.ps1"
    Write-Host "Staging the demo's assets..."
    python src\tools\web_assets.py src\assets build\web-assets\assets
    if ($LASTEXITCODE -ne 0) { Write-Host "Staging the assets failed." -ForegroundColor Red; exit 1 }
    $stage = (Resolve-Path "build\web-assets\assets").Path -replace '\\', '/'
    emcmake cmake -S src -B build\cmake-web -G Ninja -DCMAKE_BUILD_TYPE=Release "-DTRIFFALL_DEMO=ON" "-DTRIFFALL_WEB_ASSETS=$stage"
    if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed (first run downloads raylib; rerun once if it was the download)." -ForegroundColor Red; exit 1 }
    cmake --build build\cmake-web --target Triffall
    if ($LASTEXITCODE -ne 0) { Write-Host "Browser build failed." -ForegroundColor Red; exit 1 }
    if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
    New-Item -ItemType Directory $dist -Force | Out-Null
    foreach ($f in "index.html", "index.js", "index.wasm", "index.data") { Copy-Item "build\cmake-web\$f" $dist }
    Write-Host "Built $dist (v$version browser demo)" -ForegroundColor Green
}

if ($Release) {
    New-Item -ItemType Directory "releases" -Force | Out-Null
    $zip = "releases\triffall-v$version-web-demo.zip"
    if (Test-Path $zip) { Remove-Item $zip -Force }
    Compress-Archive -Path "$dist\*" -DestinationPath $zip
    Write-Host "Released $zip (upload it to itch.io as an HTML game)" -ForegroundColor Green
}

if ($Serve) {
    # a page loaded from file:// can't fetch index.data: serve it over http
    $url = "http://localhost:$Port/"
    $server = Start-Process python -ArgumentList "-m", "http.server", "$Port", "--bind", "127.0.0.1", "--directory", (Resolve-Path $dist).Path -PassThru -WindowStyle Hidden
    Write-Host "Serving the browser demo at $url (server process $($server.Id); stop it from Idea Jotter or end that process)"
    $edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe", "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($env:IDEAJOTTER_RUN_DATA -and $edge) {
        $profile = Join-Path $env:IDEAJOTTER_RUN_DATA "browser"
        Start-Process $edge -ArgumentList "--user-data-dir=`"$profile`"", "--no-first-run", $url
    } else { Start-Process $url }
    Wait-Process -Id $server.Id
}
