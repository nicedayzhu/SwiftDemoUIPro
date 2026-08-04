param(
	[string]$ProjectRoot = "",
	[string]$CsgoPath = "F:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo",
	[string]$AddonName = "swift_demo_menu_override"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "lib\SwiftMenu.Common.ps1")

$projectRoot = Resolve-SwiftMenuProjectRoot -ProjectRoot $ProjectRoot -ScriptDirectory $scriptDir
Assert-SwiftMenuSafeAddonName -Name $AddonName

$vpkPath = Join-Path $projectRoot "dist\$AddonName.vpk"
$targetName = "$AddonName.vpk"

$result = Install-SwiftMenuOverrideVpk `
	-CsgoPath $CsgoPath `
	-SourceVpk $vpkPath `
	-TargetName $targetName `
	-BackupSuffix "$AddonName.restore"

Write-Host "Installed demo menu override: $($result.TargetVpk)"
Write-Host "SearchPath: $($result.SearchPathLine)"
Write-Host "Original gameinfo backup: $($result.BackupPath)"
Write-Host "Restart CS2 before testing."
