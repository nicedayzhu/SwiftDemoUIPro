---
title: Swift DemoUI Pro
type: landing

sections:
  - block: hero
    id: top
    content:
      eyebrow: SWIFT DEMOUI PRO / WINDOWS X64
      title: Voice controls for CS2 demo playback.
      text: >-
        Keep the native DemoUI and add the controls it is missing: choose who you hear, identify the
        current speaker, switch POV, and move to a recorded round start.
      primary_action:
        text: Download v0.1.2
        url: https://github.com/nicedayzhu/SwiftDemoUIPro/releases/latest
        icon: arrow-down-tray
        style: solid
      secondary_action:
        text: View on GitHub
        url: https://github.com/nicedayzhu/SwiftDemoUIPro
        icon: brands/github
        style: text
      media:
        type: image
        src: demo-voice-hero.webp
        alt: Swift DemoUI Pro voice panel alongside CS2 demo playback
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
        - title: Control recorded voice by player
          text: >-
            The panel reads VoiceData already stored in the demo. Listen to everyone, mute everyone,
            isolate T or CT, or toggle individual players.
          feature_icon: check
          features:
            - Demo-tick timing stays aligned through pause, seek, and playback-speed changes
            - The active speaker is identified by avatar and name
            - Audio and speaker indicators follow the same filter
          image: voice-panel-en.webp
          button:
            text: In-game panel guide
            url: https://github.com/nicedayzhu/SwiftDemoUIPro#using-the-in-game-panel
        - title: Open downloaded demos directly
          text: >-
            Drop a `.dem`, `.zip`, or `.dem.zst` into the launcher. Copying, decompression, validation,
            and voice indexing run in the background.
          feature_icon: check
          features:
            - Handles FACEIT `.dem.zst` files without a separate zstd installation
            - Switch POV by selecting a living player
            - Seek to parsed round-start ticks
          image: voice-panel-zh.webp
          button:
            text: Full usage guide
            url: https://github.com/nicedayzhu/SwiftDemoUIPro#quick-start
    design:
      css_class: "swift-showcase"

  - block: features
    content:
      title: What it adds
      text: Everything is scoped to demo review. CS2's native timeline and playback controls remain in place.
      items:
        - name: Recorded voice
          icon: speaker-wave
          description: Read VoiceData stored in the demo and filter it by all players, team, or individual.
        - name: Speaker identity
          icon: user-circle
          description: Show the avatar and name of the player speaking at the current demo tick.
        - name: POV and rounds
          icon: cursor-arrow-rays
          description: Switch to a living player's POV or seek to any parsed round start.
        - name: Downloaded files
          icon: document-arrow-down
          description: Open `.dem`, `.zip`, and `.dem.zst` files without manual decompression.
        - name: Background preparation
          icon: arrow-path
          description: Large-file staging and voice indexing do not block the launcher window.
        - name: English and Chinese UI
          icon: language
          description: The launcher and in-game panel follow CS2's English or Simplified Chinese setting.
    design:
      layout: grid
      css_class: "swift-capabilities"

  - block: steps
    id: how-it-works
    content:
      title: From file to playback
      text: The release package includes the voice indexer, Zstandard support, and session VPK components.
      items:
        - title: Extract the release
          text: Download the complete Windows x64 package from GitHub Releases and keep its directory structure intact.
        - title: Drop in a demo
          text: Choose the demo file; the launcher locates CS2 and prepares the playback session.
        - title: Play, then restore
          text: After reviewing, fully exit CS2 and choose “Stop and Restore” to clean up the session.
    design:
      layout: horizontal
      marker_style: dot
      connector: line
      css_class: "swift-steps"

  - block: features
    id: safety
    content:
      title: How it stays contained
      text: This is a playback companion, not a game DLL patch.
      items:
        - name: No `client.dll` changes
          icon: shield-check
          description: It does not inject into or patch game DLLs, and it does not rewrite the source demo.
        - name: No Workshop Tools required
          icon: wrench-screwdriver
          description: End users do not need Workshop Tools, ResourceCompiler, or VPKEdit.
        - name: Explicit session cleanup
          icon: arrow-uturn-left
          description: Temporary resources stay isolated and are removed by “Stop and Restore.”
    design:
      layout: grid
      css_class: "swift-safety"

  - block: faq
    id: faq
    content:
      title: Frequently asked questions
      items:
        - question: Why is voice missing from some demos?
          answer: >-
            The tool can only play VoiceData that is present in the demo. Voice that was not recorded cannot be reconstructed later.
        - question: Does it support FACEIT `.dem.zst` files?
          answer: >-
            Yes. The launcher streams, decompresses, and validates the demo without requiring a separate `zstd.exe` installation.
        - question: Do I need CS2 Workshop Tools?
          answer: >-
            No. Workshop Tools are only used by developers when compiling resources. End users only need the complete release package.
        - question: Does it modify `client.dll` or the demo file?
          answer: >-
            No. It does not inject into or patch game DLLs, and runtime resources are kept separate from the original demo.
        - question: How do I end a playback session cleanly?
          answer: >-
            Fully exit CS2, return to the launcher, and select “Stop and Restore” so it can remove temporary resources and restore session settings.
    design:
      css_class: "swift-faq"
---
