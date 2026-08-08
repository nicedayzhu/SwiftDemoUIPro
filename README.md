# Swift DemoUI Pro

[English](README.md) | [简体中文](README_CN.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](#quick-start)

Swift DemoUI Pro is an unofficial, client-side enhancement for Counter-Strike 2 Demo and HLTV playback. Its Windows launcher opens `.dem` files or downloaded `.zip` archives, while the in-game panel adds recorded-voice controls, one-click POV switching, and round navigation without replacing Valve's native DemoUI.

No SwiftlyS2 installation, server plugin, Workshop item, or manual ZIP extraction is required.

> [!IMPORTANT]
> Demo playback is started with `-insecure`, and the launcher temporarily adds an override SearchPath to `gameinfo.gi`. Do not use that CS2 session for matchmaking. When finished, fully exit CS2 and select **Stop and restore** in the launcher.

## Interface Preview

### Windows launcher

![Swift DemoUI Pro launcher with Demo and ZIP selection and TrueView compatibility control](docs/images/launcher-playback-ui.png)

Select or drag in a Demo or ZIP archive, confirm the detected CS2 installation, and start playback. The launcher remembers the interface language and TrueView preference, then removes its temporary files after playback.

### In-game DemoUI

![Swift DemoUI Pro player voice, POV, and round navigation panel during CS2 Demo playback](docs/images/demo-voice-ui.png)

The added panel keeps the native timeline and playback controls available while providing per-player recorded voice, POV selection, and direct round navigation. During launcher-started playback, a native-inspired lower-left HUD also shows the avatar and name of each player whose recorded voice packets are active at the current Demo tick.

## Highlights

- Opens `.dem` files and downloaded `.zip` archives directly. If an archive contains several Demos, the launcher lets you choose one and extracts only that entry.
- Detects CS2 across Steam libraries and starts it with a one-time `-insecure` argument without changing permanent Steam launch options.
- Defaults **TrueView prediction** off to prevent flicker in Demos that do not contain compatible TrueView command data.
- Controls recorded voice for everyone, T only, CT only, or individual players across all 64 display slots.
- Parses recorded `VoiceData` before launch and shows active speakers by Demo tick without modifying the Demo or patching `client.dll`.
- Switches to a live player's first-person POV after validating the target.
- Jumps directly to the beginning of any recorded round.
- Supports English and Simplified Chinese: the launcher follows the system language, while the in-game DemoUI automatically follows the CS2 interface language.
- Checks the latest official GitHub Release in the background and shows only a small, dismissible notice. Launcher and DemoUI updates remain separate choices; nothing is downloaded automatically.
- Keeps installation reversible with backups, an isolated staging directory, and explicit cleanup.

## Quick Start

Download and extract `SwiftDemoUIPro-v<version>-win64.zip`, then:

1. Run `SwiftDemoUIPro.exe` from the extracted folder. Keep the whole package together.
2. Select or drag in a `.dem` file or `.zip` archive.
3. Confirm the automatically detected CS2 installation.
4. Leave **TrueView prediction** disabled for most downloaded or third-party Demos. Enable it only when the recording is known to support TrueView.
5. Select **Start playback**. The launcher installs the DemoUI for this session and starts CS2.
6. When finished, fully exit CS2, return to the launcher, and select **Stop and restore**.

If the launcher was interrupted, reopen it to resume the pending cleanup.

## Using the In-Game Panel

- Use CS2's native Demo mouse-mode hotkey to show the cursor.
- Click a live player's name to switch to first-person POV; the current POV is highlighted in gold.
- Click the audio button on a player row to toggle only that player's recorded voice.
- Use **HEAR ALL**, **MUTE ALL**, **T ONLY**, or **CT ONLY** for quick voice filtering; these labels are shown in English or Chinese according to the CS2 language.
- The lower-left speaker HUD follows seeking, pause, and playback speed because it is driven by the current Demo tick rather than a live microphone callback.
- Expand **ROUND NAVIGATION** and select a round to jump to its starting tick.
- The panel uses a fixed position that leaves a clear lane for the right-side weapon slots. Click anywhere on the title bar to expand or collapse it.

## Common Problems

| Symptom | What to do |
| --- | --- |
| The picture flickers or the console repeats `Not enough TrueView command lookahead` | Stop playback, disable **TrueView prediction**, and start the Demo again. |
| Windows reports a missing `Qt6Gui.dll` | Run the EXE from the extracted release package, not from the raw `launcher\build\Release` directory. |
| The launcher says cleanup is pending | Fully exit CS2, reopen the launcher if necessary, and select **Stop and restore**. |
| Some or all player voices are unavailable | The launcher can only play voice packets stored in the Demo; missing recordings cannot be recovered. |
| Voice plays but the lower-left speaker HUD stays empty | Start playback through the complete launcher package. A standalone DemoUI VPK has only an empty fallback index, and older packages do not include `swift-demo-voice-indexer.exe`. |

## Compatibility and Safety

- FACEIT Demos commonly contain recorded voice, but older, damaged, or differently sourced Demos may not.
- ZIP entries must be unencrypted and use a compression method supported by the bundled miniz library. Extracted Demos are limited to 8 GB.
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
