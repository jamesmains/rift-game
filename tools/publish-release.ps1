# Triffall - publish a release to the public GitHub repo in releases-repo.txt (1.7.1, #150; since 1.7.2 the game's own
# public repo, jamesmains/rift-game, #151), so website copies of the game find it
# through their Update button (update-source.txt = https://github.com/<repo>/releases/latest/download/).
#   .\tools\publish-release.ps1                 publish the version in releases\latest.json (build.ps1 -Release runs this)
#   .\tools\publish-release.ps1 -Version 1.7.1  publish an older release again (its zip must still be in releases\)
#   .\tools\publish-release.ps1 -CreateRepo     make the public repo first (once; it only ever holds releases)
#   .\tools\publish-release.ps1 -Check          only say whether publishing would work (sign-in, repo)
# Uploads only two files per release: triffall-v<version>-windows.zip (the game as handed out: exe, assets, readme,
# update-source.txt, assets-manifest.txt; no local campaigns) and latest.json (version, zip, sha256, size, notes).
# Never source code or loose asset/music files (the music licence forbids handing the tracks out on their own).
#
# Sign-in, first that exists (stays on this PC; never in the game or in git):
#   $env:TRIFFALL_GITHUB_TOKEN, $env:GITHUB_TOKEN, the file %APPDATA%\triffall-publish\github-token.txt, or `gh auth token`
#   (the GitHub CLI after `gh auth login`). A fine-grained token needs Contents: read and write on the releases repo
#   (plus Administration: write for -CreateRepo); a classic token needs public_repo.
# Not signed in, or no such repo: a note, and nothing fails (exit 0). An upload that goes wrong prints a red line; the
# local release is fine either way and this script can be run again.
param([string]$Version = "", [string]$Repo = "", [switch]$CreateRepo, [switch]$Check)
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
if (-not $Repo) { $Repo = (Get-Content (Join-Path $root "releases-repo.txt") -Raw).Trim() }
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

function Note($t) { Write-Host "  GitHub releases: $t" -ForegroundColor Yellow }
$token = $env:TRIFFALL_GITHUB_TOKEN
if (-not $token) { $token = $env:GITHUB_TOKEN }
$tokenFile = Join-Path $env:APPDATA "triffall-publish\github-token.txt"
if (-not $token -and (Test-Path $tokenFile)) { $token = (Get-Content $tokenFile -Raw).Trim() }
if (-not $token -and (Get-Command gh -ErrorAction SilentlyContinue)) { $t = (& gh auth token 2>$null); if ($LASTEXITCODE -eq 0 -and $t) { $token = "$t".Trim() } }if (-not $token) { Note "not signed in, so this release wasn't published to $Repo (see tools\publish-release.ps1 to set it up). Nothing else is affected."; exit 0 }
$h = @{ Authorization = "Bearer $token"; Accept = "application/vnd.github+json"; "X-GitHub-Api-Version" = "2022-11-28"; "User-Agent" = "triffall-publish" }
$api = "https://api.github.com"

function Get-Status($err) { try { return [int]$err.Exception.Response.StatusCode } catch { return 0 } }
$repoInfo = $null
try { $repoInfo = Invoke-RestMethod "$api/repos/$Repo" -Headers $h } catch {
    $code = Get-Status $_
    if ($code -eq 401) { Note "the GitHub sign-in was refused (token expired?). Not published."; exit 0 }
    if ($code -ne 404) { Note "couldn't reach GitHub ($($_.Exception.Message)). Not published."; exit 0 }
}
if (-not $repoInfo) {
    if (-not $CreateRepo) { Note "the repo $Repo doesn't exist yet. Run .\tools\publish-release.ps1 -CreateRepo once (it makes it public, with a README). Not published."; exit 0 }
    $owner, $name = $Repo -split '/', 2
    $me = Invoke-RestMethod "$api/user" -Headers $h
    $body = @{ name = $name; description = "Releases of Triffall, an old-school tactical RPG hiding inside a retro PC desktop. Downloads only: the game updates itself from here."; private = $false; auto_init = $true; has_issues = $false; has_wiki = $false; has_projects = $false } | ConvertTo-Json
    $url = if ($me.login -ieq $owner) { "$api/user/repos" } else { "$api/orgs/$owner/repos" }
    $repoInfo = Invoke-RestMethod $url -Method Post -Headers $h -Body $body -ContentType "application/json"
    Write-Host "  Made the public repo $Repo" -ForegroundColor Cyan
    Start-Sleep -Seconds 3
}
if ($repoInfo.private) { Note "$Repo is private: website copies can't download from it. Make it public. Not published."; exit 0 }
if ($Check) { Write-Host "  GitHub releases: signed in, $Repo is ready." -ForegroundColor Green; exit 0 }

# what to upload
$latestFile = Join-Path $root "releases\latest.json"
$latest = Get-Content $latestFile -Raw | ConvertFrom-Json
if (-not $Version) { $Version = $latest.version }
$zip = Join-Path $root "releases\triffall-v$Version-windows.zip"
if (-not (Test-Path $zip)) { Note "releases\triffall-v$Version-windows.zip is missing. Not published."; exit 0 }
$sha = (Get-FileHash $zip -Algorithm SHA256).Hash
if ($latest.version -ne $Version -or $latest.sha256 -ne $sha) {
    # an older release published again: its own latest.json (it only becomes "latest" on GitHub if it's the newest)
    $latest = [ordered]@{ version = $Version; exe = $Version; zip = "triffall-v$Version-windows.zip"; sha256 = $sha; size = (Get-Item $zip).Length; notes = "$($latest.notes)"; released = (Get-Date).ToString("s") }
    if ($Version -ne (Get-Content $latestFile -Raw | ConvertFrom-Json).version) { $latest.notes = "" }
    $latestFile = Join-Path $env:TEMP "triffall-latest-$Version.json"
    [IO.File]::WriteAllText($latestFile, ($latest | ConvertTo-Json), (New-Object Text.UTF8Encoding $false))
}
# the zip must carry the address (else website copies of it would never update)
Add-Type -AssemblyName System.IO.Compression.FileSystem
$z = [IO.Compression.ZipFile]::OpenRead($zip)
try { $hasSource = @($z.Entries | Where-Object { $_.FullName -eq "update-source.txt" }).Count -gt 0 } finally { $z.Dispose() }
if (-not $hasSource) { Write-Host "  GitHub releases: the zip has no update-source.txt (made before 1.7.1?). Publishing it anyway." -ForegroundColor DarkGray }

try {
    $tag = "v$Version"
    $rel = $null
    try { $rel = Invoke-RestMethod "$api/repos/$Repo/releases/tags/$tag" -Headers $h } catch { if ((Get-Status $_) -ne 404) { throw } }
    $notes = "$($latest.notes)"
    if (-not $rel) {
        $body = @{ tag_name = $tag; name = "Triffall $Version"; body = $notes; draft = $false; prerelease = $false; make_latest = "true" } | ConvertTo-Json
        $rel = Invoke-RestMethod "$api/repos/$Repo/releases" -Method Post -Headers $h -Body $body -ContentType "application/json; charset=utf-8"
    }
    $upload = $rel.upload_url -replace '\{.*\}$', ''
    foreach ($f in @(@{ Path = $zip; Name = "triffall-v$Version-windows.zip"; Type = "application/zip" }, @{ Path = $latestFile; Name = "latest.json"; Type = "application/json" })) {
        foreach ($a in @($rel.assets | Where-Object { $_.name -eq $f.Name })) { Invoke-RestMethod "$api/repos/$Repo/releases/assets/$($a.id)" -Method Delete -Headers $h | Out-Null }
        Write-Host "  uploading $($f.Name) ($([math]::Round((Get-Item $f.Path).Length / 1MB, 1)) MB)..."
        Invoke-RestMethod "$($upload)?name=$($f.Name)" -Method Post -Headers $h -InFile $f.Path -ContentType $f.Type -TimeoutSec 3600 | Out-Null
    }
    # check: the address the game reads
    $check = Invoke-RestMethod "https://github.com/$Repo/releases/latest/download/latest.json" -Headers @{ "User-Agent" = "triffall-publish" }
    if ($check.version -eq $Version -and $check.sha256 -eq $sha) { Write-Host "Published Triffall $Version to https://github.com/$Repo/releases/tag/$tag" -ForegroundColor Green }
    else { Write-Host "  Published $tag, but the latest release on GitHub says $($check.version) (a newer one exists?)." -ForegroundColor Yellow }
} catch {
    Write-Host "  GitHub releases: publishing $Version failed: $($_.Exception.Message). The local release is fine; run .\tools\publish-release.ps1 -Version $Version to try again." -ForegroundColor Red
}
exit 0
