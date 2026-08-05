param(
    [string]$Version = "",
    [string]$Cs2Root = "",
    [string]$VpkEditCli = "",
    [string]$QtRoot = "",
    [switch]$Publish
)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$versionPath = Join-Path $projectRoot "VERSION"
$releaseRoot = Join-Path $projectRoot "release"

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

$targetVersion = if ($Version) { $Version.Trim() } else { $currentVersion }
if ($targetVersion -notmatch '^\d+\.\d+\.\d+$') {
    throw "-Version must use MAJOR.MINOR.PATCH, for example 1.2.3."
}
if (-not $Publish -and $targetVersion -ne $currentVersion) {
    throw "Changing VERSION creates a release commit and therefore requires -Publish."
}

if ($Publish) {
    if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
        throw "GitHub CLI (gh) was not found on PATH."
    }

    Invoke-Native -Command "git" -Arguments @("remote", "get-url", "origin") `
        -FailureMessage "Git remote 'origin' is missing. Create the GitHub repository and add origin first."
    Invoke-Native -Command "gh" -Arguments @("auth", "status") `
        -FailureMessage "GitHub CLI is not authenticated. Run: gh auth login"

    if ($targetVersion -ne $currentVersion) {
        [System.IO.File]::WriteAllText(
            $versionPath,
            "$targetVersion`n",
            [System.Text.UTF8Encoding]::new($false)
        )
        Invoke-Native -Command "git" -Arguments @("add", "--", "VERSION") `
            -FailureMessage "Unable to stage VERSION."
        Invoke-Native -Command "git" -Arguments @("commit", "-m", "chore(release): v$targetVersion") `
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

$packageName = "SwiftDemoUIPro-v$targetVersion-win64.zip"
$packageSource = Join-Path $projectRoot "launcher\package\$packageName"
if (-not (Test-Path -LiteralPath $packageSource)) {
    throw "Expected launcher package was not produced: $packageSource"
}
$packageAsset = Join-Path $versionReleaseDir $packageName
Copy-Item -LiteralPath $packageSource -Destination $packageAsset

$sourceName = "SwiftDemoUIPro-v$targetVersion-source.zip"
$sourceAsset = Join-Path $versionReleaseDir $sourceName
Invoke-Native -Command "git" -Arguments @(
    "archive",
    "--format=zip",
    "--prefix=SwiftDemoUIPro-v$targetVersion/",
    "--output=$sourceAsset",
    "HEAD"
) -FailureMessage "Unable to create the source archive."

$checksumPath = Join-Path $versionReleaseDir "SHA256SUMS.txt"
$releaseAssets = @($packageAsset, $sourceAsset)
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
Write-Host "Version: $targetVersion"
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
