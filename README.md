# Swift DemoUI Pro

[English](README.md) | [简体中文](README_CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](#requirements)

A client-side Panorama enhancement and optional Qt launcher for Counter-Strike 2 demo and HLTV playback. It adds per-player recorded-voice controls, one-click POV switching, and direct round navigation while preserving Valve's native DemoUI.

The project does not require SwiftlyS2, a server plugin, or a Workshop resource.

> [!IMPORTANT]
> Swift DemoUI Pro launches CS2 with `-insecure` and temporarily adds an override SearchPath to `gameinfo.gi`. Do not use that session for normal matchmaking. After watching, fully exit CS2 and choose **Stop Watching Demo** in the launcher so it can remove the VPK, SearchPath, and temporary files.

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

## Quick Start

For end users, extract a packaged `SwiftDemoUIPro-win64.zip`, then:

1. Run `SwiftDemoUIPro.exe` with `swift_demo_menu_override.vpk` beside it.
2. Select or drag in a `.dem` file or a `.zip` archive. If the ZIP contains multiple Demos, choose one from the list.
3. Confirm the detected CS2 installation.
4. Choose **Start Watching Demo**. The launcher starts CS2 with a one-time `-insecure` argument; it does not change Steam's permanent launch options.
5. When finished, fully exit CS2, return to the launcher, and choose **Stop Watching Demo**.

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

Build the VPK first, then run:

```powershell
.\launcher\build-launcher.ps1 -QtRoot "C:\Qt\6.8.3\msvc2022_64" -Package
```

The script configures CMake, builds the launcher, runs its Qt tests, collects the required dynamic Qt libraries with `windeployqt`, and creates:

```text
launcher\package\SwiftDemoUIPro-win64.zip
```

See [launcher/README.md](launcher/README.md) for launcher architecture, localization, and packaging details.

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
