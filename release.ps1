param(
    [string]$Version = "",
    [string]$MenuVersion = "",
    [string]$Cs2Root = "",
    [string]$VpkEditCli = "",
    [string]$QtRoot = "",
    [switch]$MenuOnly,
    [switch]$Publish
)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$versionPath = Join-Path $projectRoot "VERSION"
$menuVersionPath = Join-Path $projectRoot "MENU_VERSION"
$releaseRoot = Join-Path $projectRoot "release"
$githubRepository = "nicedayzhu/SwiftDemoUIPro"

function Invoke-Native {
    param(
        [string]$Command,
        [string[]]$Arguments,
        [string]$FailureMessage
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw $FailureMessage
    }
}

foreach ($commandName in @("git", "node")) {
    if (-not (Get-Command $commandName -ErrorAction SilentlyContinue)) {
        throw "$commandName was not found on PATH."
    }
}

Invoke-Native -Command "git" -Arguments @("rev-parse", "--is-inside-work-tree") `
    -FailureMessage "This command must run inside the SwiftDemoUIPro Git repository."

$workingTree = (& git status --porcelain --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw "Unable to inspect the Git working tree." }
if ($workingTree) {
    throw "The Git working tree is not clean. Commit or stash changes before creating a release."
}

$currentVersion = (Get-Content -Raw -LiteralPath $versionPath).Trim()
if ($currentVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION must contain a semantic version such as 1.2.3."
}
$currentMenuVersion = (Get-Content -Raw -LiteralPath $menuVersionPath).Trim()
if ($currentMenuVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "MENU_VERSION must contain a semantic version such as 1.2.3."
}
if ($MenuOnly -or $MenuVersion) {
    throw "Component-only releases are no longer supported. Use -Version; VERSION and MENU_VERSION are published together in a complete Release."
}

$targetVersion = if ($Version) { $Version.Trim() } else { $currentVersion }
$targetMenuVersion = $targetVersion
if ($targetVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "-Version must use MAJOR.MINOR.PATCH, for example 1.2.3."
}
if (-not $Publish -and ($targetVersion -ne $currentVersion -or $targetMenuVersion -ne $currentMenuVersion)) {
    throw "VERSION and MENU_VERSION must match for a local candidate. Use -Version with -Publish to update both files."
}

if ($Publish) {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        throw "GitHub CLI (gh) was not found on PATH."
    }

    Invoke-Native -Command "git" -Arguments @("remote", "get-url", "origin") `
        -FailureMessage "Git remote 'origin' is missing. Create the GitHub repository and add origin first."
    Invoke-Native -Command "gh" -Arguments @("auth", "status") `
        -FailureMessage "GitHub CLI is not authenticated. Run: gh auth login"

    $versionFilesChanged = @()
    if ($targetVersion -ne $currentVersion) {
        [System.IO.File]::WriteAllText(
            $versionPath,
            "$targetVersion`n",
            [System.Text.UTF8Encoding]::new($false)
        )
        $versionFilesChanged += "VERSION"
    }
    if ($targetMenuVersion -ne $currentMenuVersion) {
        [System.IO.File]::WriteAllText(
            $menuVersionPath,
            "$targetMenuVersion`n",
            [System.Text.UTF8Encoding]::new($false)
        )
        $versionFilesChanged += "MENU_VERSION"
    }
    if ($versionFilesChanged.Count -gt 0) {
        Invoke-Native -Command "git" -Arguments (@("add", "--") + $versionFilesChanged) `
            -FailureMessage "Unable to stage release version files."
        $releaseCommitMessage = "chore(release): v$targetVersion"
        Invoke-Native -Command "git" -Arguments @("commit", "-m", $releaseCommitMessage) `
            -FailureMessage "Unable to create the version commit."
    }
}

Write-Host "Testing Panorama logic..."
Invoke-Native -Command "node" -Arguments @((Join-Path $projectRoot "tests\test_demo_voice_mask.js")) `
    -FailureMessage "Panorama tests failed."

Write-Host "Building the Panorama VPK..."
$menuArguments = @{ Action = "Build" }
if ($Cs2Root) { $menuArguments.Cs2Root = $Cs2Root }
if ($VpkEditCli) { $menuArguments.VpkEditCli = $VpkEditCli }
& (Join-Path $projectRoot "demo-menu.ps1") @menuArguments

Write-Host "Building, testing, and packaging the Qt launcher..."
$launcherArguments = @{ Package = $true }
if ($QtRoot) { $launcherArguments.QtRoot = $QtRoot }
& (Join-Path $projectRoot "launcher\build-launcher.ps1") @launcherArguments

$tag = "v$targetVersion"
$versionReleaseDir = Join-Path $releaseRoot $tag
if (Test-Path -LiteralPath $versionReleaseDir) {
    Remove-Item -LiteralPath $versionReleaseDir -Recurse -Force
}
New-Item -ItemType Directory -Path $versionReleaseDir -Force | Out-Null

$releaseAssets = @()
$packageName = "SwiftDemoUIPro-v$targetVersion-win64.zip"
$packageAsset = Join-Path $versionReleaseDir $packageName
$packageSource = Join-Path $projectRoot "launcher\package\$packageName"
if (-not (Test-Path -LiteralPath $packageSource)) {
    throw "Expected launcher package was not produced: $packageSource"
}
Copy-Item -LiteralPath $packageSource -Destination $packageAsset
$releaseAssets += $packageAsset

$sourceName = "SwiftDemoUIPro-v$targetVersion-source.zip"
$sourceAsset = Join-Path $versionReleaseDir $sourceName
Invoke-Native -Command "git" -Arguments @(
    "archive",
    "--format=zip",
    "--prefix=SwiftDemoUIPro-v$targetVersion/",
    "--output=$sourceAsset",
    "HEAD"
) -FailureMessage "Unable to create the source archive."
$releaseAssets += $sourceAsset

$menuName = "swift_demo_menu_override-v$targetMenuVersion.vpk"
$menuAsset = Join-Path $versionReleaseDir $menuName
Copy-Item -LiteralPath (Join-Path $projectRoot "dist\swift_demo_menu_override.vpk") -Destination $menuAsset
$releaseAssets += $menuAsset

$launcherHash = (Get-FileHash -LiteralPath $packageAsset -Algorithm SHA256).Hash.ToLowerInvariant()
$menuHash = (Get-FileHash -LiteralPath $menuAsset -Algorithm SHA256).Hash.ToLowerInvariant()
$launcherUrl = "https://github.com/$githubRepository/releases/download/v$targetVersion/$packageName"
$menuUrl = "https://github.com/$githubRepository/releases/download/$tag/$menuName"
$manifest = [ordered]@{
    schemaVersion = 1
    launcher = [ordered]@{
        version = $targetVersion
        url = $launcherUrl
        sha256 = $launcherHash
    }
    menu = [ordered]@{
        version = $targetMenuVersion
        url = $menuUrl
        sha256 = $menuHash
    }
}
$manifestPath = Join-Path $versionReleaseDir "update-manifest.json"
[System.IO.File]::WriteAllText(
    $manifestPath,
    (($manifest | ConvertTo-Json -Depth 4) + "`n"),
    [System.Text.UTF8Encoding]::new($false)
)
$releaseAssets += $manifestPath

$checksumPath = Join-Path $versionReleaseDir "SHA256SUMS.txt"
$checksumLines = foreach ($asset in $releaseAssets) {
    $hash = (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([System.IO.Path]::GetFileName($asset))"
}
[System.IO.File]::WriteAllLines(
    $checksumPath,
    $checksumLines,
    [System.Text.UTF8Encoding]::new($false)
)
$releaseAssets += $checksumPath

$commit = (& git rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Unable to read the release commit." }

Write-Host "Release candidate ready: $versionReleaseDir"
Write-Host "Launcher version: $targetVersion"
Write-Host "DemoUI version: $targetMenuVersion"
Write-Host "Commit: $commit"
Get-Content -LiteralPath $checksumPath | ForEach-Object { Write-Host $_ }

if (-not $Publish) {
    Write-Host "Local build only. Re-run with -Publish to create tag $tag and a GitHub Release."
    exit 0
}

$branch = (& git branch --show-current).Trim()
if ($LASTEXITCODE -ne 0 -or -not $branch) {
    throw "Publishing from a detached HEAD is not supported."
}

$existingTagCommit = (& git rev-list -n 1 $tag 2>$null)
$tagExists = $LASTEXITCODE -eq 0
if ($tagExists) {
    if ($existingTagCommit.Trim() -ne $commit) {
        throw "Tag $tag already points to another commit."
    }
} else {
    Invoke-Native -Command "git" -Arguments @("tag", "-a", $tag, "-m", "Swift DemoUI Pro $tag") `
        -FailureMessage "Unable to create tag $tag."
}

Invoke-Native -Command "git" -Arguments @("push", "origin", $branch) `
    -FailureMessage "Unable to push branch $branch."
Invoke-Native -Command "git" -Arguments @("push", "origin", $tag) `
    -FailureMessage "Unable to push tag $tag."

$releaseJson = (& gh release view $tag --json isDraft 2>$null)
$releaseExists = $LASTEXITCODE -eq 0
if ($releaseExists) {
    $githubRelease = $releaseJson | ConvertFrom-Json
    if (-not $githubRelease.isDraft) {
        throw "GitHub Release $tag is already published; existing assets were not changed."
    }
    Invoke-Native -Command "gh" -Arguments (@("release", "upload", $tag) + $releaseAssets + @("--clobber")) `
        -FailureMessage "Unable to upload assets to draft release $tag."
} else {
    Invoke-Native -Command "gh" -Arguments (@(
        "release", "create", $tag
    ) + $releaseAssets + @(
        "--draft",
        "--verify-tag",
        "--generate-notes",
        "--title", "Swift DemoUI Pro $tag"
    )) -FailureMessage "Unable to create draft release $tag."
}

Invoke-Native -Command "gh" -Arguments @("release", "edit", $tag, "--draft=false") `
    -FailureMessage "Assets were uploaded, but release $tag could not be published."
Write-Host "Published GitHub Release: $tag"
