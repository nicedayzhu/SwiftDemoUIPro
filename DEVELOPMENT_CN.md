# Swift DemoUI Pro 开发者指南

[English](DEVELOPMENT.md) | [简体中文](DEVELOPMENT_CN.md) | [玩家 README](README_CN.md)

本文档集中说明 Swift DemoUI Pro 的架构、构建、测试、打包和发布流程。玩家安装与使用方法请阅读主 [README](README_CN.md)。

## 项目架构

Swift DemoUI Pro 由三个协作组件组成：

1. Panorama override：扩展 Valve 原生 `huddemocontroller`，加入已录制语音控制、解析后的说话状态、经过核验的 POV 切换和回合导航。
2. C++17/Qt 6 Widgets 启动器：检测 CS2，接收 `.dem`、`.zip` 和 `.dem.zst` 文件，准备独立回放会话，使用 `-insecure` 启动 CS2，并在结束后恢复本项目产生的修改。
3. Rust `swift-demo-voice-indexer` 辅助程序：流式解压 Zstandard Demo，只读 `SvcVoiceData`，输出紧凑的 tick/槽位 Panorama 数据脚本，将其编译为最小 Source 2 `vjs` Version 4 资源，并直接写入每个 Demo 的 session VPK；不会重写源 Demo。

ZIP 读取功能由仓库内置的 miniz 源码直接编译进启动器。程序会在进程内枚举压缩包，并且只流式写出玩家选择的 `.dem`；发布包无需携带或调用外部解压程序。

Zstandard 解码通过 `zstd-rs` 静态编译进 Rust 辅助程序。FACEIT `.dem.zst` 会流式写入项目自有的暂存 Demo，同时限制最大 8 GB 并校验 `PBDEMS2` 文件头；玩家无需安装 `zstd.exe` 或 Zstandard 运行库 DLL。

## 开发环境

### 静态 Panorama/VPK 开发构建

- Windows PowerShell。
- 当前版本的 CS2；只有开发者构建静态 DemoUI VPK 时才需要安装 Valve `resourcecompiler.exe`。
- [VPKEdit](https://github.com/craftablescience/VPKEdit) 命令行工具；只用于开发阶段打包/检查静态 DemoUI VPK。
- 用于 Panorama 测试的 Node.js。

### Qt 启动器

- Qt 6.5 或更高版本的 64 位 MSVC Desktop kit。
- 安装了“使用 C++ 的桌面开发”工作负载的 Visual Studio。
- 已加入 `PATH`，或由 Qt/Visual Studio 安装的 CMake。
- Rust stable 工具链与 Cargo。启动器构建脚本会编译并测试语音索引器。

### GitHub 发布

- Git，且工作区保持干净。
- [GitHub CLI](https://cli.github.com/)，并已通过 `gh auth login` 登录。
- 已配置 `origin` 远端仓库。

以下命令均从仓库根目录执行。可重复使用的命令应显式传入本机相关路径。

## 构建与测试

### Panorama 测试

```powershell
node .\tests\test_demo_voice_mask.js
```

该测试覆盖语音掩码生成、解析语音 pulse 查询、玩家发现与状态处理、POV 指令、回合区间、原生 DemoUI 必需布局，以及中英文 Panorama 本地化目录与 Token 一致性。

也可以单独测试 Rust 解析器：

```powershell
cargo test --locked --manifest-path .\tools\voice-indexer\Cargo.toml
```

### 构建 Panorama VPK

```powershell
.\demo-menu.ps1 -Action Build `
  -Cs2Root "<CS2 根目录>" `
  -VpkEditCli "<vpkeditcli.exe>"
```

输出：

```text
dist\swift_demo_menu_override.vpk
```

其他生命周期操作包括 `Compile`、`Pack`、`Install` 和 `Uninstall`。`Install`、`Uninstall` 与 `-InstallLocalOverride` 会修改本机 CS2 安装，只有明确需要时才应使用。安装、更新或卸载编译后的 Panorama 资源后，需要完整重启 CS2。

### 编译并测试启动器

如需执行不依赖 VPK、接近 CI 的构建：

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -SkipVpkCheck
```

脚本会测试并编译 Rust 语音索引器、配置 CMake、编译启动器和翻译，并运行 CTest。它不会执行 `windeployqt`，因此 `launcher\build\Release\SwiftDemoUIPro.exe` 只是裸编译产物，并非可独立运行的程序；在没有 Qt 开发环境时启动，可能提示缺少 `Qt6Gui.dll` 或平台插件。

已有构建目录也可以单独运行：

```powershell
ctest --test-dir .\launcher\build -C Release --output-on-failure
```

### 生成可运行包

先构建 VPK，再执行：

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -Package
```

输出：

```text
launcher\package\SwiftDemoUIPro-v<版本号>\SwiftDemoUIPro.exe
launcher\package\SwiftDemoUIPro-v<版本号>-win64.zip
```

打包过程使用 `windeployqt`。产物必须包含启动器、`swift-demo-voice-indexer.exe`、Qt 运行库、`platforms\qwindows.dll`、DemoUI VPK、翻译、README、项目许可证、第三方说明和依赖许可文本。玩家端不携带也不依赖 Valve ResourceCompiler、Workshop Tools DLC 或 VPKEdit。本地端到端测试请运行展开版本目录中的 EXE，并保持整个目录结构完整。

启动器流程、国际化、ZIP 防护与清理边界详见[启动器指南](launcher/README_CN.md)。

## 验证矩阵

| 修改范围 | 最低验证要求 |
| --- | --- |
| Panorama JavaScript/布局/样式/本地化 | 运行 `node .\tests\test_demo_voice_mask.js`；具备 CS2 工具时构建 VPK，并确认原始语言文件进入 VPK。 |
| Rust 语音索引器、Zstandard 解码器、VJS_C/VPK 写入器或 Demo 语音 schema | 运行 Cargo 测试，并解析一份确认含语音的 Demo；解码器修改还需解压真实 `.dem.zst` 并核对 `PBDEMS2` 文件头。 |
| 启动器核心、ZIP/Zstandard 选择、SearchPath、暂存、启动或清理 | 编译启动器并运行 CTest；新增或更新 `tst_launcher_core.cpp`。 |
| 启动器 UI/QSS/对话框 | 编译和测试后，在每种受影响语言下检查真实渲染或交互状态。 |
| 翻译文本 | 更新 `.ts`，完成新增翻译，构建 `.qm` 并检查布局与占位符。 |
| PowerShell/CMake 逻辑 | 完整运行受影响命令并检查实际产物。 |
| 打包/许可证 | 打开 ZIP，核对运行库、说明和许可证文本。 |
| Release 逻辑 | 不带 `-Publish` 生成对应的完整候选包或 `-MenuOnly` 候选包，核对所有 SHA-256 与 `update-manifest.json`。 |
| 仅文档 | 检查相对链接和图片资源，然后运行 `git diff --check`。 |

GitHub Actions 会在 Windows Server 2022 上运行可移植的 Panorama、Rust 与 Qt 测试。Panorama 测试已完全包含在仓库内，通过固定 SHA-256 保护导入的原生 DemoUI Root，不再读取相邻的 `res_panorama` 目录；Cargo 测试会完整覆盖运行时 VJS_C 与 session VPK 写入器。托管环境仍有意不重新构建静态 DemoUI VPK，因为 Valve `resourcecompiler.exe` 及其匹配的运行文件来自当前 CS2 安装。

[DepotDownloader](https://github.com/SteamRE/DepotDownloader) 在 Valve 允许时可以匿名获取 App 730 内容，也支持用 `-filelist` 限制下载文件。因此 GitHub 托管 runner 理论上可以构建 VPK，但会依赖 Valve 随时变化的 depot 结构、完整且同版本的 ResourceCompiler 依赖、Steam 可用性和第三方下载器引导。建议继续使用现有本机构建，或使用安装了当前 CS2 的受控 Windows 自托管 runner；不要让普通 CI 依赖下载游戏 depot。

## 国际化

### Qt 启动器

- C++ 源码和后备文本保持英文；用户可见文本使用 Qt 翻译 API。
- 英文和简体中文文档保持语义一致。
- 保留 `%1`、`%2` 等 Qt 占位符，以及换行、命令和路径。
- 配置 CMake 后更新翻译源：

  ```powershell
  cmake --build .\launcher\build --target SwiftDemoUIPro_lupdate --config Release
  ```

- 翻译 `launcher/translations/swift_demoui_pro_zh_CN.ts` 中每个新增的 unfinished 条目。
- 编译启动器以生成 `.qm`；生成的 `.qm` 文件不提交到 Git。
- 可使用 `--ui-language en` 和 `--ui-language zh_CN` 检查不同语言界面。

### 游戏内 Panorama DemoUI

- CS2 与 Dota 2 插件的加载约定不同。本项目使用 CS2 会主动加载的 `addon/resource/platform_english.txt` 和 `platform_schinese.txt`，构建后对应 VPK 内的 `resource/platform_<language>.txt`；缺少对应语言时由本地化系统回退为英文。
- XML 中的静态用户文案使用 `#SwiftDemoVoice_*` Token；JavaScript 动态文案必须通过 `_Localize()`（内部调用 `$.Localize`）处理，不能直接把 Token 赋给 `.text`。
- 动态数字和文本使用 Panorama dialog variable，例如 `{d:count}` 和 `{s:player}`。新增或删除 Token 时必须同时更新两份语言目录，测试会检查引用、缺失项和未使用项。
- 语言文件属于运行时 `game` 资源，不由 ResourceCompiler 编译。构建脚本会以带 BOM 的 UTF-8 原始文本复制到 `game/csgo_addons/swift_demo_menu_override/resource/`，随后打进 VPK。
- DemoUI 不提供独立语言开关，而是自动跟随 CS2 界面语言；测试语言变更时需要完整重启 CS2。

## 版本与本地 Release

仓库根目录有两个相互独立、格式均为 `MAJOR.MINOR.PATCH` 的版本文件：

- [VERSION](VERSION) 是启动器/安装包版本，供 CMake、Windows 元数据和启动器资产名使用。
- [MENU_VERSION](MENU_VERSION) 是 DemoUI VPK 版本，会嵌入启动器并用于独立 VPK 资产名。

启动器还会嵌入当前 Git 短哈希。更新器先读取 GitHub 最新正式 Release，再读取 `update-manifest.json`；即使只更新一个组件，清单也始终描述两个组件。

Release 源码包从 `HEAD` 生成，因此创建候选版本前 Git 工作区必须干净：

```powershell
.\release.ps1 `
  -Cs2Root "<CS2 根目录>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt Desktop kit>"
```

该命令会运行两组测试，重新构建 VPK，编译、测试并打包启动器，创建 Git 源码归档，并输出：

```text
release\v<版本号>\SwiftDemoUIPro-v<版本号>-win64.zip
release\v<版本号>\SwiftDemoUIPro-v<版本号>-source.zip
release\v<版本号>\swift_demo_menu_override-v<DemoUI版本号>.vpk
release\v<版本号>\update-manifest.json
release\v<版本号>\SHA256SUMS.txt
```

测试未提交修改时使用 `build-launcher.ps1`。完成修改并提交后再运行 `release.ps1`；stash 中的改动不会进入源码包。

## GitHub 关联与发布

首次创建远端仓库：

```powershell
gh auth login
gh repo create nicedayzhu/SwiftDemoUIPro --public --source . --remote origin --push
```

发布完整启动器版本（可同时递增 DemoUI 版本）：

```powershell
.\release.ps1 -Version 0.2.0 -MenuVersion 0.1.1 `
  -Cs2Root "<CS2 根目录>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt Desktop kit>" `
  -Publish
```

只发布新的 DemoUI VPK、不重新构建或重复发布启动器：

```powershell
.\release.ps1 -MenuOnly -MenuVersion 0.1.2 `
  -Cs2Root "<CS2 根目录>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -Publish
```

完整发布可更新两个版本文件，并创建 `v<启动器版本>`；`-MenuOnly` 只更新 `MENU_VERSION`、跳过 Qt 构建，并创建 `menu-v<DemoUI版本>`。两种模式都会构建和测试 VPK，生成带版本号的独立 VPK、`update-manifest.json` 与 `SHA256SUMS.txt`，先上传到 GitHub Release 草稿，再在全部成功后发布。仅 DemoUI Release 的清单会继续引用标准 `v<VERSION>` 启动器资产，因此 latest-release API 仍能同时描述两个独立组件。

下载后可这样核对文件：

```powershell
Get-FileHash .\SwiftDemoUIPro-v0.2.0-win64.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

请勿手动修改生成的归档或校验和。

## 仓库结构

| 路径 | 用途 |
| --- | --- |
| `addon/` | 打进 override VPK 的 Panorama 布局、JavaScript、样式和原始本地化文本。 |
| `launcher/` | Qt 启动器源码、测试、翻译、内置 miniz 与打包脚本。 |
| `powershell/` | 共用的 Panorama 编译、打包、安装与卸载实现。 |
| `tests/` | Panorama 逻辑和原生布局集成测试。 |
| `demo-menu.ps1` | 仓库级 Panorama 生命周期入口。 |
| `release.ps1` | 带版本号的本地 Release 与可选 GitHub 发布入口。 |
| `.github/workflows/ci.yml` | 可移植的 Windows CI。 |
| `VERSION` | 启动器/安装包语义化版本。 |
| `MENU_VERSION` | 独立的 DemoUI VPK 语义化版本。 |

`dist/`、`release/`、`launcher/build/`、`launcher/package/`、`launcher/.qt/` 和 `launcher/.tools/` 等生成目录不得提交。

## 贡献与提交规则

- 保持 C++17 和 Qt 6.5 或更高版本兼容。
- 文件系统与进程行为放在 `Cs2Manager`，界面展示放在 `LauncherWindow`。
- 保持 SearchPath 与清理流程精确、幂等且可恢复。
- 面向玩家的文档保持中英文同步；新增启动器文本时同时完成翻译。
- 提交前运行 `git diff --check`、检查 `git status --short`，并执行相关测试。
- 使用 Conventional Commits，例如 `feat(launcher): support demos in ZIP archives` 或 `docs(readme): add launcher preview`。
- 不提交编译、打包、翻译二进制或 Release 生成物。

代码 Agent 还应遵循 [AGENTS.md](AGENTS.md)，其中记录了项目安全不变量和操作规则。

## 许可证与再分发

- 项目原创代码使用 MIT License，请保留 [LICENSE](LICENSE)。
- miniz 位于 `launcher/third_party/miniz/`，使用 MIT License。
- Noto Sans SC 使用 SIL Open Font License 1.1。
- 打包的 Qt 库以 `LGPL-3.0-only` 动态链接；必须保持可替换，并保留 Qt 说明与许可文本。
- VPKEdit 使用 MIT License，保留版权与许可文本即可再分发；但本项目只在开发阶段用它构建/检查静态 VPK，因此 Release 不携带它。运行时 session VPK 由内置 Rust 辅助程序写入。
- Valve 游戏资源、名称和商标不会因本仓库的 MIT License 而重新授权。

依赖或分发方式变化时，请同步更新 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 和 [launcher/THIRD_PARTY_NOTICES.txt](launcher/THIRD_PARTY_NOTICES.txt)。
