# Third-Party Notices

Unless a file or directory says otherwise, original Swift DemoUI Pro source code is licensed under the repository's [MIT License](LICENSE). That license does not replace or supersede the terms of third-party components or materials.

## Valve and Counter-Strike 2

This project interoperates with Counter-Strike 2 and overrides the native `huddemocontroller` Panorama layout. The compatible layout structure and any referenced Counter-Strike 2 resources, names, icons, trademarks, and other game material remain the property of Valve Corporation and their respective owners. They are not licensed under this project's MIT License.

Counter-Strike, Counter-Strike 2, CS2, Steam, and the associated logos and trademarks are property of Valve Corporation. Swift DemoUI Pro is an independent, unofficial project and is not affiliated with or endorsed by Valve.

Users and distributors are responsible for ensuring that their use and redistribution of game-derived material complies with Valve's applicable terms.

## Qt 6

The optional launcher dynamically links to Qt 6 Core, Gui, and Widgets; its test executable additionally links to Qt Test. Open-source Qt binaries used by release packages are distributed under the GNU Lesser General Public License version 3 (`LGPL-3.0-only`) and remain replaceable dynamic libraries.

The packaged launcher includes the applicable Qt license text. Source and licensing information is available from:

- <https://www.qt.io/licensing/open-source-lgpl-obligations>
- <https://code.qt.io/cgit/qt/qtbase.git/>

Qt is copyright The Qt Company Ltd. and other contributors.

## miniz 3.1.2

The launcher statically compiles the vendored miniz 3.1.2 source to enumerate ZIP archives and stream the selected Demo entry. miniz is distributed under the MIT License. The unmodified upstream source and license are stored in `launcher/third_party/miniz`, and packaged releases include the license as `licenses/miniz-MIT.txt`.

- <https://github.com/richgel999/miniz/tree/3.1.2>

## Noto Sans SC

The launcher embeds Noto Sans SC from the Noto CJK project. The font is licensed under the SIL Open Font License 1.1 (`OFL-1.1`). A copy of the license is stored at `launcher/assets/fonts/OFL-1.1.txt` and included with packaged releases.

- <https://github.com/notofonts/noto-cjk>

## cs2-demo-opener Reference

The Steam/CS2 launch flow was informed by the MIT-licensed [drjackild/cs2-demo-opener](https://github.com/drjackild/cs2-demo-opener). No 2D replay, demo parser, web frontend, or source files from that project are included.

For launcher-specific distribution details, see [launcher/THIRD_PARTY_NOTICES.txt](launcher/THIRD_PARTY_NOTICES.txt).
