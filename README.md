# Swift DemoUI Pro

[English](README.md) | [简体中文](README_CN.md)

[Official website](https://nicedayzhu.github.io/SwiftDemoUIPro/) · [Download latest release](https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](#quick-start)

Swift DemoUI Pro is an unofficial, client-side enhancement for Counter-Strike 2 Demo and HLTV playback. Its Windows launcher opens `.dem` files, downloaded `.zip` archives, and FACEIT `.dem.zst` downloads, while the in-game panel adds recorded-voice controls, one-click POV switching, and round navigation without replacing Valve's native DemoUI.

No SwiftlyS2 installation, server plugin, Counter-Strike 2 Workshop Tools DLC, Workshop item, or manual archive extraction is required. The release package contains the voice-index compiler, Zstandard decoder, and session VPK writer it needs at runtime.

> [!IMPORTANT]
> Demo playback is started with `-insecure`, and the launcher temporarily adds an override SearchPath to `gameinfo.gi`. Do not use that CS2 session for matchmaking. When finished, fully exit CS2 and select **Stop and restore** in the launcher.

## Interface Preview

### Windows launcher

![Swift DemoUI Pro launcher with Demo and ZIP selection and TrueView compatibility control](docs/images/launcher-playback-ui.png)

Select or drag in a Demo, ZIP archive, or `.dem.zst` download, confirm the detected CS2 installation, and start playback. The launcher remembers the interface language, TrueView preference, and optional advanced launch arguments, then removes its temporary files after playback.

### In-game DemoUI

![Swift DemoUI Pro player voice, POV, and round navigation panel during CS2 Demo playback](docs/images/demo-voice-ui.png)

The added panel keeps the native timeline and playback controls available while providing per-player recorded voice, POV selection, and direct round navigation. During launcher-started playback, a native-inspired lower-left HUD also shows the avatar and name of each player whose recorded voice packets are active at the current Demo tick.

## Highlights

- Opens `.dem` files, downloaded `.zip` archives, and FACEIT `.dem.zst` files directly. ZIP archives expose only the selected Demo; Zstandard files are stream-decompressed in the background.
- Detects CS2 across Steam libraries and starts it with a one-time `-insecure` argument without changing permanent Steam launch options.
- Provides a lightweight advanced launch-options entry that opens a separate dialog for experienced players. Custom arguments are stored locally and appended only to launcher-started sessions; launcher-managed safety arguments cannot be overridden.
- Defaults **TrueView prediction** off to prevent flicker in Demos that do not contain compatible TrueView command data.
- Controls recorded voice for everyone, T only, CT only, or individual players across all 64 display slots.
- Parses recorded `VoiceData` before launch and shows active speakers by Demo tick without modifying the Demo or patching `client.dll`.
- Compiles the per-Demo Panorama voice index and session VPK with the bundled Rust sidecar; players do not need Valve's ResourceCompiler or VPKEdit.
- Prepares large Demos and voice indexes in the background, with visible stage feedback instead of freezing the launcher window.
- Switches to a live player's first-person POV after validating the target.
- Jumps directly to the beginning of any recorded round.
- Supports English and Simplified Chinese: the launcher follows the system language, while the in-game DemoUI automatically follows the CS2 interface language.
- Checks the latest official GitHub Release in the background and shows only a small, dismissible notice. Launcher and DemoUI updates remain separate choices; nothing is downloaded automatically.
- Keeps installation reversible with backups, an isolated staging directory, and explicit cleanup.

## Quick Start

Download and extract `SwiftDemoUIPro-v<version>-win64.zip`, then:

1. Run `SwiftDemoUIPro.exe` from the extracted folder. Keep the whole package together.
2. Select or drag in a `.dem`, `.zip`, or `.dem.zst` file.
3. Confirm the automatically detected CS2 installation.
4. Leave **TrueView prediction** disabled for most downloaded or third-party Demos. Enable it only when the recording is known to support TrueView.
5. Select **Start playback**. The launcher installs the DemoUI for this session and starts CS2.
6. When finished, fully exit CS2, return to the launcher, and select **Stop and restore**.

If the launcher was interrupted, reopen it to resume the pending cleanup.

## Using the In-Game Panel

- Use CS2's native Demo mouse-mode hotkey to show the cursor.
- Use the prominent checked tune icon immediately to the right of the playback-speed control in CS2's full native Demo bar to hide the complete Swift Demo tools panel or bring it back. The panel also follows the native **Shift+F2** DemoUI states and does not install a custom keyboard binding.
- When Demo playback starts, all 64 recorded-voice slots are enabled automatically. The panel also uses CS2's Panorama mute APIs to enable each discovered Demo XUID, without directly accessing profile data files.
- Click a live player to switch to first-person POV; the current POV is highlighted and labeled **CURRENT**. Check **SHOW AVATAR** above the list to show each Steam avatar beside the player's name and team/slot metadata for quicker visual identification.
- Click the audio button on a player row to toggle only that player's recorded Demo voice slot. Enabling it also clears that XUID's current native mute through `GameStateAPI`.
- Use **HEAR ALL**, **MUTE ALL**, **T ONLY**, or **CT ONLY** for quick Demo voice filtering; **HEAR ALL** and the team filters clear current native mutes for their enabled XUIDs. These labels follow the CS2 language.
- Player selection follows native team identity: T players use gold/yellow accents and CT players use blue accents for the observed row and enabled voice state.
- The lower-left speaker HUD follows seeking, pause, and playback speed because it is driven by the current Demo tick rather than a live microphone callback.
- Expand **ROUND NAVIGATION** and select a round to jump to its starting tick.
- The panel starts in the lower-right area above the native bottom equipment strip. In Demo mouse mode, drag the gold speaker control to move it; the cursor changes to the move shape and a translucent full-size placement outline follows the pointer while the old panel disappears. The preview snaps near screen edges and the panel is clamped so it cannot be lost off-screen. The reset icon restores the responsive default position, and the remaining title area expands or collapses the panel without changing its readable width. Position is remembered for the current Panorama session and is re-clamped when the viewport changes.

## Common Problems

| Symptom | What to do |
| --- | --- |
| The picture flickers or the console repeats `Not enough TrueView command lookahead` | Stop playback, disable **TrueView prediction**, and start the Demo again. |
| Windows reports a missing `Qt6Gui.dll` | Run the EXE from the extracted release package, not from the raw `launcher\build\Release` directory. |
| The launcher says cleanup is pending | Fully exit CS2, reopen the launcher if necessary, and select **Stop and restore**. |
| Some or all player voices are unavailable | The launcher can only play voice packets stored in the Demo; missing recordings cannot be recovered. |
| The speaker HUD shows a player but no voice is audible | Restart playback with the current package. Demo slots and discovered XUIDs are enabled automatically; **HEAR ALL** repeats both operations manually. |
| Voice plays but the lower-left speaker HUD stays empty | Start playback through the complete launcher package. A standalone DemoUI VPK has only an empty fallback index, and older packages do not include `swift-demo-voice-indexer.exe`. |

## Compatibility and Safety

- FACEIT Demos commonly contain recorded voice, but older, damaged, or differently sourced Demos may not.
- ZIP entries must be unencrypted and use a compression method supported by the bundled miniz library. Extracted Demos are limited to 8 GB.
- `.dem.zst` files must decompress to a CS2 Demo with a `PBDEMS2` header. Decompressed Demos are limited to 8 GB; no external `zstd.exe` is required.
- Valve can change the native DemoUI at any time, which may require a project update.
- This project is not affiliated with or endorsed by Valve or FACEIT.

## Documentation

- [Developer Guide](DEVELOPMENT.md) — architecture, prerequisites, building, tests, localization, versioning, and releases.
- [Launcher Guide](launcher/README.md) — launcher workflow, safety boundaries, and packaging behavior.
- [Third-Party Notices](THIRD_PARTY_NOTICES.md) — dependency and redistribution information.

## Contributing

Issues and pull requests are welcome. Before contributing, read the [Developer Guide](DEVELOPMENT.md) and run the relevant tests.

## License

Original project code is available under the [MIT License](LICENSE). Third-party libraries, fonts, game resources, names, and trademarks remain under their respective terms; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Support

If Swift DemoUI Pro is useful to you, you can support continued development on Ko-fi:

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/K6C623WHCQ)
