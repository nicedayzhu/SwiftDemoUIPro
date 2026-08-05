# Swift DemoUI Pro 启动器

[English](README.md) | [简体中文](README_CN.md) | [项目主页](../README_CN.md)

这是一个轻量级 Windows Qt 6 Widgets 应用，用于安全地安装本项目的 Panorama DemoUI，并直接打开 Counter-Strike 2 `.dem` 文件或下载的 `.zip` 压缩包。它不包含 2D 回放、Demo 解析器或网络服务。

## 使用流程

1. 选择或拖入一个 `.dem` 文件或 `.zip` 压缩包。ZIP 中只有一个 Demo 时会自动选择；存在多个时，请从列表中选择一个。
2. 确认自动检测到的 CS2 路径，必要时选择其他安装目录。
3. 点击**开始观看 Demo**。启动器会安装/校验 VPK，把 Demo 复制到专用临时目录；如果选择的是 ZIP，则只流式写入所选条目。随后创建 `swift_demo_launcher.cfg` 并执行：

   ```text
   steam.exe -applaunch 730 -insecure -novid +exec swift_demo_launcher.cfg
   ```

4. 观看结束后先彻底退出 CS2，再点击**停止观看 Demo**。启动器会移除自己拥有的精确 SearchPath、VPK、临时 CFG、会话标记和 Demo 副本。

`-insecure` 只存在于本次启动命令中，不会写入 Steam 的永久启动项。DemoUI VPK 的 SearchPath 会持续存在到清理完成，因此请勿跳过最后一步。如果启动器意外中断，重新打开即可继续清理。

## 构建

请使用 Qt 6.5 或更高版本的 64 位 MSVC Desktop kit、Visual Studio C++ 工具和 CMake。先在项目根目录构建 VPK，再运行：

```powershell
.\demo-menu.ps1 `
  -Cs2Root "C:\Program Files (x86)\Steam\steamapps\common\Counter-Strike Global Offensive" `
  -VpkEditCli "C:\Tools\VPKEdit\vpkeditcli.exe"

.\launcher\build-launcher.ps1 -QtRoot "C:\Qt\6.8.3\msvc2022_64" -Package
```

打包产物：

```text
launcher\package\SwiftDemoUIPro-v<版本号>-win64.zip
```

构建脚本会运行 Qt 单元测试，并通过 `windeployqt` 收集所需的运行库。发布目录中的 `SwiftDemoUIPro.exe` 与 `swift_demo_menu_override.vpk` 必须放在一起。

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
