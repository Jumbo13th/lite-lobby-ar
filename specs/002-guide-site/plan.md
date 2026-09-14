# Implementation Plan: Lite Lobby Guide Site

## Architecture

Astro builds static MDX pages. Starlight supplies localization, article navigation,
search, themes and code rendering. Shared Astro components provide the homepage,
header, help panel, organizational diagrams and screenshot figures. CSS changes are
limited to the agreed palette, typography, spacing and responsive layout.

The public address and repository prefix are configured in docs/astro.config.mjs.
Tests import its exported base rather than maintaining a second prefix.

## Constitution check

This is website-only work. No replicated values, game scripts, Workbench assets or
engine UI are added. Codex reads the shared .claude/skills instructions; managed
integrations and feature 004 remain untouched. The feature's content constraints are
recorded in [spec.md](spec.md#constraints). The supplied review defines the fix scope;
no additional documentation, compatibility framework or placeholder content is added.

## Content and navigation

[The site contract](contracts/site.md) defines reader-facing routes and presentation.
[The content model](data-model.md) defines shared metadata and asset records.

The course sequence and part membership come from docs/src/data/chapters.json.
Astro configuration builds the sidebar from that data. Each course MDX file has
explicit previous/next frontmatter, checked against the same sequence. Optional
references have no sequential neighbors. H2/H3 headings define chapter sections.

Finish shared equipment before creating character variants; finish both variants
before translations and editor display settings. Register and test character/group
prefabs before placing playable units in the world. Configure objectives after the
forces and finish with testing and publication.

Keep one source page per topic in each language. Active crosslinks point directly
to the owning chapter and heading. There is no redirect map, fragment-forwarding
component or content route for a discarded page layout.

## Components and assets

LL_GuideHome renders the contracted homepage entries. LL_Header composes native
Starlight controls with a compact homepage popover. LL_Footer displays pagination
when present and the shared help panel. All components select translations by lang.

LL_Screenshot uses Astro Image for constrained responsive WebP variants and keeps
an unchanged original URL. Development document navigation to an /@fs/ asset needs
the configured base prefix. Both original-image links have the same accessible name.
LL_OrgChart derives its accessible figure name from its caption.

Only referenced PNGs live in assets/screenshots topic folders. Inventory validation
checks paths, filename case, sizes and hashes against tests/screenshots.json.

## Validation and cleanup

scripts/validate.mjs checks localized sources, built URLs and anchors, page language,
image integrity and publication exclusions. tests/chapters.mjs checks the course
sequence, section ownership, supplementary navigation and translated illustrations.
tests/production.mjs exercises real Chromium in both languages and themes at the
widths required by the specification. screenshot-navigation.mjs checks that clicking
either original-image action returns the source PNG in production and development.

The validator clears its previous results once at startup. It reads the actual port
from each started server, tracks browsers, servers and child builds, and releases
those resources on success, failure, SIGINT or SIGTERM. Fixture creation refuses to
overwrite pre-existing temporary paths. Cleanup waits for child builds before
removing owned fixture sources and .validation-dist. Normal dist is hashed before
fixture validation and must remain unchanged.

Validation results are ignored local artifacts. The workflow uploads them on failure
with hidden files included. The normal dist directory alone is the Pages artifact.

## Installation and delivery

[Quickstart](quickstart.md) owns the runtime and dependency policy and maintainer
commands. Dependabot checks npm and GitHub Actions weekly, grouping each ecosystem.
No update is automatically merged.

Pull requests run validation with read-only repository permissions. Relevant pushes
to main validate and publish automatically; manual runs retain a publish option.
Only the deployment job receives pages:write and id-token:write. Publication uses the
github-pages environment and a deployment concurrency group. No remote configuration
or publication is performed by local validation.
