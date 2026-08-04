param(
	[ValidateSet("Build", "Compile", "Pack", "Install", "Uninstall")]
	[string]$Action = "Build",
	[string]$Cs2Root = "F:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive",
	[string]$AddonName = "swift_demo_menu_override",
	[string]$VpkEditCli = "F:\cs2dev\SkinTools\VPKEdit-Windows-Standalone-msvc-Release\vpkeditcli.exe",
	[switch]$InstallLocalOverride
)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$scriptsDir = Join-Path $projectRoot "powershell"

function Invoke-CurrentCompile {
	& (Join-Path $scriptsDir "build_demo_menu_override.ps1") `
		-ProjectRoot $projectRoot `
		-Cs2Root $Cs2Root `
		-AddonName $AddonName
}

function Invoke-CurrentPack {
	& (Join-Path $scriptsDir "pack_demo_menu_override_vpk.ps1") `
		-ProjectRoot $projectRoot `
		-Cs2Root $Cs2Root `
		-AddonName $AddonName `
		-VpkEditCli $VpkEditCli
}

function Invoke-CurrentInstall {
	& (Join-Path $scriptsDir "install_demo_menu_override.ps1") `
		-ProjectRoot $projectRoot `
		-CsgoPath (Join-Path $Cs2Root "game\csgo") `
		-AddonName $AddonName
}

function Invoke-CurrentUninstall {
	& (Join-Path $scriptsDir "uninstall_demo_menu_override.ps1") `
		-CsgoPath (Join-Path $Cs2Root "game\csgo") `
		-AddonName $AddonName
}

switch ($Action) {
	"Compile" {
		Invoke-CurrentCompile
	}
	"Pack" {
		Invoke-CurrentPack
	}
	"Install" {
		Invoke-CurrentInstall
	}
	"Uninstall" {
		Invoke-CurrentUninstall
	}
	"Build" {
		Invoke-CurrentCompile
		Invoke-CurrentPack
		if ($InstallLocalOverride) {
			Invoke-CurrentInstall
		}
	}
}

Write-Host "Swift Demo Menu action complete: $Action"
