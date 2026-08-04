# CS2 Demo Menu Override

这是一个可扩展的纯客户端 Panorama demo 菜单项目，不依赖 SwiftlyS2、服务器插件或
Workshop 资源。项目覆盖当前 CS2 的 `huddemocontroller.vxml_c`，在 demo/HLTV 回放
状态自动加载。当前首个模块是按玩家动态控制已录制语音；后续可继续加入书签、镜头、
时间轴和分析工具，而不需要更换项目或 VPK 名称。

## 功能

- 进入 demo 后自动执行：

  ```text
  tv_listen_voice_indices -1
  tv_listen_voice_indices_h -1
  ```

- 自动读取当前 demo 的玩家名称、阵营和 player slot。
- 可立即切换：全部收听、全部静音、仅 T、仅 CT、单独玩家。
- 支持 1–64 号显示槽位；内部按 0–63 位分别生成低/高两个有符号 32 位掩码。
- 保留 Valve 原生 DemoUI 的时间轴、播放控制和设置；右侧自定义菜单只负责玩家语音与
  一键 POV，并增加独立的回合导航组件，避免重复原生控件和布局拥挤。
- 回合导航直接读取原生 `RoundIntervals`，显示当前/总回合数和每回合起止时间；展开列表后
  可直接跳转到任意回合的起始 tick，完成后自动收起。
- 点击玩家名称区域会通过 account ID、显示槽位和玩家名定位目标，核验成功后使用
  `spec_mode 2` 切换到第一人称 POV；点击右侧音频按钮才会切换该玩家的语音收听状态。
- 使用与原生记分板一致的玩家状态字段；死亡或断线玩家会降灰并禁用 POV，避免产生误导性的
  `POV switch failed`，但仍允许单独调整其已录制语音槽位。
- 不接管原生 Space 按键，仍可使用游戏默认操作在第一人称、第三人称和自由镜头间切换。
- 视觉样式参考 `swift_menu_poc`，改用无纹理的中性石墨灰底板、金色主强调和原生字体层级；
  蓝色只用于克制的 CT 阵营识别和已开启的喇叭图标，语音开启不会把整行染蓝。语音开关使用
  标准未静音/静音图标并同时显示 `ON`/`OFF`。

## 构建

在本目录运行：

```powershell
.\demo-menu.ps1
```

输出：

```text
dist\swift_demo_menu_override.vpk
```

只执行某一步：

```powershell
.\demo-menu.ps1 -Action Compile
.\demo-menu.ps1 -Action Pack
```

## 安装与卸载

构建并安装：

```powershell
.\demo-menu.ps1 -InstallLocalOverride
```

或者安装已经打好的 VPK：

```powershell
.\demo-menu.ps1 -Action Install
```

安装脚本会：

1. 复制 VPK 到 `game\csgo\overrides\swift_demo_menu_override.vpk`；
2. 在 `gameinfo.gi` 的 `Game csgo` 之前加入精确的 override SearchPath；
3. 首次安装时保留一份 `gameinfo.gi.swift_demo_menu_override.restore.bak`。

卸载：

```powershell
.\demo-menu.ps1 -Action Uninstall
```

安装、更新或卸载后都要完整重启 CS2，因为 Panorama 编译资源会被客户端缓存。

## 使用

1. 启动 CS2 并播放一个 demo。
2. 菜单会自动展开，并默认启用所有低/高 voice slot。
3. 使用原生 demo 的鼠标模式热键显示光标。
4. 点击存活玩家的名称区域直接切换到第一人称视角；当前 POV 会以金色观察标记高亮，死亡
   或断线玩家会显示 `DEAD`/`N/A` 并禁止点击。
5. 点击 `ROUND NAVIGATION` 展开回合列表，选择任意回合即可直接跳到该回合开始位置。
6. 点击玩家行右侧音频按钮或顶部快捷按钮；新的 bitmask 会立即写入两个
   `tv_listen_voice_indices` 命令。

菜单显示的 `SLOT 1` 对应低位掩码 bit 0，`SLOT 32` 对应低位 bit 31，
`SLOT 33` 对应高位 bit 0，`SLOT 64` 对应高位 bit 31。例如参考帖中的
显示槽位 4、5、9、11、12，对应低位掩码 `3352`。

## 限制

- VPK 只能控制 demo 中已经录制的语音包；如果 demo 本身没有语音数据，菜单无法恢复它。
- FACEIT demo 通常可用，但不同来源、旧版本或损坏的 demo 可能没有完整语音。
- Valve 更新原生 `huddemocontroller.xml` 后，应重新从当前 `pak01_dir.vpk` 同步布局再构建，
  否则旧 override 可能漏掉新增的原生控件。
- `GameStateAPI.GetPlayerDataJSO()` 尚未准备好时，菜单会显示等待状态，并每 0.75 秒自动刷新。

原理参考：
[FACEITcom 讨论：CS2 demo 语音与 voice indices](https://www.reddit.com/r/FACEITcom/comments/16vvidt/no_recorded_voice_chat_in_faceit_cs2_demos/)

## Qt 启动器

`launcher` 目录包含一个原生 Qt 6 Widgets Windows 启动器。它可以选择/拖入 `.dem`、自动
检测 Steam 多库中的 CS2、安装或修复菜单 VPK，并只为本次启动附加 `-insecure` 参数。

观看结束后，玩家必须先退出 CS2，再点击“停止观看 Demo”。启动器会移除自己添加的 VPK
SearchPath、VPK 和临时文件，使环境恢复为可正常游戏的状态。构建与发布方法见
[launcher/README.md](launcher/README.md)。
