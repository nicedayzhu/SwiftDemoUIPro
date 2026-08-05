param(
    [string]$QtRoot = "",
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [switch]$Package
)

$ErrorActionPreference = "Stop"

$launcherRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $launcherRoot
$buildDir = Join-Path $launcherRoot "build"
$packageDir = Join-Path $launcherRoot "package\SwiftDemoUIPro"

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

$vpk = Join-Path $projectRoot "dist\swift_demo_menu_override.vpk"
if (-not (Test-Path -LiteralPath $vpk)) {
    throw "Missing menu VPK. Run ..\demo-menu.ps1 first."
}

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
    Copy-Item -LiteralPath $vpk -Destination (Join-Path $packageDir "swift_demo_menu_override.vpk")
    Copy-Item -LiteralPath (Join-Path $launcherRoot "README.md") -Destination (Join-Path $packageDir "README.txt")
    Copy-Item -LiteralPath (Join-Path $launcherRoot "THIRD_PARTY_NOTICES.txt") -Destination (Join-Path $packageDir "THIRD_PARTY_NOTICES.txt")
    $appTranslations = Join-Path $launcherExe.DirectoryName "translations"
    if (Test-Path -LiteralPath $appTranslations) {
        Copy-Item -LiteralPath $appTranslations -Destination (Join-Path $packageDir "translations") -Recurse
    }

    $deploy = Join-Path $qt "bin\windeployqt.exe"
    if (-not (Test-Path -LiteralPath $deploy)) { throw "windeployqt.exe was not found in $qt\bin" }
    & $deploy --release --compiler-runtime --no-translations --no-opengl-sw --no-system-dxc-compiler `
        --skip-plugin-types generic,imageformats,networkinformation,tls,styles `
        (Join-Path $packageDir "SwiftDemoUIPro.exe")
    if ($LASTEXITCODE -ne 0) { throw "windeployqt failed." }

    $licenseDir = Join-Path $packageDir "licenses"
    New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null
    $fontLicense = Join-Path $launcherRoot "assets\fonts\OFL-1.1.txt"
    Copy-Item -LiteralPath $fontLicense -Destination (Join-Path $licenseDir "NotoSansSC-OFL-1.1.txt")

    $qtLicense = Join-Path $launcherRoot "licenses\Qt-LGPL-3.0-only.txt"
    Copy-Item -LiteralPath $qtLicense -Destination (Join-Path $licenseDir "Qt-LGPL-3.0.txt")

    $zipPath = Join-Path $launcherRoot "package\SwiftDemoUIPro-win64.zip"
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Compress-Archive -LiteralPath $packageDir -DestinationPath $zipPath -CompressionLevel Optimal
    Write-Host "Packaged launcher: $zipPath"
}

Write-Host "Swift DemoUI Pro build complete."
