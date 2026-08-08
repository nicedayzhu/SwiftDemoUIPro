---
title: Swift DemoUI Pro
type: landing

sections:
  - block: hero
    id: top
    content:
      eyebrow: 为 Counter-Strike 2 Demo 回放而生
      title: 听见战术。看见谁在说话。
      text: >-
        Swift DemoUI Pro 保留原生 DemoUI，同时补上录制语音、说话者提示、POV 与回合控制。
        免费开源，面向 Windows x64。
      primary_action:
        text: 下载 Windows 版
        url: https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest
        icon: arrow-down-tray
        style: primary
      secondary_action:
        text: 查看源代码
        url: https://github.com/nicedayzhu/SwiftDemoUIPro
        icon: brands/github
        style: ghost
      announcement:
        badge:
          text: v0.1.2
          color: primary
        text: 免费开源 · MIT · 无需 Workshop Tools
        link:
          text: 查看发布说明
          url: https://github.com/nicedayzhu/SwiftDemoUIPro/releases/tag/v0.1.2
      media:
        type: image
        src: demo-voice-hero.webp
        alt: CS2 Demo 回放中，Swift DemoUI Pro 显示语音控制面板和正在说话的玩家头像
    design:
      layout: split-left
      size: none
      css_class: "dark swift-hero"
      background:
        color: "#0b0f14"
      spacing:
        padding: ["5rem", 0, "5rem", 0]

  - block: stats
    content:
      items:
        - statistic: "3"
          description: |
            直接支持 `.dem`、`.zip`
            和 `.dem.zst`
        - statistic: "64"
          description: |
            玩家显示槽位
            支持独立语音控制
        - statistic: "0"
          description: |
            DLL 注入或修补
            不改写 Demo 文件
    design:
      layout: minimal
      numbers_gradient: false
      css_class: "swift-stats"
      spacing:
        padding: ["2.75rem", 0, "2.75rem", 0]

  - block: cta-image-paragraph
    id: features
    content:
      items:
        - title: 想听谁，由你决定。
          text: >-
            收听全部、全部静音、仅 T、仅 CT，或精确控制单个玩家。
            语音与说话者头像始终遵循同一套过滤规则。
          feature_icon: check
          features:
            - Demo Tick 驱动，暂停、跳转与倍速播放仍保持同步
            - 显示正在说话的玩家头像与名称
            - 自动跟随 CS2 的简体中文或英文界面
          image: voice-panel-zh.webp
          button:
            text: 查看面板使用说明
            url: https://github.com/nicedayzhu/SwiftDemoUIPro#using-the-in-game-panel
        - title: 复盘不再被准备工作打断。
          text: >-
            拖入 Demo 后，启动器会在后台完成复制、解压、检查和语音索引。
            你可以直接切换第一视角，并跳到任意已记录回合的起点。
          feature_icon: bolt
          features:
            - 原生打开 `.dem`、下载的 ZIP 与 FACEIT `.dem.zst`
            - 点击存活玩家切换第一视角
            - 一键跳转到指定回合开始位置
          image: voice-panel-en.webp
          button:
            text: 阅读完整使用指南
            url: https://github.com/nicedayzhu/SwiftDemoUIPro#quick-start
    design:
      css_class: "swift-showcase"

  - block: features
    content:
      subtitle: 设计边界
      title: 只补上原生回放真正缺少的能力。
      text: 保留 Valve 的原生时间轴和播放控件，不把 DemoUI 变成一套陌生界面。
      items:
        - name: 录制语音控制
          icon: speaker-wave
          description: 按全队、阵营或单个玩家筛选 Demo 中真实保存的语音。
        - name: 说话者提示
          icon: user-circle
          description: 用头像和名称显示当前 Demo Tick 正在发声的玩家。
        - name: POV 与回合导航
          icon: cursor-arrow-rays
          description: 快速切换第一视角，直接抵达关键回合，不再反复拖时间轴。
        - name: 直接打开下载文件
          icon: document-arrow-down
          description: 无需手动解压，也无需另外安装 zstd 工具。
        - name: 后台准备
          icon: arrow-path
          description: 大型 Demo 的暂存与语音索引不会冻结启动器窗口。
        - name: 中英文界面
          icon: language
          description: 启动器与游戏内面板均支持英文和简体中文。
    design:
      layout: bento
      css_class: "swift-capabilities"

  - block: steps
    id: how-it-works
    content:
      subtitle: 三步开始
      title: 把时间留给复盘。
      text: 完整运行包已包含语音索引、Zstandard 解压与会话 VPK 所需组件。
      items:
        - title: 下载并解压
          text: 从 GitHub Releases 获取 Windows x64 完整包，并保持目录内文件完整。
          icon: arrow-down-tray
        - title: 拖入 Demo
          text: 选择 `.dem`、`.zip` 或 `.dem.zst`；启动器会自动识别 CS2 安装位置。
          icon: document-plus
        - title: 开始回放
          text: 点击开始观看；结束后完全退出 CS2，再点击“停止并还原”完成清理。
          icon: play
    design:
      layout: horizontal
      marker_style: icon
      connector: line
      css_class: "swift-steps"

  - block: features
    id: safety
    content:
      subtitle: 安全设计
      title: 不 Hack DLL。每一步都可恢复。
      text: Swift DemoUI Pro 是客户端回放辅助工具，不是游戏模块补丁。
      items:
        - name: 不修改 client.dll
          icon: shield-check
          description: 不注入、不修补游戏 DLL，也不改写你的 Demo 文件。
        - name: 无需开发工具
          icon: wrench-screwdriver
          description: 普通用户无需 Workshop Tools、ResourceCompiler 或 VPKEdit。
        - name: 明确停止并还原
          icon: arrow-uturn-left
          description: 完全退出 CS2 后，由启动器清理一次性资源并恢复临时设置。
    design:
      layout: bento
      css_class: "swift-safety"

  - block: faq
    id: faq
    content:
      title: 常见问题
      subtitle: 下载前最需要知道的几件事。
      items:
        - question: 需要安装 CS2 Workshop Tools 吗？
          answer: >-
            不需要。普通用户只需下载完整 Windows Release；Workshop Tools 仅用于开发者编译静态资源。
        - question: 它会修改 client.dll 或 Demo 文件吗？
          answer: >-
            不会。项目不注入或修补游戏 DLL，也不改写 Demo。运行时资源位于隔离的临时目录，并可明确还原。
        - question: 为什么有些 Demo 没有语音？
          answer: >-
            工具只能播放 Demo 中真实保存的 VoiceData；录制时没有写入的语音无法事后恢复。
        - question: 支持 FACEIT 下载的 `.dem.zst` 吗？
          answer: >-
            支持。启动器会流式解压并验证 Demo，不需要额外安装 `zstd.exe`。
        - question: 这个项目收费吗？
          answer: >-
            不收费。Swift DemoUI Pro 免费开源，原创项目代码采用 MIT License。

  - block: cta-card
    content:
      title: 下一场复盘，别再错过一句话。
      text: Windows x64 · 免费开源 · 当前版本 v0.1.2
      button:
        text: 下载 Swift DemoUI Pro
        url: https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest
    design:
      css_class: "swift-final-cta"
      card:
        css_class: "bg-zinc-900 text-white border border-zinc-800 shadow-xl"
---
