# Swift DemoUI Pro promotional website

This directory contains the Hugo website published to GitHub Pages at
<https://nicedayzhu.github.io/SwiftDemoUIPro/>.

The site uses the MIT-licensed [Hugo Blox](https://github.com/HugoBlox/kit)
SaaS landing-page template. Page copy and section order live in
`content/_index.md`; site identity, colors, navigation, and metadata live in
`config/_default/`.

## Local development

Install Hugo Extended `0.162.0`, Node.js 24+, and pnpm, then run:

```powershell
corepack pnpm install
corepack pnpm run dev
```

Create the production output in `public/` with:

```powershell
corepack pnpm run build
```

Pushing a change under `site/` or to `.github/workflows/pages.yml` runs the
Pages deployment workflow automatically.

The download button starts with a language-specific “latest” label and then
reads the current version from GitHub's public API in the browser. The same
request adds a compact trust row below the hero actions with the repository's
Star count and the cumulative download count for published
`SwiftDemoUIPro-v*-win64.zip` assets. Update checks, source archives, standalone
VPKs, manifests, and checksum files are deliberately excluded from the user
download metric. Keep the source label version-free and the statistics
progressively enhanced so a failed API request never leaves stale data or a
broken placeholder on the page.
