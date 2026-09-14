# Research: Lite Lobby Guide Site

## Website runtime

Astro and Starlight provide static MDX, translated navigation and Pagefind search
without a custom application backend. Markdown headings with explicit IDs populate
the native article contents panel. Homepage HTML anchors do not need that panel.
See [Starlight localization](https://starlight.astro.build/guides/i18n/) and
[search](https://starlight.astro.build/guides/site-search/).

The supported toolchain and update policy are defined once in
[quickstart](quickstart.md#toolchain-and-dependencies). Astro Check's declared peer
range excludes TypeScript 7; retain a compatible TypeScript version rather than
forcing a peer override. Versions resolve through the committed lockfile.

npm 12 reads package.json allowScripts and blocks unapproved dependency install
scripts. The esbuild permission prepares its platform binary during clean installs;
it is not an unused pnpm setting. Name-only approval covers compatible updates.
See [npm install-scripts](https://docs.npmjs.com/cli/v12/commands/npm-install-scripts/).

## Layout and images

Starlight splash pages omit the article sidebar that normally hosts mobile
preferences. A compact homepage popover therefore composes native language, theme
and social controls. Article pages keep the native menu. Conditional footer rendering
avoids space from empty metadata and pagination containers.

Astro Image provides optimized variants while the original PNG preserves small UI
text. A development asset URL starting with /@fs/ needs the base prefix when opened
as a document. Navigation checks cover actual browser requests rather than relying
on a fetch with a generic Accept header.
See [Astro assets](https://docs.astro.build/en/reference/modules/astro-assets/).

## Editorial and source verification

Russian prose uses short connected explanations, concrete verbs and familiar terms.
Explain unfamiliar concepts at the step where they matter; preserve UI labels and
resource names. Avoid literal translations, redundant cautions and unrelated advice.

Mission steps are checked against the supplied screenshots, the example mission and
confirmed Workbench actions. Existing-mission screenshots establish the GitHub-first
sequence and the Workshop route. Missing dependencies link to the course's recovery
section. Git screenshots show repository creation, commits and pushes; the project
chapter owns the repository/addon-folder illustration.

## Project license

The homepage describes the operator-declared APL license and links its official
terms. No LICENSE file exists in this checkout; this feature does not change that
wording or add licensing files.

## Validation and delivery

A probed free port may be claimed before Vite starts, so validation uses the started
server's address. Browsers, server handles and child builds must close before owned
fixture paths are removed. Fixture output is isolated from the publication directory.

The workflow preserves failure evidence, including hidden result directories, and
publishes validated main-branch output through GitHub Pages. Local validation has no
remote side effects.
