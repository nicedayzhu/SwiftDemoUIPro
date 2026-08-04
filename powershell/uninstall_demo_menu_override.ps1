param(
	[string]$CsgoPath = "F:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive\game\csgo",
	[string]$AddonName = "swift_demo_menu_override"
)

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "lib\SwiftMenu.Common.ps1")

Assert-SwiftMenuSafeAddonName -Name $AddonName

$targetName = "$AddonName.vpk"
$gameInfoPath = Join-Path $CsgoPath "gameinfo.gi"
$searchPathLine = "`t`t`tGame`tcsgo/overrides/$targetName"

Assert-SwiftMenuFileExists -Path $gameInfoPath -Message "Missing gameinfo.gi: $gameInfoPath"
Remove-SwiftMenuOverrideVpk -CsgoPath $CsgoPath -TargetName $targetName

$content = Get-Content -LiteralPath $gameInfoPath
$updated = @($content | Where-Object { $_ -ne $searchPathLine })
Set-Content -LiteralPath $gameInfoPath -Value $updated

Write-Host "Removed demo menu override VPK and SearchPath."
Write-Host "Restart CS2 to clear cached Panorama resources."
