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
reads the current version from GitHub's public latest-release API in the
browser. Keep the source label version-free so a failed API request never
leaves a stale release number on the page.
