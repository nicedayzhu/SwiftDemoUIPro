---
title: Swift DemoUI Pro
type: landing

sections:
  - block: hero
    id: top
    content:
      eyebrow: SWIFT DEMOUI PRO / WINDOWS X64
      title: 让 CS2 Demo 回放重新有声音。
      text: >-
        保留原生 DemoUI，只在旁边增加语音与导航控制：选择收听对象、辨认当前说话者、
        切换 POV，并跳到指定回合。
      primary_action:
        text: 下载 v0.1.2
        url: https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest
        icon: arrow-down-tray
        style: solid
      secondary_action:
        text: 查看 GitHub
        url: https://github.com/nicedayzhu/SwiftDemoUIPro
        icon: brands/github
        style: text
      media:
        type: image
        src: demo-voice-hero.webp
        alt: Swift DemoUI Pro 在 CS2 Demo 回放中显示语音面板
    design:
      layout: split-left
      no_padding: true
      css_class: "dark swift-hero"
      background:
        color: "#101210"
      spacing:
        padding: ["5rem", 0, "5rem", 0]

  - block: cta-image-paragraph
    id: features
    content:
      items:
        - title: 按玩家控制 Demo 语音
          text: >-
            面板读取 Demo 中已经保存的 VoiceData。可以收听全部、全部静音、只听 T 或 CT，
            也可以单独开关任意玩家。
          feature_icon: check
          features:
            - 语音位置由 Demo Tick 驱动，暂停、跳转和倍速播放仍保持同步
            - 当前说话者直接显示头像与名称
            - 过滤规则同时作用于声音与说话者提示
          image: voice-panel-zh.webp
          button:
            text: 面板使用说明
            url: https://github.com/nicedayzhu/SwiftDemoUIPro#using-the-in-game-panel
        - title: 选择文件，启动回放
          text: >-
            这里展示的是实际 Windows 启动器。选择 `.dem`、`.zip` 或 `.dem.zst` 后，
            它会检查文件、准备语音索引，再以可还原的方式启动 CS2。
          feature_icon: check
          features:
            - 在同一页确认所选文件、压缩格式与当前运行状态
            - 原生处理 FACEIT 下载的 `.dem.zst`，大型文件在后台准备
            - 完全退出 CS2 后，点击“停止观看并恢复”清理本次会话
          image: launcher-zh.png
          button:
            text: 查看完整使用流程
            url: https://github.com/nicedayzhu/SwiftDemoUIPro#quick-start
    design:
      css_class: "swift-showcase"

  - block: features
    content:
      title: 功能范围
      text: 这些功能围绕回放本身展开；CS2 的原生时间轴和播放控件保持不变。
      items:
        - name: 录制语音
          icon: speaker-wave
          description: 读取 Demo 内已有的 VoiceData，支持全员、阵营和单人过滤。
        - name: 说话者识别
          icon: user-circle
          description: 根据当前 Demo Tick 显示正在发声的玩家头像与名称。
        - name: POV 与回合
          icon: cursor-arrow-rays
          description: 切换到存活玩家的第一视角，或跳到任意已记录回合的起点。
        - name: 下载文件
          icon: document-arrow-down
          description: 直接打开 `.dem`、`.zip` 与 `.dem.zst`，不要求手动解压。
        - name: 后台准备
          icon: arrow-path
          description: 大型文件的暂存和语音索引不会阻塞启动器界面。
        - name: 中英文界面
          icon: language
          description: 启动器和游戏内面板跟随 CS2 的简体中文或英文设置。
    design:
      layout: grid
      css_class: "swift-capabilities"

  - block: steps
    id: how-it-works
    content:
      title: 使用流程
      text: 发布包已经包含运行所需的语音索引、Zstandard 解压与会话 VPK 组件。
      items:
        - title: 解压发布包
          text: 从 GitHub Releases 下载 Windows x64 完整包，并保持目录结构不变。
        - title: 拖入 Demo
          text: 选择 Demo 文件；启动器会识别 CS2 路径并完成回放前准备。
        - title: 开始与还原
          text: 观看结束后完全退出 CS2，再点击“停止并还原”清理本次会话。
    design:
      layout: horizontal
      marker_style: dot
      connector: line
      css_class: "swift-steps"

  - block: features
    id: safety
    content:
      title: 运行边界
      text: 它是回放辅助工具，不是游戏 DLL 补丁。
      items:
        - name: 不修改 client.dll
          icon: shield-check
          description: 不注入、不修补游戏 DLL，也不改写原始 Demo 文件。
        - name: 普通发布包即可运行
          icon: wrench-screwdriver
          description: 使用者不需要 Workshop Tools、ResourceCompiler 或 VPKEdit。
        - name: 会话可明确结束
          icon: arrow-uturn-left
          description: 临时资源位于隔离目录，并由“停止并还原”完成清理。
    design:
      layout: grid
      css_class: "swift-safety"

  - block: faq
    id: faq
    content:
      title: 常见问题
      items:
        - question: 为什么有些 Demo 听不到语音？
          answer: >-
            工具只能播放 Demo 中实际存在的 VoiceData。录制时没有写入的语音，之后无法恢复。
        - question: 支持 FACEIT 的 `.dem.zst` 吗？
          answer: >-
            支持。启动器会流式解压并检查 Demo，不需要另外安装 `zstd.exe`。
        - question: 需要安装 CS2 Workshop Tools 吗？
          answer: >-
            不需要。Workshop Tools 只用于开发者编译资源，普通使用者下载完整 Release 即可。
        - question: 会修改 `client.dll` 或 Demo 文件吗？
          answer: >-
            不会。项目不注入或修补游戏 DLL，也不改写 Demo；运行时资源与原始文件分开存放。
        - question: 如何完整退出？
          answer: >-
            先完全退出 CS2，再回到启动器点击“停止并还原”，让它清理临时资源并恢复本次会话设置。
    design:
      css_class: "swift-faq"
---
