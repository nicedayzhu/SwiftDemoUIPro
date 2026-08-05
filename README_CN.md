# Swift DemoUI Pro

[English](README.md) | [简体中文](README_CN.md)

[![许可证：MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![平台：Windows](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](#环境要求)

这是一个用于 Counter-Strike 2 Demo 和 HLTV 回放的纯客户端 Panorama 增强项目，并附带可选的 Qt 启动器。它在保留 Valve 原生 DemoUI 的同时，加入按玩家控制已录制语音、一键切换 POV 和直接跳转回合等功能。

项目不依赖 SwiftlyS2、服务器插件或 Workshop 资源。

> [!IMPORTANT]
> Swift DemoUI Pro 会使用 `-insecure` 启动 CS2，并在 Demo 会话期间临时向 `gameinfo.gi` 添加 override SearchPath。请勿使用该会话进行正常匹配。观看结束后，请先彻底退出 CS2，再回到启动器点击**停止观看 Demo**，以清理 VPK、SearchPath 和临时文件。

## 界面预览

![Swift DemoUI Pro 在 CS2 Demo 回放中的玩家语音、POV 与回合导航面板](docs/images/demo-voice-ui.png)

游戏内面板会保留原生 DemoUI 控件，同时加入按玩家控制已录制语音、一键切换 POV 和直接跳转回合等功能。

## 功能

- Demo 打开后自动启用低位和高位两组语音掩码：

  ```text
  tv_listen_voice_indices -1
  tv_listen_voice_indices_h -1
  ```

- 自动读取当前 Demo 的玩家名称、阵营、account ID 和显示槽位。
- 支持全部 1–64 号显示槽位，分别生成低位和高位两个有符号 32 位掩码。
- 支持全部收听、全部静音、仅 T、仅 CT 和单独玩家语音控制。
- 通过 account ID、显示槽位和玩家名核验目标，再切换到该玩家的第一人称 POV。
- 死亡或断线玩家会禁用容易误导的 POV 操作，但仍可调整其已录制语音。
- 读取原生 `RoundIntervals`，可直接跳转到任意回合的起始 tick。
- 保留原生时间轴、播放控制、设置和默认镜头模式热键。
- 使用克制的石墨灰与金色界面，并清晰区分 CT/T 阵营和语音状态。
- 附带原生 Qt 6 启动器 **Swift DemoUI Pro**，支持 Steam 多库检测、拖放 `.dem`/`.zip`、自动清理、中英文界面和系统语言识别。
- 可直接打开下载的 ZIP 压缩包：包内只有一个 Demo 时自动选择，存在多个 Demo 时显示选择列表，并且只把所选 `.dem` 流式写入启动器的临时目录。

## 快速开始

普通用户可解压已经打包的 `SwiftDemoUIPro-v<版本号>-win64.zip`，然后：

1. 确保 `SwiftDemoUIPro.exe` 与 `swift_demo_menu_override.vpk` 位于同一目录，运行程序。
2. 选择或拖入一个 `.dem` 文件或 `.zip` 压缩包；如果 ZIP 中包含多个 Demo，请从列表中选择一个。
3. 确认自动检测到的 CS2 安装目录。
4. 点击**开始观看 Demo**。启动器只会为本次启动附加 `-insecure`，不会修改 Steam 的永久启动项。
5. 观看结束后彻底退出 CS2，回到启动器并点击**停止观看 Demo**。

如果启动器意外中断，重新打开它即可继续完成待处理的清理。

## 使用 Demo 菜单

菜单会在 Demo/HLTV 回放期间自动展开，并默认启用所有语音槽位。

- 使用 CS2 原生 Demo 鼠标模式热键显示光标。
- 点击存活玩家的名称区域切换到第一人称 POV；当前 POV 会以金色标记。
- 点击玩家行右侧音频按钮，只切换该玩家的已录制语音。
- 展开 **ROUND NAVIGATION**，选择回合即可跳转到其起始 tick。
- 使用顶部快捷按钮收听全部、静音全部或仅收听 T/CT。

菜单中的 `SLOT 1` 对应低位掩码 bit 0，`SLOT 32` 对应低位 bit 31，`SLOT 33` 对应高位 bit 0，`SLOT 64` 对应高位 bit 31。例如显示槽位 4、5、9、11、12 会生成低位掩码 `3352`。

## 环境要求

### 菜单/VPK 构建

- Windows 与 PowerShell。
- 当前版本的 Counter-Strike 2，安装中需包含 Valve 的 `resourcecompiler.exe`。
- [VPKEdit](https://github.com/craftablescience/VPKEdit) 命令行工具。
- 用于 Panorama 掩码测试的 Node.js。

### 启动器构建

- Qt 6.5 或更高版本的 64 位 MSVC Desktop kit。
- 安装了“使用 C++ 的桌面开发”工作负载的 Visual Studio。
- 已加入 `PATH`，或由 Qt/Visual Studio 安装的 CMake。

## 构建 Panorama Override

在仓库根目录运行以下命令，并按本机环境替换示例路径：

```powershell
.\demo-menu.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe"
```

输出：

```text
dist\swift_demo_menu_override.vpk
```

也可以单独执行各步骤：

```powershell
.\demo-menu.ps1 -Action Compile -Cs2Root "<CS2 根目录>"
.\demo-menu.ps1 -Action Pack -Cs2Root "<CS2 根目录>" -VpkEditCli "<vpkeditcli.exe>"
.\demo-menu.ps1 -Action Install -Cs2Root "<CS2 根目录>"
.\demo-menu.ps1 -Action Uninstall -Cs2Root "<CS2 根目录>"
```

`-InstallLocalOverride` 可在构建后直接安装。安装过程会把 VPK 复制到 `game\csgo\overrides`，在 `Game csgo` 之前插入一条精确的 SearchPath，并在首次修改前创建 `gameinfo.gi.swift_demo_menu_override.restore.bak`。

安装、更新或卸载 Panorama override 后都要完整重启 CS2，因为客户端会缓存编译后的 UI 资源。

## 构建启动器

先构建 VPK，再运行：

```powershell
.\launcher\build-launcher.ps1 -QtRoot "C:\Qt\6.8.3\msvc2022_64" -Package
```

脚本会配置 CMake、构建启动器、运行 Qt 测试、通过 `windeployqt` 收集所需的动态 Qt 库，并生成：

```text
launcher\package\SwiftDemoUIPro-v<版本号>-win64.zip
```

启动器的架构、国际化和打包细节见 [launcher/README_CN.md](launcher/README_CN.md)。

## 版本与 Release

仓库根目录的 [VERSION](VERSION) 是唯一版本来源。CMake 会把它自动用于应用版本、关于页面、Windows 文件属性和带版本号的包名；启动器还会嵌入当前 Git commit。

使用一条命令生成完整的本地 Release 候选包：

```powershell
.\release.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe" `
  -QtRoot "C:\Qt\6.8.3\msvc2022_64"
```

脚本会运行全部 JavaScript 与 Qt 测试，重新构建 VPK 和启动器，然后在 `release\v<版本号>` 下生成：

```text
SwiftDemoUIPro-v<版本号>-win64.zip
SwiftDemoUIPro-v<版本号>-source.zip
SHA256SUMS.txt
```

首次关联 GitHub 时，安装 [GitHub CLI](https://cli.github.com/)，然后运行：

```powershell
gh auth login
gh repo create nicedayzhu/SwiftDemoUIPro --public --source . --remote origin --push
```

以后每次发布只需选择一个[语义化版本号](https://semver.org/lang/zh-CN/)，再给同一命令增加 `-Version` 和 `-Publish`：

```powershell
.\release.ps1 -Version 0.2.0 `
  -Cs2Root "<CS2 根目录>" `
  -VpkEditCli "<vpkeditcli.exe>" `
  -QtRoot "<Qt Desktop kit>" `
  -Publish
```

发布脚本会在需要时更新 `VERSION`，创建规范提交 `chore(release): v<版本号>`，重新构建并测试全部内容，创建带注释的 Git tag，推送分支与 tag，把三个文件上传到 GitHub Release 草稿，并且只在全部上传成功后正式发布。失败的草稿可以安全重跑；已经发布的 Release 不会被覆盖。

下载 Release 后，可把文件 SHA-256 与 `SHA256SUMS.txt` 对照：

```powershell
Get-FileHash .\SwiftDemoUIPro-v0.2.0-win64.zip -Algorithm SHA256
Get-Content .\SHA256SUMS.txt
```

GitHub Actions 会在每次 push 和 Pull Request 时运行可移植的 JavaScript 与 Qt 测试。VPK 和最终 Release 包仍由本地发布命令生成，因为它们依赖已安装的 CS2 和 Valve `resourcecompiler.exe`。

## 测试

运行 Panorama 语音掩码和布局集成测试：

```powershell
node .\tests\test_demo_voice_mask.js
```

`build-launcher.ps1` 会自动运行启动器测试，也可以在已有构建目录中单独执行：

```powershell
ctest --test-dir .\launcher\build -C Release --output-on-failure
```

## 仓库结构

| 路径 | 用途 |
| --- | --- |
| `addon/` | 编译进 override VPK 的 Panorama 布局、JavaScript 和样式 |
| `launcher/` | Qt 6 Widgets 启动器、测试、翻译、字体和打包脚本 |
| `powershell/` | 共用的编译、打包、安装和卸载实现 |
| `tests/` | Panorama 语音掩码与原生布局集成测试 |
| `demo-menu.ps1` | 仓库级构建和生命周期入口 |
| `dist/` | 生成的 VPK 输出，不由 Git 跟踪 |

## 兼容性与限制

- 菜单只能控制 Demo 中已经存在的语音包，无法恢复从未录制的语音。
- FACEIT Demo 通常包含可用语音，但旧版本、损坏或来源不同的 Demo 可能没有完整数据。
- Valve 随时可能调整原生 `huddemocontroller` 资源。发生变化后，应先与当前游戏文件重新同步布局，再构建 override。
- `GameStateAPI.GetPlayerDataJSO()` 尚未准备好时，菜单会显示等待状态，并每 0.75 秒重试。
- 本项目是非官方客户端修改，与 Valve 或 FACEIT 没有隶属、授权或背书关系。
- ZIP 播放支持未加密且压缩方式受内置 miniz 库支持的条目。启动器不会展开包内其他文件，并会拒绝解压后超过 8 GB 的 Demo。

原理参考：[FACEITcom 关于 CS2 Demo 语音 indices 的讨论](https://www.reddit.com/r/FACEITcom/comments/16vvidt/no_recorded_voice_chat_in_faceit_cs2_demos/)。

## 参与贡献

欢迎提交 Issue 和 Pull Request。请让修改保持聚焦，运行相关 JavaScript 与 Qt 测试，并在变更面向用户的文档时同步更新两种语言。新增启动器文本时，请在源码中使用英文并通过 Qt Linguist 翻译，同时保留 `%1`、`%2` 等占位符。

## 许可证与第三方组件

项目原创代码使用 [MIT License](LICENSE)。第三方库、字体、游戏资源、名称和商标仍遵循各自条款，不因本项目的 MIT 许可证而重新授权。详见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) 和 [launcher/THIRD_PARTY_NOTICES.txt](launcher/THIRD_PARTY_NOTICES.txt)。

尤其需要注意：兼容原生 DemoUI 的布局源自或需要与 Valve 的 Counter-Strike 2 UI 资源互操作。重新分发游戏派生材料或打包资源前，请自行核对 Valve 的相关条款。本段仅用于说明授权边界，不构成法律意见。

## 赞助

如果 Swift DemoUI Pro 对你有帮助，欢迎通过 Ko-fi 支持后续开发：

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/K6C623WHCQ)
