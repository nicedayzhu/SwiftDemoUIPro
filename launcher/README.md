# Swift DemoUI Pro

轻量级 Windows Qt 6 Widgets 应用，用于安全地安装本项目的 Panorama DemoUI 并观看 CS2
`.dem` 文件。它不包含 2D 回放、Demo 解析或网络服务。

## 使用流程

1. 选择或拖入 `.dem` 文件。
2. 确认自动检测到的 CS2 路径；必要时点击“更改”。
3. 点击“开始观看 Demo”。启动器会安装/校验 VPK，把 Demo 复制到专用临时目录，生成
   `swift_demo_launcher.cfg`，然后执行：

   ```text
   steam.exe -applaunch 730 -insecure -novid +exec swift_demo_launcher.cfg
   ```

4. 观看结束后先完全退出 CS2，再回到启动器点击“停止观看 Demo”。启动器会精确移除自己
   添加的 SearchPath、VPK、临时 CFG、会话标记和复制的 Demo。

`-insecure` 只存在于这一次启动命令中，不会写入 Steam 的永久启动项。但是 DemoUI VPK 的
SearchPath 会持续存在，直到执行“停止观看 Demo”；因此不要跳过最后一步。

## 构建

建议使用 Qt 6.5 或更高版本的 64 位 MSVC Desktop kit，以及带 C++ 工具的 Visual Studio。
先在项目根目录构建 VPK，然后执行：

```powershell
.\demo-menu.ps1
.\launcher\build-launcher.ps1 -QtRoot C:\Qt\<version>\msvc2022_64 -Package
```

打包产物：

```text
launcher\package\SwiftDemoUIPro-win64.zip
```

构建脚本会运行 Qt 单元测试，并通过 `windeployqt` 收集运行所需的 Qt DLL。发布目录中的
`SwiftDemoUIPro.exe` 与 `swift_demo_menu_override.vpk` 必须放在一起。

## 国际化与字体

- 软件代码和源文本使用英文；默认语言为“跟随系统”。中文系统自动加载简体中文，其他
  系统在没有对应翻译时使用内置英文。
- 用户可在侧栏手动选择“跟随系统 / 简体中文 / English”，选择结果会保存到本机设置。
- 翻译源文件位于 `translations/swift_demoui_pro_<locale>.ts`。新增一个符合该命名规则的
  `.ts` 文件后，CMake 会自动发现并编译为外置 `.qm` 文件；程序启动时也会自动扫描这些
  文件并加入语言列表。
- 更新源文本后，可构建 CMake 的 `update_translations` 目标，再使用 Qt Linguist 编辑
  `.ts` 文件。请保留 `%1`、`%2` 等占位符。

界面嵌入 Noto Sans SC 可变字体，同一字体同时覆盖简体中文与拉丁字符，避免不同系统的
字体回退造成字重、行高不一致。字体按 SIL Open Font License 1.1 使用，发布包会附带许可。

## 安全边界

- 启动器不会编辑 Steam 永久启动选项。
- CS2 运行时禁止安装、替换或删除 VPK。
- `gameinfo.gi` 首次修改前会保留
  `gameinfo.gi.swift_demo_launcher.restore.bak`，停止时只删除本项目拥有的精确 SearchPath，
  不覆盖玩家或其他工具的修改。
- 如果启动器异常退出，会话标记仍会保留；重新打开后会继续显示清理警告。

## 第三方说明

- 启动流程参考 MIT 许可的
  [drjackild/cs2-demo-opener](https://github.com/drjackild/cs2-demo-opener)，未包含其 2D 回放、
  Demo 解析或 Web 前端代码。
- 启动器动态链接 Qt 6（LGPL v3）。打包脚本会保留可替换的 Qt DLL，并在发布目录附带 Qt
  许可文本。
- 界面字体使用 SIL Open Font License 1.1 许可的 Noto Sans SC，字体许可同样随发布包提供。
