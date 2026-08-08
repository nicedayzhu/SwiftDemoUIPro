param(
    [string]$QtRoot = "",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Package,
    [switch]$SkipVpkCheck
)

$ErrorActionPreference = "Stop"

$launcherRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $launcherRoot
$buildDir = Join-Path $launcherRoot "build"
$versionPath = Join-Path $projectRoot "VERSION"
$version = (Get-Content -Raw -LiteralPath $versionPath).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION must contain a semantic version such as 1.2.3."
}
$packageDir = Join-Path $launcherRoot "package\SwiftDemoUIPro-v$version"

function Resolve-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @(
        "C:\Qt\Tools\CMake_64\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) { return $candidate }
    }
    throw "CMake was not found. Install the Qt build tools or Visual Studio C++ CMake tools."
}

function Resolve-QtRoot {
    param([string]$RequestedRoot)
    if ($RequestedRoot) {
        $resolved = (Resolve-Path -LiteralPath $RequestedRoot).Path
        if (Test-Path -LiteralPath (Join-Path $resolved "lib\cmake\Qt6\Qt6Config.cmake")) { return $resolved }
        throw "QtRoot does not contain a Qt 6 desktop kit: $resolved"
    }

    if ($env:CMAKE_PREFIX_PATH) {
        foreach ($prefix in ($env:CMAKE_PREFIX_PATH -split ';')) {
            if (Test-Path -LiteralPath (Join-Path $prefix "lib\cmake\Qt6\Qt6Config.cmake")) { return $prefix }
        }
    }

    if (Test-Path -LiteralPath "C:\Qt") {
        $config = Get-ChildItem -LiteralPath "C:\Qt" -Recurse -Filter "Qt6Config.cmake" -ErrorAction SilentlyContinue |
            Where-Object { $_.FullName -match '\\(msvc|mingw)[^\\]*\\lib\\cmake\\Qt6\\Qt6Config\.cmake$' } |
            Sort-Object FullName -Descending |
            Select-Object -First 1
        if ($config) {
            return (Resolve-Path -LiteralPath (Join-Path $config.Directory.FullName "..\..\..")).Path
        }
    }
    throw "Qt 6 desktop kit was not found. Pass -QtRoot C:\Qt\<version>\msvc2022_64."
}

function Resolve-Cargo {
    $command = Get-Command cargo -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidate = Join-Path $env:USERPROFILE ".cargo\bin\cargo.exe"
    if (Test-Path -LiteralPath $candidate) { return $candidate }
    throw "Rust/Cargo was not found. Install the Rust stable toolchain to build the Demo voice indexer."
}

$vpk = Join-Path $projectRoot "dist\swift_demo_menu_override.vpk"
if (-not (Test-Path -LiteralPath $vpk) -and ($Package -or -not $SkipVpkCheck)) {
    throw "Missing menu VPK. Run ..\demo-menu.ps1 first."
}

$cargo = Resolve-Cargo
$voiceIndexerManifest = Join-Path $projectRoot "tools\voice-indexer\Cargo.toml"
& $cargo test --locked --manifest-path $voiceIndexerManifest
if ($LASTEXITCODE -ne 0) { throw "Demo voice indexer tests failed." }
& $cargo build --release --locked --manifest-path $voiceIndexerManifest
if ($LASTEXITCODE -ne 0) { throw "Demo voice indexer build failed." }
$voiceIndexer = Join-Path $projectRoot "tools\voice-indexer\target\release\swift-demo-voice-indexer.exe"
if (-not (Test-Path -LiteralPath $voiceIndexer)) { throw "Built Demo voice indexer was not found." }

$cmake = Resolve-CMake
$qt = Resolve-QtRoot -RequestedRoot $QtRoot
$env:PATH = "$(Join-Path $qt 'bin');$env:PATH"

& $cmake -S $launcherRoot -B $buildDir "-DCMAKE_PREFIX_PATH=$qt" -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }

& $cmake --build $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { throw "Launcher build failed." }

$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"
& $ctest --test-dir $buildDir -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Launcher tests failed." }

if ($Package) {
    if (Test-Path -LiteralPath $packageDir) {
        Remove-Item -LiteralPath $packageDir -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

    $launcherExe = Get-ChildItem -LiteralPath $buildDir -Recurse -Filter "SwiftDemoUIPro.exe" |
        Where-Object { $_.FullName -notmatch '\\CMakeFiles\\' } |
        Select-Object -First 1
    if (-not $launcherExe) { throw "Built launcher executable was not found." }

    Copy-Item -LiteralPath $launcherExe.FullName -Destination (Join-Path $packageDir "SwiftDemoUIPro.exe")
    Copy-Item -LiteralPath $voiceIndexer -Destination (Join-Path $packageDir "swift-demo-voice-indexer.exe")
    Copy-Item -LiteralPath $vpk -Destination (Join-Path $packageDir "swift_demo_menu_override.vpk")
    Copy-Item -LiteralPath (Join-Path $launcherRoot "README.md") -Destination (Join-Path $packageDir "README.txt")
    Copy-Item -LiteralPath (Join-Path $launcherRoot "README_CN.md") -Destination (Join-Path $packageDir "README_CN.txt")
    Copy-Item -LiteralPath (Join-Path $projectRoot "LICENSE") -Destination (Join-Path $packageDir "LICENSE.txt")
    Copy-Item -LiteralPath (Join-Path $launcherRoot "THIRD_PARTY_NOTICES.txt") -Destination (Join-Path $packageDir "THIRD_PARTY_NOTICES.txt")
    $appTranslations = Join-Path $launcherExe.DirectoryName "translations"
    if (Test-Path -LiteralPath $appTranslations) {
        Copy-Item -LiteralPath $appTranslations -Destination (Join-Path $packageDir "translations") -Recurse
    }

    $deploy = Join-Path $qt "bin\windeployqt.exe"
    if (-not (Test-Path -LiteralPath $deploy)) { throw "windeployqt.exe was not found in $qt\bin" }
    & $deploy --release --compiler-runtime --no-translations --no-opengl-sw --no-system-dxc-compiler `
        --skip-plugin-types generic,imageformats,networkinformation,styles `
        (Join-Path $packageDir "SwiftDemoUIPro.exe")
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

    $licenseDir = Join-Path $packageDir "licenses"
    New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null
    $fontLicense = Join-Path $launcherRoot "assets\fonts\OFL-1.1.txt"
    Copy-Item -LiteralPath $fontLicense -Destination (Join-Path $licenseDir "NotoSansSC-OFL-1.1.txt")

    $qtLicense = Join-Path $launcherRoot "licenses\Qt-LGPL-3.0-only.txt"
    Copy-Item -LiteralPath $qtLicense -Destination (Join-Path $licenseDir "Qt-LGPL-3.0.txt")

    $minizLicense = Join-Path $launcherRoot "third_party\miniz\LICENSE"
    Copy-Item -LiteralPath $minizLicense -Destination (Join-Path $licenseDir "miniz-MIT.txt")

    $source2DemoLicense = Join-Path $projectRoot "tools\voice-indexer\SOURCE2_DEMO_LICENSE-MIT.txt"
    Copy-Item -LiteralPath $source2DemoLicense -Destination (Join-Path $licenseDir "source2-demo-MIT.txt")

    $zstdRsLicense = Join-Path $projectRoot "tools\voice-indexer\ZSTD_RS_LICENSE-MIT.txt"
    Copy-Item -LiteralPath $zstdRsLicense -Destination (Join-Path $licenseDir "zstd-rs-MIT.txt")

    $zstdLicense = Join-Path $projectRoot "tools\voice-indexer\ZSTD_LICENSE-BSD-3-Clause.txt"
    Copy-Item -LiteralPath $zstdLicense -Destination (Join-Path $licenseDir "zstd-BSD-3-Clause.txt")

    $zipPath = Join-Path $launcherRoot "package\SwiftDemoUIPro-v$version-win64.zip"
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Packaged launcher: $zipPath"
}

Write-Host "Swift DemoUI Pro build complete."
