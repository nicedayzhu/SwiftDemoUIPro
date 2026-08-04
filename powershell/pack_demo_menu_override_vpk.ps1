param(
	[string]$ProjectRoot = "",
	[string]$Cs2Root = "F:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive",
	[string]$AddonName = "swift_demo_menu_override",
	[string]$VpkEditCli = "F:\cs2dev\SkinTools\VPKEdit-Windows-Standalone-msvc-Release\vpkeditcli.exe"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "lib\SwiftMenu.Common.ps1")

$projectRoot = Resolve-SwiftMenuProjectRoot -ProjectRoot $ProjectRoot -ScriptDirectory $scriptDir
Assert-SwiftMenuSafeAddonName -Name $AddonName

$sourceAddon = Join-Path $Cs2Root "game\csgo_addons\$AddonName"
$distRoot = Join-Path $projectRoot "dist"
$stageRoot = Join-Path $distRoot ("vpk_stage_" + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds())
$stageAddon = Join-Path $stageRoot $AddonName
$outVpk = Join-Path $distRoot "$AddonName.vpk"

Assert-SwiftMenuFileExists -Path $sourceAddon -Message "Compiled addon folder not found: $sourceAddon. Run Compile first."

New-Item -ItemType Directory -Force -Path $distRoot, $stageRoot | Out-Null
try {
	Copy-Item -LiteralPath $sourceAddon -Destination $stageAddon -Recurse -Force
	Invoke-SwiftMenuVpkPack -VpkEditCli $VpkEditCli -SourceDir $stageAddon -OutVpk $outVpk
} finally {
	if (Test-Path -LiteralPath $stageRoot) {
		Remove-SwiftMenuDirectoryIfChild -Path $stageRoot -ExpectedParent $distRoot
	}
}

Write-Host "Built $outVpk"
