# Swift DemoUI Pro 启动器

[English](README.md) | [简体中文](README_CN.md) | [项目主页](../README_CN.md)

这是一个轻量级 Windows Qt 6 Widgets 应用，用于安全地安装本项目的 Panorama DemoUI，并直接打开 Counter-Strike 2 `.dem` 文件或下载的 `.zip` 压缩包。内置的只读 Rust 辅助程序会提取已录制语音的玩家/tick 数据；程序不包含 2D 回放或网络服务。

## 界面预览

![Swift DemoUI Pro 启动器的 Demo、ZIP 选择与 TrueView 兼容控制](../docs/images/launcher-playback-ui.png)

启动器把整个回放流程集中在同一页面：选择 Demo，确认兼容选项，启动 CS2，并在观看结束后恢复临时会话。

## 使用流程

1. 选择或拖入一个 `.dem` 文件或 `.zip` 压缩包。ZIP 中只有一个 Demo 时会自动选择；存在多个时，请从列表中选择一个。
2. 确认自动检测到的 CS2 路径，必要时选择其他安装目录。
3. 为获得最佳兼容性，建议保持 **TrueView 预测**关闭。启动器会在临时 CFG 中写入 `cl_demo_predict 0`，避免缺少 TrueView 指令数据的 Demo 出现预测闪烁。只有确认录像支持时再启用；启动器会记住该选项。
4. 点击**开始观看 Demo**。启动器会安装/校验 VPK，把 Demo 复制到专用临时目录；如果选择的是 ZIP，则只流式写入所选条目。随后由内置 Rust 辅助程序解析 `SvcVoiceData`、编译会话专用 Panorama 数据资源并写入 VPK，再创建 `swift_demo_launcher.cfg` 并执行：

   ```text
   steam.exe -applaunch 730 -insecure -novid +exec swift_demo_launcher.cfg
   ```

5. 观看结束后先彻底退出 CS2，再点击**停止观看 Demo**。启动器会移除自己拥有的精确 SearchPath、VPK、解析后的语音资源、临时 CFG、会话标记和 Demo 副本。

如果控制台持续出现 `Not enough TrueView command lookahead`，或者画面不断闪烁，请停止回放、关闭 **TrueView 预测**，再重新启动 Demo。

`-insecure` 只存在于本次启动命令中，不会写入 Steam 的永久启动项。DemoUI VPK 的 SearchPath 会持续存在到清理完成，因此请勿跳过最后一步。如果启动器意外中断，重新打开即可继续清理。

## 更新

启动器打开后会检查仓库最新发布的 GitHub Release。发现更新时只显示一个可关闭的小气泡；没有玩家操作就不会下载或安装任何内容。随后可在**关于**页面分别决定：

- 启动器更新会在浏览器中打开带版本号的 Windows 压缩包，不会尝试替换正在运行的 EXE。
- 只有 DemoUI 更新时，玩家可单独选择下载带版本号的 VPK。启动器会校验 SHA-256，并保存到用户的本地应用数据目录；下次开始 Demo 回放时优先使用该版本。

Release 通过 `update-manifest.json` 分别记录启动器和 DemoUI 版本。对于尚无清单的旧 Release，启动器仍可根据 `SwiftDemoUIPro-v<版本号>-win64.zip` 资产名称检测启动器更新。

更新检查匿名读取公开仓库的 GitHub REST API，不需要也不会保存玩家的 GitHub Token。请求包含 GitHub 要求的 `User-Agent`、推荐的 JSON `Accept` 与 API 版本头，并检查响应状态、内容类型、JSON、超时和匿名访问频率限制；无效 JSON 会自动重试一次。

## 构建

完整的项目构建、测试、版本和发布流程见[开发者指南](../DEVELOPMENT_CN.md)。

请使用 Qt 6.5 或更高版本的 64 位 MSVC Desktop kit、Visual Studio C++ 工具、CMake 以及 Rust stable/Cargo，并从仓库根目录执行以下命令。

### 编译并测试

如需进行不依赖 Panorama VPK 的快速、接近 CI 的构建：

```powershell
.\launcher\build-launcher.ps1 `
  -QtRoot "<Qt Desktop kit>" `
  -Configuration Release `
  -SkipVpkCheck
```

脚本会测试/编译 `swift-demo-voice-indexer.exe`、配置 CMake、编译启动器和翻译，并运行 CTest。`-SkipVpkCheck` 只适用于非打包构建，它不会执行 `windeployqt`；因此 `launcher\build\Release\SwiftDemoUIPro.exe` 依赖 Qt 开发环境，不应直接双击运行。出现缺少 `Qt6Gui.dll` 的提示，通常就是误用了裸编译目录。

### 生成可运行包

先构建 VPK，再打包启动器：

```powershell
.\demo-menu.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe"

.\launcher\build-launcher.ps1 -QtRoot "C:\Qt\6.8.3\msvc2022_64" -Package
```

打包产物：

```text
launcher\package\SwiftDemoUIPro-v<版本号>\SwiftDemoUIPro.exe
launcher\package\SwiftDemoUIPro-v<版本号>-win64.zip
```

本地端到端测试请运行展开版本目录中的 EXE。打包步骤会通过 `windeployqt` 收集 `Qt6Core.dll`、`Qt6Gui.dll`、`Qt6Network.dll`、`Qt6Widgets.dll`、Windows TLS 后端和 `platforms\qwindows.dll`。`SwiftDemoUIPro.exe`、`swift-demo-voice-indexer.exe`、DLL/插件、翻译和 `swift_demo_menu_override.vpk` 必须保持在同一完整目录结构中。玩家电脑无需安装 Workshop Tools DLC、`resourcecompiler.exe` 或 VPKEdit。

仓库根目录的 `release.ps1` 用于已经提交的 Release 候选版本，而不是日常开发测试。由于源码包从 `HEAD` 生成，只要 Git 工作区不干净，它就会拒绝继续。

ZIP 支持由项目内置的 miniz 3.1.2 源码直接编译进 `SwiftDemoUIPro.exe`，用户不需要安装 7-Zip、调用 PowerShell 解压或携带其他可执行程序。发布包会包含 `licenses/miniz-MIT.txt`。

## 国际化与字体

- 程序源码和后备文本使用英文。选择**跟随系统**时，中文系统会自动使用简体中文；没有对应翻译时回退到英文。
- 用户可在侧栏选择**跟随系统 / 简体中文 / English**，选择结果会保存到本机。
- 翻译源文件使用 `translations/swift_demoui_pro_<locale>.ts` 命名。CMake 会自动发现并编译匹配的外置 `.qm` 文件，程序启动时也会扫描这些翻译。
- 修改源码文本后，可构建 CMake 的 `update_translations` 目标，再用 Qt Linguist 编辑 `.ts`。请保留 `%1`、`%2` 等占位符。

界面嵌入 Noto Sans SC 可变字体，以保持中文与拉丁字符的字重和行高一致。该字体使用 SIL Open Font License 1.1。

## 安全边界

- 启动器不会编辑 Steam 的永久启动选项。
- CS2 运行时拒绝安装、替换或删除 VPK。
- 首次修改 `gameinfo.gi` 前会创建 `gameinfo.gi.swift_demo_launcher.restore.bak`。
- 清理时只移除本项目精确的 SearchPath 和自有临时文件，不覆盖玩家或其他工具的修改。
- 解析后的语音数据由内置 Rust 辅助程序直接编译并打包为小型 session VPK，通过独立 VPK SearchPath 挂载，并在清理时删除；源 Demo 不会被重写。
- 持久化会话标记让程序能够提示并恢复被中断的清理。
- ZIP 处理只枚举压缩包内容，并把选中的 `.dem` 流式写入自有的 `current.dem`；同时校验解压大小与 CRC、检查磁盘余量，并限制解压后最大为 8 GB。
- 加密条目或不支持的压缩方式会给出错误，压缩包内其他文件不会落盘。
- Demo 回放使用 `-insecure`，不能用于正常匹配。

## 第三方组件与许可证

- 项目启动器源码使用仓库中的 [MIT License](../LICENSE)。
- Qt 6 以 `LGPL-3.0-only` 动态链接；发布包中的 DLL 保持可替换。
- Noto Sans SC 使用 `OFL-1.1`。
- miniz 3.1.2 以 MIT License 静态编译进启动器。
- 启动流程参考 MIT 许可的 [drjackild/cs2-demo-opener](https://github.com/drjackild/cs2-demo-opener)，未包含其回放、解析器、Web 或源代码文件。

完整说明与许可文本见 [THIRD_PARTY_NOTICES.txt](THIRD_PARTY_NOTICES.txt) 和发布包内的 `licenses` 目录。

## 赞助

[![Support me on Ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/K6C623WHCQ)
