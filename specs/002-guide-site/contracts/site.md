# Site and Authoring Contract

## Routes

- Site origin: https://jumbo13th.github.io
- Project prefix: /lite-lobby-ar/
- English home: /lite-lobby-ar/
- Russian home: /lite-lobby-ar/ru/
- Mission guide: /lite-lobby-ar/create-mission/ and /lite-lobby-ar/ru/create-mission/.
- Supporting introductions: example-mission/ and git/ under both locale roots.
- Shared homepage anchors: create-mission, example-mission, git, triad-tactics, api, scripting, website-integration.
- Content pages added later must have corresponding English and Russian sources.

## Guide references

Use this homepage order and these exact titles in headings and in-text references:

| Anchor | English title | Russian title |
| --- | --- | --- |
| create-mission | Create a mission | Как создать миссию |
| example-mission | Open the example mission | Как открыть пример миссии |
| git | Git and Arma Reforger | Git и Arma Reforger |
| triad-tactics | Tune a mission for Triad Tactics | Как настроить миссию для Triad Tactics |
| api | Lite Lobby API | API Lite Lobby |

T097 removes the standalone character-prefab entry: that material is part of the
mission course. The five rows expose seven anchors and three active guide links
(mission creation, example mission and Git). Existing character/group URLs remain
available as redirects to the course.

The main guide references the example and Git in ordinary preface paragraphs.
T098 links Triad Shooting Star to its existing repository and describes recreating
the mission from opening the editor to Workshop publication. Address readers new
to Arma Reforger Tools, without requiring programming. Recommend reading the connected
chapters in order, opening the example alongside them and consulting individual
chapters as needed. Explain Git through restoring working versions and saving time
and effort. Omit the authors' learning anecdote, sidebar explanation, GitHub-backup
and optional-workflow sentences, and the AI-help paragraph. Use natural Russian
technical-book prose. Main-guide project creation recommends using the
repository folder under ArmaReforgerWorkbench/addons as Location and
refers to the Git guide for its setup and rationale. The supplied repository screenshot
belongs in this main-guide section in both languages, stored unchanged under
assets/mission/repository-layout.png. It illustrates the repository and addon folders
for triad-shooting-star-ar/Triad Shooting Star/addon.gproj. The Git guide links back
to this section instead of duplicating its screenshot.

## Reader presentation

The site title is singular: "Lite Lobby guide" in English and "Руководство Lite Lobby"
in Russian. Use the singular form in homepage metadata, the hero, and home navigation.

A compact bilingual introduction precedes the guide list, crediting the Triad Tactics
collective and welcoming the wider Arma community. The Triad Tactics name links to
https://triad-tactics.com/en on English pages and https://triad-tactics.com/ru on Russian
pages. It links the full source using
the configured GitHub URL and the official Arma Public License (APL) at
https://www.bohemia.net/en/licenses/arma-public-license. The license summary mentions
reuse, modification, and sharing for noncommercial Arma use with attribution.
All introduction links open new tabs with rel="noopener noreferrer". Russian license
copy uses complete, natural sentences identifying the license before its permissions.

Homepages use a centered splash layout with exactly five .ll-guide-entry articles
inside one .ll-guide-list, without article sidebars or on-page contents. Rows share
their width, title alignment, and padding. API audiences are compact muted
items in a two-item bulleted list below the title at every width. The API row grows
to fit this list. No split panel,
individual card boxes, or decorative ordinal numbering is used.
Titles and API audience labels own the seven anchors above. The guide list contains no
slogans, descriptions, prompts, decorative text, or preparation notices. Entries have
no links/buttons suggesting unwritten guides can be opened.
The mission guide contains its approved preparation section. Its homepage title is
a native link to the matching locale's article; the row is clickable with visible
hover and keyboard focus. It also appears in the localized sidebar. The example-mission
and Git entries link to their introductory pages; their full procedures will be written
together later. Other topics remain text until their bilingual pages exist.
Search, language, theme, and skip-to-content controls remain usable on each layout.
The left sidebar links between guide pages. The right-hand contents panel links to
headings in the current page. Keep these two navigation levels separate.
On desktop, the right contents panel stays 16rem wide beside the bounded article
column; additional viewport space sits outside their centered layout.
At 64rem and wider, the search field is centered in the viewport; the title and
right-side controls remain at the header edges without overlap.
Below 50rem, a navbar button opens the homepage's compact
#starlight__sidebar.ll-mobile-menu popover with native theme/language selectors and
community links. The button uses popovertarget="starlight__sidebar" and Starlight's
localized menu label. Escape and outside clicks dismiss it; crossing to desktop
closes it so returning to mobile starts closed. Search remains visible in the navbar.
Homepage content has no separate language/theme or social rows. Native article
sidebar navigation continues to use its existing menu.
Search and menu buttons have 44px targets with centered 20px icons sharing the same
vertical center. The homepage popover is at most 14rem wide, constrained to the
viewport, with 0.75rem padding. Its three social icons are 20px, centered in equal
44px targets with equal gaps; the group is centered within the popover.
Discord, GitHub, and Telegram use the URLs configured in docs/astro.config.mjs.
Their named icon links appear in the desktop header and mobile menus. Every social link opens a new
tab with target="_blank" and rel="me noopener noreferrer".

A separate help panel follows homepage and article content. It contains a localized
help heading, brief instructions to ask in #lite-lobby-issues on Discord, a statement
that the authors respond quickly, and one button to the configured Discord invite.
The button opens a new tab with rel="noopener noreferrer". The panel follows normal
page flow, wraps on mobile, and is excluded from search indexing.
Its Russian description is exactly: «Пишите в канал #lite-lobby-issues в Discord.
Авторы быстро отвечают.»

The native footer renders only when it has content. With help as the final content,
the main box ends at most 32px below .ll-help, using the content panel's bottom inset.
Viewport-filling page height must not create additional scrolling on short pages.

Actual article pages retain native sidebar and contents navigation. Temporary
validation articles cover section headings, prose, lists, a table, an aside, code,
and both screenshot sizes. They remain excluded from normal publication output.

## Screenshot component

LL_Screenshot.astro takes src (imported ImageMetadata), alt, caption, and originalLabel
(all strings localized by the caller). Missing/blank text fails validation. Images use
constrained responsive layout, lazy loading, and intrinsic dimensions; small images
must not upscale. The original link points to the unmodified source image emitted by Astro.

## Author commands

Run npm commands from docs: dev, check, build, preview, validate. A clean machine uses
npm ci and npx playwright install chromium before validate. Validation checks the
normal production output, generates isolated fixture output for browser exercises,
cleans temporary content even on failure, and never deploys.

## CI and release

Pushes to main and pull requests touching docs or its workflow validate only. Manual
workflow_dispatch has a boolean publish input defaulting to false. Deployment requires
that input, refs/heads/main, and successful validation. Only the deploy job receives
pages:write and id-token:write. No custom credentials or site APIs are required.

Both screenshot links (image and caption action) open the original in a new tab with
rel="noopener noreferrer", preserving the guide URL and reading position. Main-guide
project creation uses original screenshots 01.png and 02.png with localized captions
and alternative text. The Location path in 02.png is correct: it selects the parent repository folder.

Creation order: workbench-project precedes project-folder. Location selects the parent
repository; Workbench creates Project Name/addon.gproj beneath it. The preface contains
no general naming advice. Its beginner introduction follows the T098 wording above.
A short tip beside Project Name gives one example.
Write for confident computer users without Arma Reforger or development knowledge;
define unfamiliar terms where needed and keep explanations tied to the current action.
Do not claim unsupported Workbench naming restrictions.

The Project Name tip is the first naming-consistency reminder: preserve the Desert
Storm example, connect it to naming patterns on screenshots, and explain that Arma
conventions depend on the kind of item. Keep the advice concise in both locales.

Editorial rule: retain the information needed to understand and complete an action;
remove repeated statements, generic encouragement, and unnecessary introductions.
T098 removes the previously requested AI-help paragraph in both locales.

Both mission guides add lite-lobby-dependency after project-folder, illustrated by
03.png and 04.png. Explain dependency purpose and successful loading. Workshop file
selection uses the .gproj in LiteLobby_699319AAA5BFC57F; retain Arma Reforger and verify
Launcher registration before reopening the project.

Missing addon recovery (T053) replaces proactive Add Existing Project instructions.
The missing-addon-dependencies subsection shows the user-supplied error, GUID search,
scan menu, and addons-folder screenshots in that order. It links the official Workshop
page, distinguishes installing a mod in-game from discovering it in Launcher, and uses
Scan for Projects with the game's addons directory. No reminder to preserve the existing
Arma Reforger dependency appears in the normal plus-button step.

World creation starts at create-world, after the project/dependency section. Both
locales use identical section anchors and original screenshot imports. Base world:
ArmaReforger/worlds/Eden/Eden.ent; project output: Worlds/Eden/ShootingStar_Ep1.ent.
The systems-layer rename command must be source- or operator-confirmed before inclusion.

Game Mode Setup (T056) follows world creation with game-mode-setup,
game-mode-template, generate-systems, and create-mission-header anchors. Both locales
use 11–18 in order. The template is GameMaster.conf, and the generated header is
Missions/ShootingStar_Ep1.conf. Do not present wizard completion as a finished mission.

Historical character-prefab entry (T060; separate route and homepage placement
superseded by T097): /character-prefabs/ and /ru/character-prefabs/ are real
introductory guides. The main mission guide links to the matching locale; homepage
and sidebar include this fourth guide. Only Triad Tactics/API remain noninteractive.
Both pages include naming-and-folders, TSS_, the US_Army directory and the supplied
official Faction Creation link. Keep all six homepage entries in their existing order.

## Historical connected chapter contracts (T080–T096; layout superseded by T097)

These paragraphs record prior layouts. Their separate hubs, topic pages and active
route requirements no longer define navigation. T097 below owns the current
structure; technical dependencies and screenshot preservation still apply.

This supersedes single-page structure and the character-guide label above.
/create-mission/ is a reading-path overview. /character-prefabs/ is Characters and
groups / Персонажи и группы; /characters/ and /groups/ beneath it are subordinate
overviews. Hubs link chapters; chapters contain context, parent and previous/next
links. Preserve legacy fragments and rewrite current crosslinks. Keep all 116
previous figure occurrences per locale (50 mission, 66 prefab) and all 113 PNG hashes.
Native sidebar branches mirror the hierarchy.
Keep prerequisite explanations and screenshot mismatches; remove redundant prose.

T087–T090: topics own related instructions even when their screenshots were taken
at different times. Mission sidebar categories and character task titles are
independent of the overview reading route. Retired chapter routes are excluded
from navigation/search and forward to current topics, including moved anchors.

T092–T095 supersedes the character partition above: group complete practical tasks
around a prepared object. Finish BaseLoadout before creating ODA_Base and finish
ODA_Base before creating role variants. Topic lookup links to canonical task
sections; it does not require collocating similar components from different stages.
Map setup precedes Game Master checks, and playable forces precede trigger checks.

T096: equipment chapters finish the character loadout; editor display configuration
belongs to a shared Name/role chapter after both variants and their translations
exist. Catalog registration/testing follows. Active roles routes have no default
redirect; older links to moved UI Info sections retain their targets.

## Course contract (T097)

T097 supersedes the fragmented hierarchy: one introduction followed by eight numbered
course chapters, with tasks as on-page sections. Left navigation and reading order
agree; the right TOC describes the current chapter. Parts group chapters (1–2, 3–5,
6–8); former character/group overviews and the separate homepage entry are retired.
There is one course overview at create-mission/. Its three parts do not add landing
pages. The sidebar groups chapters into those parts, and native previous/next links
follow the introduction and all eight chapters without intermediate hubs. Unit
planning and prefab operations remain optional references beside Git and the example
mission. Retain all 116 figures per locale and old URLs, including the former
character/group sub-overviews. Chapter boundaries follow completed work; there is
no eight- or nine-image ceiling.
