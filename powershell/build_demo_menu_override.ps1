param(
	[string]$ProjectRoot = "",
	[string]$Cs2Root = "F:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive",
	[string]$AddonName = "swift_demo_menu_override"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "lib\SwiftMenu.Common.ps1")

$root = Resolve-SwiftMenuProjectRoot -ProjectRoot $ProjectRoot -ScriptDirectory $scriptDir
Assert-SwiftMenuSafeAddonName -Name $AddonName

$resourceCompiler = Join-Path $Cs2Root "game\bin\win64\resourcecompiler.exe"
$gameDir = Join-Path $Cs2Root "game\csgo"
$contentAddonsRoot = Join-Path $Cs2Root "content\csgo_addons"
$gameAddonsRoot = Join-Path $Cs2Root "game\csgo_addons"
$contentAddon = Join-Path $contentAddonsRoot $AddonName
$gameAddon = Join-Path $gameAddonsRoot $AddonName

Assert-SwiftMenuFileExists -Path $resourceCompiler -Message "resourcecompiler.exe not found: $resourceCompiler"

New-Item -ItemType Directory -Force -Path $contentAddonsRoot, $gameAddonsRoot | Out-Null
Remove-SwiftMenuDirectoryIfChild -Path $contentAddon -ExpectedParent $contentAddonsRoot
Remove-SwiftMenuDirectoryIfChild -Path $gameAddon -ExpectedParent $gameAddonsRoot

$layoutDir = Join-Path $contentAddon "panorama\layout\hud"
$styleDir = Join-Path $contentAddon "panorama\styles\hud"
$panoramaScriptDir = Join-Path $contentAddon "panorama\scripts\hud"
New-Item -ItemType Directory -Force -Path $layoutDir, $styleDir, $panoramaScriptDir | Out-Null
Write-SwiftMenuPanoramaPreprocessorConfig -ContentAddon $contentAddon

Write-SwiftMenuTextNoBom -Path (Join-Path $contentAddon "addoninfo.txt") -Value @'
<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{
	IsPlayable = false
}
'@

$sourceLayout = Join-Path $root "addon\panorama\layout\hud\huddemocontroller.xml"
$sourceStyle = Join-Path $root "addon\panorama\styles\hud\swift_demo_voice.css"
$sourceScript = Join-Path $root "addon\panorama\scripts\hud\swift_demo_voice.js"
$sourceLocalizationDir = Join-Path $root "addon\resource"
$localizationFiles = @("platform_english.txt", "platform_schinese.txt")

Assert-SwiftMenuFileExists -Path $sourceLayout -Message "Missing demo HUD layout: $sourceLayout"
Assert-SwiftMenuFileExists -Path $sourceStyle -Message "Missing demo voice style: $sourceStyle"
Assert-SwiftMenuFileExists -Path $sourceScript -Message "Missing demo voice script: $sourceScript"
foreach ($localizationFile in $localizationFiles) {
	$sourceLocalization = Join-Path $sourceLocalizationDir $localizationFile
	Assert-SwiftMenuFileExists -Path $sourceLocalization -Message "Missing Panorama localization file: $sourceLocalization"
}

$layoutInput = Join-Path $layoutDir "huddemocontroller.vxml"
$styleInput = Join-Path $styleDir "swift_demo_voice.vcss"
$scriptInput = Join-Path $panoramaScriptDir "swift_demo_voice.vjs"

Write-SwiftMenuTextNoBom -Path $layoutInput -Value (Get-Content -Raw -LiteralPath $sourceLayout)
Write-SwiftMenuTextNoBom -Path $styleInput -Value (Get-Content -Raw -LiteralPath $sourceStyle)
Write-SwiftMenuTextNoBom -Path $scriptInput -Value (Get-Content -Raw -LiteralPath $sourceScript)

$compileInputs = @($scriptInput, $styleInput, $layoutInput)
$expectedOutputs = @(
	(Join-Path $gameAddon "panorama\layout\hud\huddemocontroller.vxml_c"),
	(Join-Path $gameAddon "panorama\styles\hud\swift_demo_voice.vcss_c"),
	(Join-Path $gameAddon "panorama\scripts\hud\swift_demo_voice.vjs_c")
)

Invoke-SwiftMenuResourceCompiler -ResourceCompiler $resourceCompiler -GameDir $gameDir -Inputs $compileInputs
Assert-SwiftMenuCompiledOutputs -Paths $expectedOutputs

# CS2 loads platform_<language>.txt from the resource directory. These are
# runtime text assets, so copy them directly instead of compiling them.
$gameLocalizationDir = Join-Path $gameAddon "resource"
New-Item -ItemType Directory -Force -Path $gameLocalizationDir | Out-Null
$utf8Bom = New-Object System.Text.UTF8Encoding($true)
foreach ($localizationFile in $localizationFiles) {
	$sourceLocalization = Join-Path $sourceLocalizationDir $localizationFile
	$targetLocalization = Join-Path $gameLocalizationDir $localizationFile
	[System.IO.File]::WriteAllText(
		$targetLocalization,
		(Get-Content -Raw -Encoding UTF8 -LiteralPath $sourceLocalization),
		$utf8Bom
	)
}
Assert-SwiftMenuCompiledOutputs -Paths @(
	(Join-Path $gameLocalizationDir "platform_english.txt"),
	(Join-Path $gameLocalizationDir "platform_schinese.txt")
)

$compiledScriptText = [System.Text.Encoding]::UTF8.GetString(
	[System.IO.File]::ReadAllBytes($expectedOutputs[2])
)
if (-not $compiledScriptText.Contains("var SwiftDemoVoice = (function () {") -or
	-not $compiledScriptText.Contains('$.Schedule(0.0, SwiftDemoVoice.OnLoad);')) {
	throw "Compiled Panorama script is missing the SwiftDemoVoice runtime bootstrap."
}

Remove-SwiftMenuPanoramaStripped -GameAddon $gameAddon

Copy-Item -LiteralPath (Join-Path $contentAddon "addoninfo.txt") -Destination (Join-Path $gameAddon "addoninfo.txt") -Force

Write-Host "Built demo menu Panorama override."
Write-Host "Compiled addon: $gameAddon"
Write-Host "Override layout: panorama/layout/hud/huddemocontroller.vxml_c"
Write-Host "Voice controller: panorama/scripts/hud/swift_demo_voice.vjs_c"
Write-Host "Localization: resource/platform_english.txt, platform_schinese.txt"
