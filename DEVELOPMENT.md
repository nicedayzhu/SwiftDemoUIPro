# Swift DemoUI Pro Developer Guide

[English](DEVELOPMENT.md) | [简体中文](DEVELOPMENT_CN.md) | [Player README](README.md)

This guide contains the technical material for building, testing, packaging, and releasing Swift DemoUI Pro. Player installation and usage are documented in the main [README](README.md).

## Architecture

Swift DemoUI Pro has three cooperating components:

1. A Panorama override that extends Valve's native `huddemocontroller` with recorded-voice controls, parsed speaker status, validated POV switching, and round navigation.
2. A C++17/Qt 6 Widgets launcher that discovers CS2, accepts `.dem` and `.zip` files, stages an isolated playback session, starts CS2 with `-insecure`, and restores project-owned changes afterward.
3. A Rust `swift-demo-voice-indexer` sidecar that reads `SvcVoiceData`, emits a compact tick/slot Panorama data script, compiles it into a minimal Source 2 `vjs` Version 4 resource, and writes the per-Demo session VPK without rewriting the source Demo.

ZIP reading is compiled into the launcher from the vendored miniz source. It enumerates archive entries in process and streams only the selected `.dem`; no external extraction executable is bundled or required.

## Prerequisites

### Static Panorama/VPK developer build

- Windows PowerShell.
- A current CS2 installation containing Valve's `resourcecompiler.exe`, only for developer builds of the static DemoUI VPK.
- [VPKEdit](https://github.com/craftablescience/VPKEdit) command-line tools, only for developer packing/inspection of the static DemoUI VPK.
- Node.js for Panorama tests.

### Qt launcher

- Qt 6.5 or newer with a 64-bit MSVC Desktop kit.
- Visual Studio with the **Desktop development with C++** workload.
- CMake on `PATH` or installed by Qt/Visual Studio.
- A stable Rust toolchain with Cargo. The launcher build compiles and tests the voice indexer.

### GitHub publication

- Git and a clean working tree.
- [GitHub CLI](https://cli.github.com/) authenticated with `gh auth login`.
- A configured `origin` remote.

Run commands from the repository root. Pass machine-specific paths explicitly in reproducible commands.

## Build and Test

### Panorama tests

```powershell
node .\tests\test_demo_voice_mask.js
```

This covers voice-mask generation, parsed voice-pulse lookup, player discovery and status behavior, POV commands, round intervals, required native DemoUI layout integration, and matching English/Chinese Panorama localization catalogs.

The Rust parser can also be tested directly:

```powershell
cargo test --locked --manifest-path .\tools\voice-indexer\Cargo.toml
```

### Build the Panorama VPK

```powershell
.\demo-menu.ps1 -Action Build `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>"
```

Output:

```text
dist\swift_demo_menu_override.vpk
```

Other lifecycle actions are `Compile`, `Pack`, `Install`, and `Uninstall`. `Install`, `Uninstall`, and `-InstallLocalOverride` modify the local CS2 installation; use them only when that is explicitly intended. Fully restart CS2 after installing, updating, or uninstalling compiled Panorama resources.

### Compile and test the launcher

For a CI-like build that does not require a VPK:

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -SkipVpkCheck
```

The script tests and builds the Rust voice indexer, configures CMake, builds the launcher and translations, and runs CTest. It does not run `windeployqt`. Therefore, `launcher\build\Release\SwiftDemoUIPro.exe` is a raw build artifact and is not a standalone application; launching it outside a configured Qt development environment may report a missing `Qt6Gui.dll` or platform plugin.

For an existing build tree, tests can also be run with:

```powershell
ctest --test-dir .\launcher\build -C Release --output-on-failure
```

### Create a runnable package

Build the VPK first, then run:

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -Package
```

Outputs:

```text
launcher\package\SwiftDemoUIPro-v<version>\SwiftDemoUIPro.exe
launcher\package\SwiftDemoUIPro-v<version>-win64.zip
```

Packaging uses `windeployqt`. The output must contain the launcher, `swift-demo-voice-indexer.exe`, Qt runtime DLLs, `platforms\qwindows.dll`, the DemoUI VPK, translations, README files, the project license, third-party notices, and dependency license texts. The player package does not contain or require Valve's ResourceCompiler, the Workshop Tools DLC, or VPKEdit. For end-to-end testing, run the EXE from the unpacked version directory and keep the directory intact.

See the [Launcher Guide](launcher/README.md) for its workflow, localization, ZIP protections, and cleanup boundaries.

## Verification Matrix

| Change | Minimum verification |
| --- | --- |
| Panorama JavaScript/layout/style/localization | Run `node .\tests\test_demo_voice_mask.js`; build the VPK when CS2 tools are available and confirm that the raw localization files are present in it. |
| Rust voice indexer, VJS_C writer, VPK writer, or Demo voice schema | Run Cargo tests and parse a known voice-bearing Demo; verify packet, slot, tick, Source 2 resource header, and VPK contents. |
| Launcher core, ZIP, SearchPath, staging, launch, or cleanup | Build the launcher and run CTest; add or update `tst_launcher_core.cpp`. |
| Launcher UI/QSS/dialogs | Build and test, then inspect an actual rendered or interactive state in each affected language. |
| Translation strings | Update `.ts`, complete new translations, build `.qm`, and inspect layout/placeholders. |
| PowerShell/CMake logic | Run the affected command end to end and inspect its artifact. |
| Packaging/licenses | Open the ZIP and verify required runtime files, notices, and license texts. |
| Release logic | Create the matching full or `-MenuOnly` local candidate without `-Publish`; verify every listed SHA-256 entry and `update-manifest.json`. |
| Documentation only | Validate relative links and assets, then run `git diff --check`. |

GitHub Actions runs portable Panorama, Rust, and Qt tests on Windows Server 2022. The Panorama test is fully repository-contained and protects the imported native DemoUI root with a canonical SHA-256; it does not read a sibling `res_panorama` checkout. Cargo tests fully exercise the runtime VJS_C and session-VPK writers. The hosted workflow intentionally does not rebuild the static DemoUI VPK because Valve's `resourcecompiler.exe` and its matching runtime files come from a current CS2 installation.

[DepotDownloader](https://github.com/SteamRE/DepotDownloader) can technically fetch App 730 content, supports anonymous access where Valve permits it, and can restrict downloads with `-filelist`. A GitHub-hosted VPK build is therefore possible in principle, but it would depend on Valve's changing depot layout, the complete current ResourceCompiler dependency set, Steam availability, and third-party downloader bootstrap. Prefer the existing local build or a controlled self-hosted Windows runner with CS2 installed; do not make ordinary CI depend on downloading game depots.

## Localization

### Qt launcher

- Keep C++ source and fallback strings in English; use Qt translation APIs for user-visible text.
- Keep English and Simplified Chinese documentation semantically aligned.
- Preserve Qt placeholders such as `%1` and `%2`, as well as newlines, commands, and paths.
- Update translation sources after configuring CMake:

  ```powershell
  cmake --build .\launcher\build --target SwiftDemoUIPro_lupdate --config Release
  ```

- Translate every new unfinished entry in `launcher/translations/swift_demoui_pro_zh_CN.ts`.
- Build the launcher to generate `.qm` output; generated `.qm` files are not committed.
- Use `--ui-language en` and `--ui-language zh_CN` for visual language checks.

### In-game Panorama DemoUI

- CS2 and Dota 2 addons use different loading conventions. This project uses the CS2-loaded `addon/resource/platform_english.txt` and `platform_schinese.txt`, which become `resource/platform_<language>.txt` inside the VPK. The localization system falls back to English when a matching language catalog is unavailable.
- Static XML text uses `#SwiftDemoVoice_*` tokens. Dynamic JavaScript text must go through `_Localize()` (which calls `$.Localize`) instead of assigning a token directly to `.text`.
- Dynamic numbers and strings use Panorama dialog variables such as `{d:count}` and `{s:player}`. Add or remove tokens in both catalogs together; the test rejects missing, mismatched, or unused entries.
- Localization files are runtime `game` resources and are not ResourceCompiler inputs. The build script copies them as raw UTF-8-with-BOM files to `game/csgo_addons/swift_demo_menu_override/resource/` before packing the VPK.
- The DemoUI has no separate language selector. It follows the CS2 interface language automatically; fully restart CS2 when testing a language change.

## Versioning and Local Releases

The two root version files use `MAJOR.MINOR.PATCH` independently:

- [VERSION](VERSION) is the launcher/package version used by CMake, Windows metadata, and launcher asset names.
- [MENU_VERSION](MENU_VERSION) is the DemoUI VPK version embedded in the launcher and used by standalone VPK assets.

The launcher also embeds the current short Git commit. Its updater reads GitHub's latest published Release and then `update-manifest.json`, which always describes both components even when only one changed.

The working tree must be clean before building a release candidate because the source archive is created from `HEAD`:

```powershell
.\release.ps1 `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt Desktop kit>"
```

This runs both test suites, rebuilds the VPK, builds/tests/packages the launcher, creates a Git source archive, and writes:

```text
release\v<version>\SwiftDemoUIPro-v<version>-win64.zip
release\v<version>\SwiftDemoUIPro-v<version>-source.zip
release\v<version>\swift_demo_menu_override-v<menu-version>.vpk
release\v<version>\update-manifest.json
release\v<version>\SHA256SUMS.txt
```

Use `build-launcher.ps1` while testing uncommitted changes. Commit the completed work before running `release.ps1`; stashed changes are not included in the source archive.

## GitHub Setup and Publication

Initial repository setup:

```powershell
gh auth login
gh repo create nicedayzhu/SwiftDemoUIPro --public --source . --remote origin --push
```

To publish a full launcher release (optionally advancing the DemoUI version in the same release):

```powershell
.\release.ps1 -Version 0.2.0 -MenuVersion 0.1.1 `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt Desktop kit>" `
  -Publish
```

To publish only a new DemoUI VPK without rebuilding or republishing the launcher:

```powershell
.\release.ps1 -MenuOnly -MenuVersion 0.1.2 `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -Publish
```

Full publication may update both version files and creates `v<launcher-version>`. `-MenuOnly` updates only `MENU_VERSION`, skips the Qt build, and creates `menu-v<menu-version>`. Both modes build/test the VPK, create a standalone versioned VPK, generate `update-manifest.json` plus `SHA256SUMS.txt`, stage a draft GitHub Release, and publish only after all uploads succeed. The manifest carries forward the standard `v<VERSION>` launcher URL for a menu-only release, allowing the latest-release API to describe both independent components.

Verify downloaded files with:

```powershell
Get-FileHash .\SwiftDemoUIPro-v0.2.0-win64.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

Do not manually edit generated archives or checksums.

## Repository Layout

| Path | Purpose |
| --- | --- |
| `addon/` | Panorama layout, JavaScript, styles, and raw localization text packed into the override VPK. |
| `launcher/` | Qt launcher source, tests, translations, vendored miniz, and packaging script. |
| `powershell/` | Shared Panorama compile, pack, install, and uninstall implementation. |
| `tests/` | Panorama logic and native-layout integration tests. |
| `demo-menu.ps1` | Repository-level Panorama lifecycle entry point. |
| `release.ps1` | Versioned local release and optional GitHub publication entry point. |
| `.github/workflows/ci.yml` | Portable Windows CI. |
| `VERSION` | Launcher/package semantic version. |
| `MENU_VERSION` | Independent DemoUI VPK semantic version. |

Generated paths such as `dist/`, `release/`, `launcher/build/`, `launcher/package/`, `launcher/.qt/`, and `launcher/.tools/` must not be committed.

## Contribution and Commit Rules

- Keep C++ compatible with C++17 and Qt 6.5 or newer.
- Keep filesystem/process behavior in `Cs2Manager` and presentation in `LauncherWindow`.
- Preserve exact, idempotent, and reversible SearchPath and cleanup behavior.
- Keep player-facing documentation bilingual and translate new launcher strings in the same change.
- Run `git diff --check`, review `git status --short`, and run the relevant tests before committing.
- Use Conventional Commits, for example `feat(launcher): support demos in ZIP archives` or `docs(readme): add launcher preview`.
- Do not commit generated build, package, translation-binary, or release output.

Coding agents should also follow [AGENTS.md](AGENTS.md), which records the project's safety invariants and operational rules.

## Licensing and Redistribution

- Original project code is MIT licensed; keep [LICENSE](LICENSE) intact.
- miniz is vendored under MIT in `launcher/third_party/miniz/`.
- Noto Sans SC is distributed under the SIL Open Font License 1.1.
- Packaged Qt libraries are dynamically linked under `LGPL-3.0-only`; keep them replaceable and retain the Qt notice/license text.
- VPKEdit is MIT licensed and redistributable with its copyright/license notice, but it is only a static-VPK build/inspection tool here and is not bundled in the release package. Runtime session VPKs are written by the bundled Rust sidecar.
- Valve game resources, names, and trademarks are not relicensed by this repository's MIT license.

Keep [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) and [launcher/THIRD_PARTY_NOTICES.txt](launcher/THIRD_PARTY_NOTICES.txt) aligned with dependency or distribution changes.
