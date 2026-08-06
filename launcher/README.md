# Swift DemoUI Pro Launcher

[English](README.md) | [简体中文](README_CN.md) | [Project overview](../README.md)

A lightweight Windows Qt 6 Widgets application that safely installs this project's Panorama DemoUI and opens Counter-Strike 2 `.dem` files directly or from downloaded `.zip` archives. It does not include a 2D replay viewer, demo parser, or network service.

## Interface Preview

![Swift DemoUI Pro launcher with Demo and ZIP selection and TrueView compatibility control](../docs/images/launcher-playback-ui.png)

The launcher keeps the playback workflow on one page: choose a Demo, review the compatibility option, start CS2, then restore the temporary session when finished.

## Workflow

1. Select or drag in a `.dem` file or `.zip` archive. A ZIP containing one Demo is selected automatically; if it contains several, choose one from the displayed list.
2. Confirm the automatically detected CS2 path, or choose a different installation.
3. Leave **TrueView prediction** off for maximum compatibility. This writes `cl_demo_predict 0` to the temporary CFG and prevents prediction flicker in Demos without TrueView command data. Enable it only for a supported recording; the preference is remembered.
4. Select **Start Watching Demo**. The launcher installs/verifies the VPK, copies the Demo—or streams only the selected ZIP entry—into a dedicated staging directory, creates `swift_demo_launcher.cfg`, and runs:

   ```text
   steam.exe -applaunch 730 -insecure -novid +exec swift_demo_launcher.cfg
   ```

5. When finished, fully exit CS2 before selecting **Stop Watching Demo**. The launcher removes the exact SearchPath it owns, the VPK, temporary CFG, session marker, and staged demo.

If the console repeatedly reports `Not enough TrueView command lookahead` or the picture flickers, stop playback, turn **TrueView prediction** off, and start the Demo again.

`-insecure` is used only by that launch command and is not written to Steam's permanent launch options. The DemoUI VPK SearchPath remains active until cleanup completes, so do not skip the final step. If the launcher was interrupted, reopen it to resume cleanup.

## Updates

After startup, the launcher checks the repository's latest published GitHub Release. An available update produces a small, dismissible notification only; it never downloads or installs anything without the player's action. The **About** page then offers separate choices:

- A launcher update opens the versioned Windows package in the browser. The running EXE is never replaced in place.
- A DemoUI-only update downloads the versioned VPK after the player selects it, verifies its SHA-256 digest, and stores it in the user's local application-data directory. It becomes the preferred VPK the next time Demo playback starts.

Release metadata comes from `update-manifest.json`, which tracks launcher and DemoUI versions independently. Older releases without a manifest remain compatible for launcher update detection through their `SwiftDemoUIPro-v<version>-win64.zip` asset name.

Update checks read the public GitHub REST API anonymously and neither require nor store a player's GitHub token. Requests include GitHub's required `User-Agent`, recommended JSON `Accept`, and API-version headers, validate the HTTP status, content type, JSON, timeout, and anonymous rate limit, and retry invalid JSON once.

## Build

For the complete project build, test, versioning, and release workflow, see the [Developer Guide](../DEVELOPMENT.md).

Use Qt 6.5 or newer with a 64-bit MSVC Desktop kit, Visual Studio C++ tools, and CMake. Run all commands from the repository root.

### Compile and test

For a fast CI-like build that does not require the Panorama VPK:

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -SkipVpkCheck
```

The script configures CMake, builds the launcher and translations, and runs CTest. `-SkipVpkCheck` is only for a non-packaging build. It does not call `windeployqt`, so `launcher\build\Release\SwiftDemoUIPro.exe` depends on the Qt development environment and is not intended to be launched by double-clicking. A missing `Qt6Gui.dll` message means the raw build output was used instead of a deployed package.

### Create a runnable package

Build the VPK and then package the launcher:

```powershell
.\demo-menu.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe"

.\launcher\build-launcher.ps1 -QtRoot "C:\Qt\6.8.3\msvc2022_64" -Package
```

Package outputs:

```text
launcher\package\SwiftDemoUIPro-v<version>\SwiftDemoUIPro.exe
launcher\package\SwiftDemoUIPro-v<version>-win64.zip
```

Run the EXE from the unpacked version directory for local end-to-end testing. The packaging step uses `windeployqt` to collect `Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Network.dll`, `Qt6Widgets.dll`, the Windows TLS backend, and `platforms\qwindows.dll`. `SwiftDemoUIPro.exe`, its DLLs/plugins, translations, and `swift_demo_menu_override.vpk` must remain together.

The repository-level `release.ps1` command is for committed release candidates, not ordinary development testing. It refuses a dirty Git working tree because the source archive is generated from `HEAD`.

ZIP support is compiled into `SwiftDemoUIPro.exe` from the vendored miniz 3.1.2 source, so users do not need 7-Zip, PowerShell extraction, or another executable. The package includes `licenses/miniz-MIT.txt`.

## Localization and Typography

- Application source and fallback strings are English. **Follow system** automatically selects Simplified Chinese on Chinese systems and falls back to English when no matching translation is available.
- Users can choose **Follow system**, **简体中文**, or **English** from the sidebar. The selection is stored locally.
- Translation sources are named `translations/swift_demoui_pro_<locale>.ts`. CMake discovers matching files, compiles external `.qm` files, and the application scans them at startup.
- After changing source strings, build CMake's `update_translations` target and edit the `.ts` file in Qt Linguist. Preserve placeholders such as `%1` and `%2`.

The interface embeds the variable Noto Sans SC font to provide consistent Chinese and Latin weights and line heights. It is distributed under the SIL Open Font License 1.1.

## Safety Boundaries

- The launcher never edits Steam's permanent launch options.
- It refuses to install, replace, or remove the VPK while CS2 is running.
- Before the first `gameinfo.gi` modification, it creates `gameinfo.gi.swift_demo_launcher.restore.bak`.
- Cleanup removes only the project's exact SearchPath and owned temporary files; it does not overwrite unrelated user or tool changes.
- A persistent session marker allows the application to warn about and recover an interrupted cleanup.
- ZIP handling enumerates entries without expanding the archive, streams only the selected `.dem` to the owned `current.dem`, verifies the uncompressed size and CRC, reserves free disk space, and enforces an 8 GB safety limit.
- Encrypted ZIP entries and unsupported compression methods are rejected with an error; unrelated entries are never written to disk.
- Demo playback runs with `-insecure` and cannot be used for normal matchmaking.

## Third-Party Components and License

- The project launcher source is licensed under the repository's [MIT License](../LICENSE).
- Qt 6 is dynamically linked under `LGPL-3.0-only`; packaged DLLs remain replaceable.
- Noto Sans SC is included under `OFL-1.1`.
- miniz 3.1.2 is statically compiled into the launcher under the MIT License.
- The launch flow was informed by the MIT-licensed [drjackild/cs2-demo-opener](https://github.com/drjackild/cs2-demo-opener); none of its replay, parser, web, or source files are included.

See [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) and the packaged `licenses` directory for the full notices and license texts.

## Support

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/K6C623WHCQ)
