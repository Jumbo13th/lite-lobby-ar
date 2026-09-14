# Develop and Validate the Guide Site

For maintainers verifying this feature during maintenance or before publication.

1. Use the latest stable Node.js release (`node` in docs/.nvmrc) and npm. Enter docs;
   with nvm, run nvm install and nvm use. Install npm with npm install --global npm@latest.
   npm ci uses the checked-in lockfile; npm update refreshes it within the manifest
   constraints. TypeScript remains on 6.x until Astro Check supports 7.x.
2. Run npm ci, then npx playwright install chromium.
3. Run npm run validate. Expect bilingual route/link checks, image-integrity checks,
   browser scenarios, and fixture-exclusion checks to pass.
4. Run npm run dev for editing, or npm run build followed by npm run preview to inspect
   production behavior. Open the /lite-lobby-ar/ path on the reported local address.
5. Check Russian through the language selector, narrow the viewport, use keyboard
   navigation and search, and confirm every homepage destination resolves. Expect
   four aligned guide rows in the order listed in contracts/site.md, with no article
   sidebar/contents panel. On narrow pages, open the navbar menu for language, theme,
   and community links; check Escape, outside dismissal, and desktop resizing.
   Article fixtures retain native navigation. Check that no empty footer adds scroll
   space below the help panel.
6. At repository root, run specify integration status --json. Claude must remain the
   healthy default; Codex follows .claude/skills through AGENTS.md.

Search is generated during build and is verified through production preview. The
automated browser fixtures exercise screenshots and code blocks without publishing
sample guide prose. The workflow is prepared but Pages activation and publication
belong to the later content launch.

Validation saves desktop/mobile light/dark captures and illustrated fixture captures
under docs/.validation-results/ (ignored by Git and excluded from publication).
The design checks cover 390, 768, 1440, and 1920 px widths. Review homepage spacing,
Russian title wrapping, article typography, screenshots/captions, tables, asides, and
code blocks in the captures. Temporary article prose is only a layout fixture.
See [research.md](research.md#website-runtime) for the dependency audit and TypeScript
compatibility constraint.

On Windows, a portable runtime can be used without changing the system installation.
After extracting the matching official Node.js Windows ZIP under
`$env:LOCALAPPDATA\Programs`, select it from docs in the current PowerShell terminal:

```powershell
$nodeVersion = (Invoke-RestMethod 'https://nodejs.org/dist/index.json')[0].version.TrimStart('v')
$nodeDirectory = "$env:LOCALAPPDATA\Programs\node-v$nodeVersion-win-x64"
if (!(Test-Path -LiteralPath "$nodeDirectory\node.exe")) {
  throw "Install Node.js $nodeVersion in $nodeDirectory first."
}
$env:PATH = "$nodeDirectory;$env:PATH"
node --version
npm --version
```

This selection applies to that terminal. Replacing a Node installation in Program
Files requires administrator rights.
