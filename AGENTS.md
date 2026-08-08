# Swift DemoUI Pro Agent Guide

This file applies to the entire repository. It is the operational reference for coding agents working on Swift DemoUI Pro. Keep it aligned with the scripts and architecture whenever those change.

## Project Background

Swift DemoUI Pro is an unofficial, client-side enhancement for Counter-Strike 2 Demo and HLTV playback. It has two cooperating parts:

1. A Panorama override that extends Valve's native `huddemocontroller` with recorded-voice controls, player POV switching, and direct round navigation.
2. A native Qt 6 Widgets launcher that detects CS2, accepts `.dem` files or ZIP archives, installs the override for an isolated playback session, launches CS2 with `-insecure`, and removes the temporary changes afterward.

The project does not require SwiftlyS2, a server plugin, a Workshop item, or a running server. It is not affiliated with or endorsed by Valve or FACEIT.

The launcher must never imply that an `-insecure` session is suitable for matchmaking. It does not change permanent Steam launch options.

## Non-Negotiable Safety Invariants

- Never launch the Demo workflow while CS2 is already running. `-insecure` must be applied when the launcher starts a new process.
- Back up `gameinfo.gi` before the first modification and preserve its detected encoding.
- SearchPath modification must remain exact, idempotent, and reversible. Remove only the line owned by this project.
- Use atomic writes/copies for `gameinfo.gi`, the staged Demo, CFG files, session markers, and installed VPK whenever possible.
- Cleanup must refuse to proceed while CS2 is running and must delete only project-owned paths. Keep the explicit staged-directory suffix check before recursive deletion.
- Preserve the user's unrelated CS2 files, SearchPaths, Steam launch options, settings, and installed overrides.
- Do not run `demo-menu.ps1 -Action Install`, `-Action Uninstall`, or `-InstallLocalOverride` during ordinary compilation/testing unless the user explicitly requests a local CS2 installation change.
- Do not run `release.ps1 -Publish`, create/push tags, push commits, or publish a GitHub Release without explicit user authorization.
- Automatic update checks may only show a dismissible notice. Never download a launcher/VPK, open a browser, or change game files until the player selects the corresponding action.
- Keep launcher updates browser-mediated. A downloaded DemoUI VPK must use HTTPS, pass its published SHA-256 digest, and be saved atomically under the user's application-data directory before it can become the preferred source.

## Repository Map

| Path | Responsibility |
| --- | --- |
| `addon/panorama/layout/hud/huddemocontroller.xml` | Native-compatible DemoUI layout override. Preserve required native controls and IDs. |
| `addon/panorama/scripts/hud/swift_demo_voice.js` | Player discovery, 64-slot voice masks, POV validation, and round navigation. Runs in Panorama, not Node.js. |
| `addon/panorama/styles/hud/swift_demo_voice.css` | In-game Demo Voice panel styling. |
| `powershell/` | Shared compile, VPK pack, install, and uninstall implementation. |
| `demo-menu.ps1` | Root entry point for Panorama compile/pack/install lifecycle actions. |
| `launcher/src/Cs2Manager.*` | Steam/CS2 discovery, SearchPath lifecycle, ZIP inspection/extraction, session staging, launch, and cleanup. |
| `launcher/src/LauncherWindow.*` | Qt UI construction, state refresh, dialogs, localization, and user actions. |
| `launcher/src/UpdateService.*` | GitHub latest-release/manifest parsing, independent component comparison, verified VPK download, and local update cache selection. |
| `launcher/src/main.cpp` | Qt application entry point and visual-preview command-line options. |
| `launcher/tests/tst_launcher_core.cpp` | Qt tests for Steam parsing, safe SearchPath edits, isolated sessions, ZIP behavior, and launch arguments. |
| `launcher/third_party/miniz/` | Vendored miniz source and MIT license used for in-process ZIP reading. |
| `launcher/translations/` | Qt Linguist `.ts` translation sources. Generated `.qm` files are build outputs. |
| `tests/test_demo_voice_mask.js` | Node-based Panorama logic and native-layout integration tests. |
| `VERSION` | Launcher/package semantic version used by CMake, UI, Windows resources, and launcher asset names. |
| `MENU_VERSION` | Independent DemoUI VPK semantic version used by the updater and standalone VPK assets. |
| `release.ps1` | Full or `-MenuOnly` local release-candidate build and optional GitHub publication entry point. |
| `.github/workflows/ci.yml` | Portable Windows CI for JavaScript and Qt tests; it intentionally does not build the VPK. |
| `README.md`, `README_CN.md` | Concise player-facing English and Simplified Chinese documentation; keep them synchronized. |
| `DEVELOPMENT.md`, `DEVELOPMENT_CN.md` | Developer-facing build, test, localization, versioning, release, and contribution documentation. |
| `docs/images/` | Screenshots referenced by public documentation; keep relative links portable. |

## Architecture and Behavior

### Panorama override

- The override augments the native DemoUI instead of replacing its timeline, playback controls, settings, and camera-mode behavior.
- Display slots 1-32 map to `tv_listen_voice_indices`; slots 33-64 map to `tv_listen_voice_indices_h`. JavaScript bitwise results must remain signed 32-bit console values.
- `BuildMasksForSlots` accepts normalized zero-based slots and produces the low/high masks. Do not introduce off-by-one conversions between display slots and internal slots.
- Player rows are derived from `GameStateAPI` with fallbacks for slot, team, name, and status. Dead/disconnected players may retain voice toggles but must not offer misleading POV actions.
- POV switching verifies account ID, slot, name, and observed target; preserve the fallback/verification sequence around `spec_lock_to_accountid` and `spec_player`.
- Round navigation uses native `RoundIntervals` and jumps to `nTickStart` through the native Demo controller.
- Panorama JavaScript must stay compatible with the game runtime. Do not add Node/browser-only APIs or a bundler requirement.

### Qt launcher

- The launcher is C++17 with Qt 6 Core, Gui, Widgets, Test, and LinguistTools.
- UI source strings are English. Chinese is supplied through Qt Linguist.
- `Cs2Manager` owns filesystem/process behavior; `LauncherWindow` owns presentation and user interaction. Keep risky filesystem operations out of UI handlers.
- `.dem` files are copied atomically into the fixed staging area.
- ZIP files are opened in process through statically linked miniz. The launcher lists only `.dem` entries and streams only the selected entry to a fixed staged path; it never expands the full archive.
- Preserve ZIP protections: reject encrypted/unsupported/invalid entries, empty Demos, size mismatches, insufficient disk space, and extracted Demos larger than 8 GB.
- A ZIP with one Demo is selected automatically; multiple Demos require explicit selection.
- The launcher defaults TrueView prediction off by writing `cl_demo_predict 0` to the session CFG. Keep this compatibility default unless the player explicitly enables TrueView for a supported recording.
- The UI must remain readable under Windows light and dark system palettes. When changing QSS, explicitly style dialog child labels/buttons and visually check important states.
- Existing QA options in `main.cpp` include `--ui-language`, `--preview-page`, `--preview-update-bubble`, `--no-update-check`, and `--render-preview`.
- Update checks use the public GitHub latest-release API. `update-manifest.json` is authoritative for independent launcher/menu versions; legacy releases fall back to the versioned launcher asset name.

## Prerequisites

### Static Panorama/VPK developer build

- Windows PowerShell.
- A current CS2 installation containing `resourcecompiler.exe`, only for developer builds of the static DemoUI VPK.
- VPKEdit CLI for developer packing/inspection of the static DemoUI VPK. Player runtime session VPKs are written by the bundled Rust sidecar.
- Node.js for the Panorama tests.

### Qt launcher

- Qt 6.5 or newer with a 64-bit MSVC Desktop kit.
- Visual Studio with the Desktop development with C++ workload.
- CMake on `PATH` or installed by Qt/Visual Studio.

### GitHub publication

- Git and a clean working tree.
- GitHub CLI (`gh`) authenticated with `gh auth login`.
- A configured `origin` remote.

The default CS2 and VPKEdit paths in `demo-menu.ps1` are developer-machine conveniences, not portable project assumptions. In documentation, automation, and reproducible commands, pass `-Cs2Root`, `-VpkEditCli`, and `-QtRoot` explicitly.

## Build Commands

Run commands from the repository root unless stated otherwise.

### Fast Panorama test

```powershell
node .\tests\test_demo_voice_mask.js
```

This test covers mask generation, player discovery/status behavior, POV commands, `RoundIntervals`, and required native DemoUI layout integration.

### Compile and pack the Panorama VPK

```powershell
.\demo-menu.ps1 -Action Build `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>"
```

Output:

```text
dist\swift_demo_menu_override.vpk
```

Available lifecycle actions are `Build`, `Compile`, `Pack`, `Install`, and `Uninstall`. Prefer `Build` for a normal artifact build. `Install`, `Uninstall`, and `-InstallLocalOverride` modify the local CS2 installation and require explicit user intent.

### Build and test the launcher without a VPK

Use this for CI-like launcher work:

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -SkipVpkCheck
```

The script configures CMake with tests enabled, builds the requested configuration, and runs CTest. `-SkipVpkCheck` is for a non-packaging build only.

Do not tell users to double-click `launcher/build/Release/SwiftDemoUIPro.exe`. That directory is a raw build tree and does not contain the Qt runtime deployed by `windeployqt`; launching it outside a configured Qt environment can fail with a missing `Qt6Gui.dll` or platform-plugin error.

### Build and package the launcher

Build the VPK first, then run:

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -Package
```

Output:

```text
launcher\package\SwiftDemoUIPro-v<version>\SwiftDemoUIPro.exe
launcher\package\SwiftDemoUIPro-v<version>-win64.zip
```

Packaging uses `windeployqt` and must include the launcher EXE, Qt Core/Gui/Network/Widgets DLLs, the Windows TLS backend, `platforms/qwindows.dll`, VPK, translations, README files, project license, third-party notices, Qt LGPL text, Noto Sans SC OFL text, and miniz MIT text. Launch the EXE from the unpacked package directory for end-to-end testing.

## Testing Expectations

Choose tests in proportion to the change, but do not skip relevant coverage:

| Change | Minimum verification |
| --- | --- |
| Panorama JS/layout/style | `node .\tests\test_demo_voice_mask.js`; build the VPK when CS2 tools are available. |
| `Cs2Manager`, ZIP, SearchPath, staging, launch, cleanup | Launcher build plus CTest. Add/update `tst_launcher_core.cpp`. |
| Launcher UI/QSS/dialogs | Launcher build plus CTest and an actual rendered screenshot or interactive check in each affected language/state. |
| Translation strings | Update `.ts`, complete every new translation, build `.qm`, and visually inspect placeholders/layout. |
| PowerShell/CMake/build logic | Parse scripts, run the affected command end to end, and inspect its produced artifact. |
| Packaging/license changes | Open the ZIP and verify required files and license texts. |
| Release changes | Run the matching full or `-MenuOnly` local candidate without `-Publish`; verify archives, standalone VPK, `update-manifest.json`, and every SHA256 entry. |
| Documentation only | Validate relative links/assets and run `git diff --check`. |

Useful direct CTest command for an existing build tree:

```powershell
ctest --test-dir .\launcher\build -C Release --output-on-failure
```

GitHub CI uses Windows Server 2022, Node 22, Qt 6.8.x/MSVC 2022, and `-SkipVpkCheck`. Cargo tests cover the runtime VJS_C/session-VPK writers. CI intentionally does not rebuild the static DemoUI VPK: obtaining the matching proprietary CS2 ResourceCompiler toolchain on every hosted run would make ordinary CI dependent on Valve's changing depot layout and Steam availability.

## Localization Workflow

- Keep translatable C++ source text in English and use `tr(...)` or `QCoreApplication::translate(...)`.
- Keep `README.md` and `README_CN.md` semantically synchronized for player-facing changes, and keep technical detail in the matching `DEVELOPMENT` documents.
- Preserve Qt placeholders exactly (`%1`, `%2`, and so on), newlines, command names, and paths.
- After configuring CMake, update translation sources with:

  ```powershell
  cmake --build .\launcher\build --target SwiftDemoUIPro_lupdate --config Release
  ```

- Translate every new `type="unfinished"` entry in `launcher/translations/swift_demoui_pro_zh_CN.ts`.
- Build the launcher to run `lrelease`; do not edit or commit generated `.qm` files.
- Language testing can use `--ui-language en` or `--ui-language zh_CN`.

## Versioning and Release Process

`VERSION` and `MENU_VERSION` are independent `MAJOR.MINOR.PATCH` sources. `VERSION` controls the launcher/package version; `MENU_VERSION` controls the DemoUI VPK version. CMake generates `AppVersion.h` with both versions plus the current short Git commit, and generates Windows metadata from the launcher version.

### Local release candidate

The working tree must be clean because the source archive is generated from `HEAD`:

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

It does not create a tag or contact GitHub.

### GitHub Release

Only with explicit user authorization:

```powershell
.\release.ps1 -Version <next-version> `
  -Cs2Root "<CS2 root>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt Desktop kit>" `
  -Publish
```

Full publishing may update `VERSION` and `MENU_VERSION`, create `chore(release): v<version>`, build/test all artifacts, create an annotated `v<version>` tag, push the current branch and tag, create or update a draft GitHub Release, upload both ZIPs, the standalone VPK, `update-manifest.json`, and `SHA256SUMS.txt`, then publish the Release. It refuses to overwrite an already published Release.

For a DemoUI-only release, use `release.ps1 -MenuOnly -MenuVersion <next-menu-version> ... -Publish`. This updates only `MENU_VERSION`, skips the Qt build, creates `menu-v<menu-version>`, and uploads the versioned VPK, `update-manifest.json`, and checksums. It still requires explicit publication authorization.

Do not manually edit generated release archives or their checksums. Generate checksums after the final packaging step.

## Generated and Ignored Files

These paths are outputs or local tooling and must not be committed:

```text
dist/
release/
launcher/build/
launcher/package/
launcher/.qt/
launcher/.tools/
native_current/
native_current_decompiled/
```

Do not edit generated `launcher/build/generated/AppVersion.h`, generated Windows resources, `.qm` files, compiler output, deployed Qt DLLs, or release archives as source. Edit their templates/sources instead.

## Licensing and Redistribution

- Original project code is MIT licensed; keep `LICENSE` intact.
- miniz is vendored under MIT in `launcher/third_party/miniz/`; keep its source version and license together.
- Noto Sans SC uses the SIL Open Font License 1.1.
- The packaged Qt runtime is dynamically linked under LGPL-3.0-only; retain the Qt license text and notices.
- Keep `THIRD_PARTY_NOTICES.md` and `launcher/THIRD_PARTY_NOTICES.txt` synchronized with dependency/distribution changes.
- VPKEdit is MIT licensed and redistributable with its notice, but remains a build-time tool and is not bundled in the release package. Runtime session VPKs do not use it.
- Valve game resources, names, and trademarks are not relicensed by this repository's MIT license. Treat game-derived UI changes and redistribution carefully.

## Coding and Editing Rules

- Prefer the codebase knowledge graph for symbol discovery and call/impact tracing when available; use `rg` for literals, generated files, scripts, and configuration.
- Preserve existing formatting: four spaces in C++/CMake and the local style of existing PowerShell/Panorama files.
- Keep C++ compatible with C++17 and Qt 6.5+.
- Prefer Qt path/file APIs, `QSaveFile`, normalized absolute paths, and explicit error propagation through `LauncherResult`.
- Do not weaken path-containment, file-size, disk-space, archive-integrity, process-state, or backup checks for convenience.
- Keep UI source English and update Chinese translations in the same change.
- Keep public documentation bilingual when user-visible behavior changes.
- Reuse existing scripts, package layouts, notice files, and assets instead of creating parallel workflows.
- Preserve unrelated user changes in a dirty working tree. Never use destructive Git commands to discard them.

## Git Commit Rules

- Do not create a commit unless the user requests it or the active task explicitly includes committing. Never push without explicit authorization.
- Before committing, inspect `git status --short`, review the diff, and run `git diff --check` plus the relevant tests.
- Do not commit ignored/generated artifacts. Confirm the worktree is clean after the commit.
- Use Conventional Commits:

  ```text
  <type>(<scope>): <imperative summary>
  ```

- Preferred types: `feat`, `fix`, `docs`, `test`, `refactor`, `build`, `ci`, `chore`.
- Preferred scopes: `launcher`, `menu`, `build`, `release`, `readme`, `translations`, `deps`.
- Examples:

  ```text
  feat(launcher): support demos in ZIP archives
  fix(launcher): keep dialogs readable in dark mode
  docs(readme): add interface preview
  test(menu): cover high voice-mask slots
  chore(release): v0.2.0
  ```

- Keep each commit focused on one coherent change. Avoid messages such as `update`, `fix stuff`, or `changes`.
- Do not amend, rewrite history, force-push, or delete tags unless the user explicitly requests it and the exact target has been verified.

## Agent Completion Checklist

Before handing off a change:

1. Re-read the affected safety invariants and confirm no user/game data can be touched unexpectedly.
2. Run the smallest complete relevant test matrix.
3. For UI changes, visually inspect the actual rendered state rather than relying only on compilation.
4. For packages, inspect archive contents and licenses.
5. Run `git diff --check` and review `git status --short`.
6. Update English/Chinese docs, translations, notices, version/release instructions, and this file when applicable.
7. Report exact tests, artifacts, remaining limitations, and commit hash (if committed).
