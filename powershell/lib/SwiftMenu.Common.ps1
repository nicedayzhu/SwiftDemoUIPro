$script:SwiftMenuDefaultCs2Root = "F:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive"
$script:SwiftMenuDefaultVpkEditCli = "F:\cs2dev\SkinTools\VPKEdit-Windows-Standalone-msvc-Release\vpkeditcli.exe"
$script:SwiftMenuDefaultAddonName = "swift_custom_game_menu_probe"

function Resolve-SwiftMenuProjectRoot {
	param(
		[string]$ProjectRoot,
		[string]$ScriptDirectory
	)

	if (-not [string]::IsNullOrWhiteSpace($ProjectRoot)) {
		return (Resolve-Path -LiteralPath $ProjectRoot).Path
	}

	if ([string]::IsNullOrWhiteSpace($ScriptDirectory)) {
		$ScriptDirectory = $PSScriptRoot
	}

	return (Resolve-Path -LiteralPath (Join-Path (Split-Path -Parent $ScriptDirectory) "..")).Path
}

function Assert-SwiftMenuSafeAddonName {
	param([string]$Name)

	if ([string]::IsNullOrWhiteSpace($Name) -or $Name -match '[\\/:*?"<>|]') {
		throw "AddonName must be a simple directory name: $Name"
	}
}

function Assert-SwiftMenuFileExists {
	param(
		[string]$Path,
		[string]$Message
	)

	if (-not (Test-Path -LiteralPath $Path)) {
		if ([string]::IsNullOrWhiteSpace($Message)) {
			$Message = "Missing file: $Path"
		}
		throw $Message
	}
}

function Test-SwiftMenuPathIsChild {
	param(
		[string]$Path,
		[string]$ExpectedParent
	)

	$resolvedPath = (Resolve-Path -LiteralPath $Path).Path.TrimEnd('\', '/')
	$resolvedParent = (Resolve-Path -LiteralPath $ExpectedParent).Path.TrimEnd('\', '/')

	return $resolvedPath.Equals($resolvedParent, [System.StringComparison]::OrdinalIgnoreCase) -or
		$resolvedPath.StartsWith($resolvedParent + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
		$resolvedPath.StartsWith($resolvedParent + [System.IO.Path]::AltDirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)
}

function Remove-SwiftMenuDirectoryIfChild {
	param(
		[string]$Path,
		[string]$ExpectedParent
	)

	if (-not (Test-Path -LiteralPath $Path)) {
		return
	}

	if (-not (Test-SwiftMenuPathIsChild -Path $Path -ExpectedParent $ExpectedParent)) {
		throw "Refusing to remove path outside expected parent: $((Resolve-Path -LiteralPath $Path).Path)"
	}

	Get-ChildItem -LiteralPath $Path -Recurse -Force | ForEach-Object {
		$_.Attributes = [System.IO.FileAttributes]::Normal
	}
	Remove-Item -LiteralPath (Resolve-Path -LiteralPath $Path).Path -Recurse -Force
}

function Write-SwiftMenuPanoramaPreprocessorConfig {
	param([string]$ContentAddon)

	$panoramaDir = Join-Path $ContentAddon "panorama"
	New-Item -ItemType Directory -Force -Path $panoramaDir | Out-Null
	Set-Content -LiteralPath (Join-Path $panoramaDir "preprocessor_config.txt") -Encoding ASCII -Value @'
"PanzipCfg"
{
    "BlockDefs"
    {
    }
}
'@
}

function Write-SwiftMenuTextNoBom {
	param(
		[string]$Path,
		[string]$Value
	)

	$encoding = New-Object System.Text.UTF8Encoding($false)
	[System.IO.File]::WriteAllText($Path, $Value, $encoding)
}

function Get-SwiftMenuCs2Paths {
	param(
		[string]$Cs2Root,
		[string]$AddonName
	)

	Assert-SwiftMenuSafeAddonName -Name $AddonName

	$gameDir = Join-Path $Cs2Root "game\csgo"
	$contentAddonsRoot = Join-Path $Cs2Root "content\csgo_addons"
	$gameAddonsRoot = Join-Path $Cs2Root "game\csgo_addons"

	return [pscustomobject]@{
		ResourceCompiler = Join-Path $Cs2Root "game\bin\win64\resourcecompiler.exe"
		GameDir = $gameDir
		CsgoPath = $gameDir
		GameInfoPath = Join-Path $gameDir "gameinfo.gi"
		OverrideDir = Join-Path $gameDir "overrides"
		ContentAddonsRoot = $contentAddonsRoot
		GameAddonsRoot = $gameAddonsRoot
		ContentAddon = Join-Path $contentAddonsRoot $AddonName
		GameAddon = Join-Path $gameAddonsRoot $AddonName
	}
}

function Invoke-SwiftMenuResourceCompiler {
	param(
		[string]$ResourceCompiler,
		[string]$GameDir,
		[string[]]$Inputs
	)

	Assert-SwiftMenuFileExists -Path $ResourceCompiler -Message "resourcecompiler.exe not found: $ResourceCompiler"

	$compilerArgs = @("-game", $GameDir)
	foreach ($inputFile in $Inputs) {
		$compilerArgs += @("-i", $inputFile)
	}
	$compilerArgs += @("-f", "-nop4", "-v")

	& $ResourceCompiler @compilerArgs
	if ($LASTEXITCODE -ne 0) {
		throw "resourcecompiler failed with exit code $LASTEXITCODE"
	}
}

function Assert-SwiftMenuCompiledOutputs {
	param([string[]]$Paths)

	foreach ($outputFile in $Paths) {
		if (-not (Test-Path -LiteralPath $outputFile)) {
			throw "Expected compiled resource not found: $outputFile"
		}
	}
}

function Remove-SwiftMenuPanoramaStripped {
	param([string]$GameAddon)

	$strippedDir = Join-Path $GameAddon "panorama_stripped"
	if (Test-Path -LiteralPath $strippedDir) {
		Remove-SwiftMenuDirectoryIfChild -Path $strippedDir -ExpectedParent $GameAddon
	}
}

function Invoke-SwiftMenuVpkPack {
	param(
		[string]$VpkEditCli,
		[string]$SourceDir,
		[string]$OutVpk
	)

	Assert-SwiftMenuFileExists -Path $VpkEditCli -Message "VPKEdit CLI not found: $VpkEditCli"
	Assert-SwiftMenuFileExists -Path $SourceDir -Message "VPK source folder not found: $SourceDir"

	$outParent = Split-Path -Parent $OutVpk
	if ($outParent) {
		New-Item -ItemType Directory -Force -Path $outParent | Out-Null
	}

	& $VpkEditCli `
		--output $OutVpk `
		--type vpk `
		--version 2 `
		--single-file `
		$SourceDir

	if ($LASTEXITCODE -ne 0) {
		throw "vpkeditcli failed with exit code $LASTEXITCODE"
	}

	Assert-SwiftMenuFileExists -Path $OutVpk -Message "Expected VPK was not created: $OutVpk"
}

function Install-SwiftMenuOverrideVpk {
	param(
		[string]$CsgoPath,
		[string]$SourceVpk,
		[string]$TargetName,
		[string]$BackupSuffix,
		[string[]]$RemoveSearchPathLines = @()
	)

	Assert-SwiftMenuFileExists -Path $SourceVpk -Message "Missing VPK: $SourceVpk. Build it first."

	$gameInfoPath = Join-Path $CsgoPath "gameinfo.gi"
	Assert-SwiftMenuFileExists -Path $gameInfoPath -Message "Missing gameinfo.gi: $gameInfoPath"

	$overrideDir = Join-Path $CsgoPath "overrides"
	$targetVpk = Join-Path $overrideDir $TargetName
	$searchPathLine = "`t`t`tGame`tcsgo/overrides/$TargetName"

	New-Item -ItemType Directory -Force -Path $overrideDir | Out-Null
	Copy-Item -LiteralPath $SourceVpk -Destination $targetVpk -Force

	$backupPath = "$gameInfoPath.$BackupSuffix.bak"
	if (-not (Test-Path -LiteralPath $backupPath)) {
		Copy-Item -LiteralPath $gameInfoPath -Destination $backupPath
	}

	$content = Get-Content -LiteralPath $gameInfoPath
	foreach ($lineToRemove in $RemoveSearchPathLines) {
		$content = @($content | Where-Object { $_ -ne $lineToRemove })
	}

	if ($content -contains $searchPathLine) {
		$updated = $content
	} else {
		$inserted = $false
		$updated = foreach ($line in $content) {
			if (-not $inserted -and $line -match '^\s*Game\s+csgo\s*$') {
				$searchPathLine
				$inserted = $true
			}
			$line
		}

		if (-not $inserted) {
			throw "Could not find 'Game csgo' SearchPath in gameinfo.gi"
		}
	}

	Set-Content -LiteralPath $gameInfoPath -Value $updated

	return [pscustomobject]@{
		TargetVpk = $targetVpk
		GameInfoPath = $gameInfoPath
		BackupPath = $backupPath
		SearchPathLine = $searchPathLine
	}
}

function Remove-SwiftMenuOverrideVpk {
	param(
		[string]$CsgoPath,
		[string]$TargetName
	)

	$overrideDir = Join-Path $CsgoPath "overrides"
	$targetVpk = Join-Path $overrideDir $TargetName

	if (Test-Path -LiteralPath $targetVpk) {
		if (-not (Test-SwiftMenuPathIsChild -Path $targetVpk -ExpectedParent $overrideDir)) {
			throw "Refusing to remove VPK outside overrides directory: $((Resolve-Path -LiteralPath $targetVpk).Path)"
		}
		Remove-Item -LiteralPath (Resolve-Path -LiteralPath $targetVpk).Path -Force
	}
}
