# Swift DemoUI Pro

[English](README.md) | [简体中文](README_CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](#requirements)

A client-side Panorama enhancement and optional Qt launcher for Counter-Strike 2 demo and HLTV playback. It adds per-player recorded-voice controls, one-click POV switching, and direct round navigation while preserving Valve's native DemoUI.

The project does not require SwiftlyS2, a server plugin, or a Workshop resource.

> [!IMPORTANT]
> Swift DemoUI Pro launches CS2 with `-insecure` and temporarily adds an override SearchPath to `gameinfo.gi`. Do not use that session for normal matchmaking. After watching, fully exit CS2 and choose **Stop Watching Demo** in the launcher so it can remove the VPK, SearchPath, and temporary files.

## Interface Preview

![Swift DemoUI Pro player voice, POV, and round navigation panel during CS2 Demo playback](docs/images/demo-voice-ui.png)

The in-game panel keeps the native DemoUI controls available while adding per-player recorded-voice controls, one-click POV switching, and direct round navigation.

## Features

- Automatically enables both recorded voice masks when a demo opens:

  ```text
  tv_listen_voice_indices -1
  tv_listen_voice_indices_h -1
  ```

- Reads player names, teams, account IDs, and display slots from the current demo.
- Supports all 64 display slots by generating separate signed 32-bit low/high masks.
- Provides all, mute-all, T-only, CT-only, and per-player voice controls.
- Switches to a player's first-person POV after validating the account ID, slot, and name.
- Disables misleading POV actions for dead or disconnected players while keeping their recorded-voice toggle available.
- Reads native `RoundIntervals` and jumps directly to the start of any round.
- Preserves the native timeline, playback controls, settings, and default camera-mode hotkeys.
- Uses a restrained graphite-and-gold interface with clear CT/T and voice-state indicators.
- Includes **Swift DemoUI Pro**, a native Qt 6 launcher with Steam-library detection, drag-and-drop `.dem`/`.zip` selection, automatic cleanup, English/Chinese UI, and system-language detection.
- Opens downloaded ZIP archives directly. A single Demo is selected automatically; archives containing multiple Demos show a selection list, and only the chosen `.dem` is streamed into the launcher's staging directory.
- Disables CS2 TrueView prediction by default for compatibility with third-party Demos that do not contain its command data; players can opt in for supported recordings.

## Quick Start

For end users, extract a packaged `SwiftDemoUIPro-v<version>-win64.zip`, then:

1. Run `SwiftDemoUIPro.exe` with `swift_demo_menu_override.vpk` beside it.
2. Select or drag in a `.dem` file or a `.zip` archive. If the ZIP contains multiple Demos, choose one from the list.
3. Confirm the detected CS2 installation.
4. Leave **TrueView prediction** disabled for downloaded or third-party Demos. Enable it only for a recording known to contain compatible TrueView command data.
5. Choose **Start Watching Demo**. The launcher starts CS2 with a one-time `-insecure` argument; it does not change Steam's permanent launch options.
6. When finished, fully exit CS2, return to the launcher, and choose **Stop Watching Demo**.

If the launcher is interrupted, reopen it to continue the pending cleanup.

## Using the Demo Menu

The menu opens automatically during demo/HLTV playback and initially enables all voice slots.

- Use CS2's native demo mouse-mode hotkey to show the cursor.
- Click a live player's name area to switch to first-person POV. The active POV is marked in gold.
- Click the audio button on the right side of a player row to toggle only that player's recorded voice.
- Expand **ROUND NAVIGATION** and select a round to jump to its starting tick.
- Use the top shortcuts to listen to everyone, mute everyone, or isolate T/CT voice.

Displayed `SLOT 1` maps to low-mask bit 0, `SLOT 32` to low-mask bit 31, `SLOT 33` to high-mask bit 0, and `SLOT 64` to high-mask bit 31. For example, display slots 4, 5, 9, 11, and 12 produce the low mask `3352`.

## Requirements

### Menu/VPK build

- Windows and PowerShell.
- A current Counter-Strike 2 installation containing Valve's `resourcecompiler.exe`.
- [VPKEdit](https://github.com/craftablescience/VPKEdit) command-line tools.
- Node.js for the Panorama mask tests.

### Launcher build

- Qt 6.5 or newer with a 64-bit MSVC Desktop kit.
- Visual Studio with the Desktop development with C++ workload.
- CMake, either on `PATH` or installed by Qt/Visual Studio.

## Build the Panorama Override

Run the following from the repository root and replace the example paths for your machine:

```powershell
.\demo-menu.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe"
```

Output:

```text
dist\swift_demo_menu_override.vpk
```

Individual actions are also available:

```powershell
.\demo-menu.ps1 -Action Compile -Cs2Root "<CS2 root>"
.\demo-menu.ps1 -Action Pack -Cs2Root "<CS2 root>" -VpkEditCli "<vpkeditcli.exe>"
.\demo-menu.ps1 -Action Install -Cs2Root "<CS2 root>"
.\demo-menu.ps1 -Action Uninstall -Cs2Root "<CS2 root>"
```

`-InstallLocalOverride` builds and installs in one operation. Installation copies the VPK to `game\csgo\overrides`, inserts one exact SearchPath before `Game csgo`, and creates `gameinfo.gi.swift_demo_menu_override.restore.bak` before the first modification.

Fully restart CS2 after installing, updating, or uninstalling the Panorama override because compiled UI resources are cached by the client.

## Build the Launcher

For a quick compile-and-test cycle that does not require a VPK, run:

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -SkipVpkCheck
```

This configures CMake, builds the launcher, compiles translations, and runs the Qt tests. It does **not** run `windeployqt`; therefore `launcher\build\Release\SwiftDemoUIPro.exe` is a raw build artifact and is not a standalone executable. Double-clicking it outside a Qt development environment can report missing files such as `Qt6Gui.dll`.

To create something that can be launched directly, build the VPK first and then package the launcher:

```powershell
.\launcher\build-launcher.ps1 -QtRoot "C:\Qt\6.8.3\msvc2022_64" -Package
```

Packaging reruns the build and tests, collects the required dynamic Qt libraries and platform plugin with `windeployqt`, and creates both an unpacked test directory and a distributable ZIP:

```text
launcher\package\SwiftDemoUIPro-v<version>\SwiftDemoUIPro.exe
launcher\package\SwiftDemoUIPro-v<version>-win64.zip
```

Run the EXE from the unpacked `launcher\package\SwiftDemoUIPro-v<version>` directory for local end-to-end testing. Keep that directory together; do not copy only the EXE.

See [launcher/README.md](launcher/README.md) for launcher architecture, localization, and packaging details.

## Versioning and Releases

The root [VERSION](VERSION) file is the single version source. CMake uses it for the application version, the About page, Windows file metadata, and versioned package names. The launcher also embeds the current Git commit.

Create a complete local release candidate with one command:

```powershell
.\release.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe" `
  -QtRoot "C:\Qt\6.8.3\msvc2022_64"
```

`release.ps1` intentionally requires `git status --short` to be empty because its source archive is created from `HEAD`. It is not the command for testing uncommitted changes: use `build-launcher.ps1` during development, then commit the completed change before building a release candidate. Stashing a change excludes it from the release build.

This runs all JavaScript and Qt tests, rebuilds the VPK and launcher, then creates these files under `release\v<version>`:

```text
SwiftDemoUIPro-v<version>-win64.zip
SwiftDemoUIPro-v<version>-source.zip
SHA256SUMS.txt
```

To connect the local repository to GitHub for the first time, install [GitHub CLI](https://cli.github.com/), then run:

```powershell
gh auth login
gh repo create nicedayzhu/SwiftDemoUIPro --public --source . --remote origin --push
```

For each release, choose a [Semantic Version](https://semver.org/) and run the same command with `-Version` and `-Publish`:

```powershell
.\release.ps1 -Version 0.2.0 `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt desktop kit>" `
  -Publish
```

Publishing updates `VERSION` when needed, creates the Conventional Commit `chore(release): v<version>`, rebuilds and tests everything, creates an annotated Git tag, pushes the branch and tag, uploads all three files to a draft GitHub Release, and publishes it only after every upload succeeds. Re-running a failed draft release is safe; an already published release is never overwritten.

After downloading a release, compare its checksum with `SHA256SUMS.txt`:

```powershell
Get-FileHash .\SwiftDemoUIPro-v0.2.0-win64.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

GitHub Actions runs the portable JavaScript and Qt tests on every push and pull request. The VPK and final release package stay in the local release command because they require an installed copy of CS2 and Valve's `resourcecompiler.exe`.

## Tests

Run the Panorama voice-mask and layout integration tests:

```powershell
node .\tests\test_demo_voice_mask.js
```

Launcher tests run automatically through `build-launcher.ps1`. They can also be run from an existing build tree:

```powershell
ctest --test-dir .\launcher\build -C Release --output-on-failure
```

## Repository Layout

| Path | Purpose |
| --- | --- |
| `addon/` | Panorama layout, JavaScript, and styles compiled into the override VPK |
| `launcher/` | Qt 6 Widgets launcher, tests, translations, fonts, and packaging script |
| `powershell/` | Shared compile, pack, install, and uninstall implementation |
| `tests/` | Panorama voice-mask and native-layout integration tests |
| `demo-menu.ps1` | Repository-level build and lifecycle entry point |
| `dist/` | Generated VPK output; not tracked by Git |

## Compatibility and Limitations

- The menu can only control voice packets already present in the demo. It cannot recover voice data that was never recorded.
- FACEIT demos commonly contain usable voice data, but older, damaged, or differently sourced demos may not.
- Valve can change the native `huddemocontroller` resource at any time. When that happens, resynchronize the layout with the current game files before rebuilding the override.
- While `GameStateAPI.GetPlayerDataJSO()` is unavailable, the menu shows a waiting state and retries every 0.75 seconds.
- This is an unofficial client modification and is not affiliated with or endorsed by Valve or FACEIT.
- ZIP playback supports unencrypted entries that use a compression method supported by the bundled miniz library. The launcher never expands unrelated archive entries and rejects an extracted Demo larger than 8 GB.

Background: [FACEITcom discussion about CS2 demo voice indices](https://www.reddit.com/r/FACEITcom/comments/16vvidt/no_recorded_voice_chat_in_faceit_cs2_demos/).

## Contributing

Issues and pull requests are welcome. Please keep changes focused, run the relevant JavaScript and Qt tests, and update both language versions when changing user-facing documentation. New launcher strings should remain English in source and be translated through Qt Linguist; preserve placeholders such as `%1` and `%2`.

## License and Third-Party Components

Original project code is available under the [MIT License](LICENSE). Third-party libraries, fonts, game resources, names, and trademarks remain under their respective terms and are not relicensed by MIT. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [launcher/THIRD_PARTY_NOTICES.txt](launcher/THIRD_PARTY_NOTICES.txt).

In particular, the native-compatible DemoUI layout is derived from or interoperates with Valve's Counter-Strike 2 UI resources. Review Valve's terms before redistributing game-derived material or packaged assets. This licensing summary is informational and is not legal advice.

## Support

If Swift DemoUI Pro is useful to you, you can support continued development on Ko-fi:

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/K6C623WHCQ)
