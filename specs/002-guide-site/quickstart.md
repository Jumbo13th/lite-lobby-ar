# Develop and Validate the Guide Site

For maintainers changing guide content, checking a pull request or publishing updates.

## Toolchain and dependencies

Use Node 26 and npm 12.0.2. docs/.nvmrc fixes the Node major; update that major
deliberately after compatibility testing. Local validation uses Node 26.8.2.
All dependencies and devDependencies use caret ranges. Dependabot checks weekly and
can propose range changes for review. package-lock.json records exact resolutions:
use npm ci for installation and npm update for intentional updates within the ranges.
TypeScript remains on 6.x because Astro Check does not support 7.x. Do not bypass
peer compatibility. GitHub Actions use maintained major tags; npm is explicitly
selected in CI so its install-script behavior matches local validation.

## Commands

From docs/:

1. Select Node with nvm install and nvm use, or use the portable Windows runtime below.
2. Run npm install --global npm@12.0.2 if needed, then npm ci.
3. Run npx playwright install chromium.
4. Run npm run validate. All source, build, browser and fixture checks must pass.
5. Use npm run dev for editing. For search, run npm run build and npm run preview;
   open the configured repository prefix on the reported local address.

On Windows, after extracting the official Node ZIP to the user Programs directory:

~~~powershell
$nodeDirectory = "$env:LOCALAPPDATA\Programs\node-v26.8.2-win-x64"
if (!(Test-Path -LiteralPath "$nodeDirectory\node.exe")) {
  throw "Install the supported Node runtime in $nodeDirectory first."
}
$env:PATH = "$nodeDirectory;$env:PATH"
node --version
npm --version
~~~

Stop development and preview processes before npm ci if they hold native module
files open. Changing PATH applies only to the current terminal.

## Verification

Review four aligned guide rows on both homepages. Check the course sidebar,
previous/next links, supplementary return links, heading navigation and search.
At 390, 768, 1440 and 1920 px, inspect both themes and languages for clipped controls,
overflow, captions and readable code. Click original images in production preview
and development.

Validation clears old captures and writes the current run to .validation-results/.
Temporary article sources and .validation-dist/ are removed after completion,
failure or a handled interruption. These paths and logs are ignored by Git and
excluded from publication. If stale fixture paths exist before a run, inspect them;
validation refuses to overwrite them.

## Publication

Relevant pushes to main validate and publish automatically. Pull requests validate
without publishing. Manual workflow runs can validate only or publish main with the
publish option. Configure the repository's Pages source as GitHub Actions before
its first deployment. A failed validation cannot deploy; download its failure
artifact to inspect screenshots.
