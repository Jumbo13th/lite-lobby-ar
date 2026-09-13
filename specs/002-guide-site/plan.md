# Implementation Plan: Lite Lobby Guide Site

**Branch**: 002-guide-site | **Date**: 2026-09-12 | **Spec**: [spec.md](spec.md)
**Input**: Approved complete guide-site scope within feature 002: design and current tooling first, collaborative guide content next, using shared Claude skills.

## Summary

T098 is an editorial update to the existing RU/EN course introductions, following
the six operator corrections in spec.md. Preserve the course structure, rebuild
the site and verify the rendered paragraphs and existing links; no new UI or tests.

Build the bilingual Starlight foundation with existing shared SDD skills. Present five
guide entries and two API audiences on a centered homepage, with a consistent article
design verified through temporary content. Author the agreed bilingual content with
the operator and connect it to the site. T097 brings mission, character and group
creation into one course with three parts and eight chapters. Feature 002 includes
both foundation and content; publication
still requires the later launch decision.

## Technical Context

**Language/Version**: Astro 7.3.2, TypeScript 6.0.3, JavaScript, Node 26.8.2, npm 12.0.2.
**Primary Dependencies**: Starlight 0.42.0, markdown-remark 7.3.1, Astro Check 0.9.10, Playwright for browser validation.
**Storage**: MDX pages and 113 original PNG files; no database.
**Testing**: Astro Check, production build, local-link/translation/asset checks, Chromium browser checks at 390, 768, 1440, and 1920 px in both languages/themes.
**Target Platform**: Static GitHub Pages website; local Windows and standard Ubuntu CI.
**Project Type**: Independent static website under docs.
**Performance Goals**: No page overflow; constrained responsive images with lazy loading; unused screenshots absent from normal output.
**Constraints**: Design/tooling before collaborative content authoring; main-only manual deployment after the launch decision; shared SDD integration preserved.
**Scale/Scope**: Two homepage locales, five guide entries, seven topic/audience anchors, three active guide links, one bilingual course with three parts and eight chapters, agreed supporting content, shared site/figure components, and validation.

## Constitution Check

- SDD: specify and clarify capture the operator's existing answers; this plan and tasks are written before site code. Run read-only analyze before implement.
- Engine fidelity, replication, authority, and Workbench changes: none. The feature only adds static web files; game-specific UI/string-table rules do not apply.
- Scope: the operator explicitly requested technical documentation infrastructure and the Triad Tactics guide title, which authorizes those references and supersedes the general foreign-name/documentation restriction for this feature.
- Assets: screenshots are relocated byte-for-byte. No game prefab, GUID, audio, or localization-table changes.
- Simplicity: use built-in Starlight controls and layouts, a shared homepage component, and small header/hero wrappers; no application framework, backend, custom search service, or live integrations.
- Publication: no commit, push, Pages activation, or deployment as part of this implementation.
- Shared configuration: preserve Claude skills, constitution, templates, and existing specifications. Codex explicitly reads .claude/skills; no .agents copy.
- Post-design check: all requirements fit the approved scope; no constitution amendments needed.

## Project Structure

- AGENTS.md: shared instruction and skill entry points.
- docs/: package configuration, Astro/content configuration, localized homepage sources, LL_Screenshot component, shared image assets, and scoped ignore rules.
- docs/scripts/ and docs/tests/: reproducible validation and asset inventory; temporary fixture pages/output excluded from normal builds.
- .github/workflows/docs.yml: validation and explicitly manual publication.
- specs/002-guide-site/: specification, checklist, research, content model, interface contract, quickstart, plan, and tasks.

## Implementation Design

### One course with parts and chapters (T097)

T097 supersedes the earlier page boundaries recorded later in this plan. The user
chose a book with 6–8 chapters.
Use eight canonical MDX chapters under create-mission, not composition of old full
pages or parallel tutorial/reference copies:

| Chapter / route | Existing sources | Result |
| --- | --- | --- |
| 1 project | project, dependencies | Addon with Lite Lobby |
| 2 world | world, game-mode, lobby-game-mode, scenario, map | Runnable world and configured map |
| 3 base-character | characters/base, equipment | Finished BaseLoadout |
| 4 character-variants | weapons, inventory, radio-operator, additional-equipment | Armed shared parent, then two equipped children |
| 5 character-editor | localization, roles, characters/catalog | Translated names/roles and tested Game Master entries |
| 6 groups-and-slots | groups/setup, roster, catalog; forces, vehicles, freeze | Groups placed, slots and starting areas configured |
| 7 objectives | map-markings, conditions, capture, loss, timer, supremacy, briefing | Mission tasks, trigger rules and briefing |
| 8 publishing | testing, publishing | Final checks and publication steps |

H1 identifies the numbered chapter, H2 owns a substantive stage and H3 its substeps.
Preserve existing explicit IDs. Remove old parent links, repeated introductions and
page-ending transitions inside merged text. Keep explanations and screenshot mismatch
notes. Do not split a chapter just to meet the rejected nine-figure limit.

Metadata holds one introduction, three parts (1–2, 3–5, 6–8), eight course chapters
and two existing supporting pages; sequence contains introduction plus eight chapters.
Parts group chapter links, without additional landing pages. Retire all three former
character/group hubs, and remove their standalone homepage entry (five rows remain).
Sidebar: course → parts → chapters, then supporting material. Old routes become
hidden noindex stubs forwarding directly to final chapter sections. Use neutral
LL_GuideFragments fallback wording. No new UI framework or dependency.

Bounded review: all chapter boundaries, headings and transitions; BaseLoadout→ODA→
variants and equipment→translations→editor display. Validate 116 figures per locale,
113 original hashes, course navigation, locales and old destinations. Inspect the
desktop/mobile overview and a long chapter. No game assets or public launch changes.

### Historical practical tasks and topic grouping (T087–T096; superseded by T097)

The page boundaries, topic index, separate hubs and figure ceiling below record
earlier implementations. T097 above replaces that navigation. The verified task
dependencies, screenshots and mismatch notes remain part of the course.

One canonical walkthrough serves two entries: sequential tasks and a topic index
on the overview linking directly to their sections. Do not create four empty
prefabs first or separate an outfit change from its storage corrections. T096 keeps
editor display settings out of equipment tasks: translations, UI Info and catalog
registration follow after both equipped character variants exist.

| Character chapter | Input | Result | Screenshots |
| --- | --- | --- | --- |
| base: create a base character | Addon with Lite Lobby | BaseLoadout with clothing and backpack | 23–30 |
| equipment: personal items and radio | Clothed BaseLoadout | Personal radio, pocket items and watch; BaseLoadout ready | 31–37 |
| weapons: shared equipment and weapons | Completed BaseLoadout | ODA_Base child with armor, webbing, weapons and grenades | 38–44 |
| inventory: ammunition and pouches | Armed ODA_Base | Ammunition distributed; ODA_Base ready | 45–49 |
| radio-operator: character with backpack radio | Completed ODA_Base | Child with radio, changed clothing and corrected storage | 52–56 |
| additional-equipment: additional character weapons | Completed ODA_Base | Commander child with launcher and binoculars | 66–69 |
| localization: names and translations | Both character variants | Registered table containing both names | 58–64, 70 |
| roles: name and role in Game Master | Both character variants and string table | UI Info Name/Authored Labels for both | 57, 65, 71 |
| catalog: add characters to Game Master | Completed characters and prepared world/map | Catalog entries and checked characters | 72–78 |

Use meaningful task titles; ODA, the communications sergeant and commander remain
examples in the text. The topic index locates both radios, weapon slots, initial
inventory, clothing replacement, names and roles without duplicating instructions.
Restore map-markings as a distinct late task; map retains early MapEntity setup.
Mission route: project/dependency/world/wizard/game-mode/header/map, characters and
groups, playable groups/vehicles/freeze zones, markings/triggers/briefing, final
test/publication. Keep the approved trigger pages. Fix catalog prerequisites,
declare extra group/character inputs, and label the full briefing image as an
example result. Retarget all original and subsequently published fragment maps.

Review scope: every changed chapter's starting object, prerequisite, result and
illustrated state, plus overview routes and cross-guide transitions. Automated
checks retain image/link integrity and exercise the new-reader route and topic
index; remove assertions that enforced the rejected component-only partition.
One bounded editorial/dependency pass, existing website validation and live preview.
No engine facts, game assets, dependencies or publication change.

T096 review is bounded to these character chapters, overview links and their legacy
destinations. Screenshot 57 shows the old name: configure the role first, then the
translated name (65). Build the translation runtime after entering both rows. Keep
commander-name on the translation step and commander-role on UI Info. Reactivate
roles without a default redirect; preserve all other historical destinations.

T091: add a Triggers sidebar category beside Map and briefing. Retain capture as
the capture-trigger chapter and conditions as shared trigger settings. Move loss,
timer and superiority sections into their own chapters with links to the common
settings. Update metadata, overview route, pagination, link labels and existing
fragment maps. Retain nine illustrations across these five pages and all old
anchors. Existing validation covers the new routes; no runtime behavior changes.

Keep four aggregation pages. Mission navigation groups chapters into project/world
setup, lobby/units, map/briefing, triggers, and testing/publication. Keep the complete
Game Mode Setup wizard on one page; separate scenario metadata, Lite Lobby game-mode
replacement, and freeze-time settings. The port is an example inside the capture
trigger chapter, not its title.

The shared chapter metadata owns topic labels and sidebar categories independently
of the suggested reading sequence. Update overviews and all crosslinks. Extend
existing fragment forwarding to moved chapter anchors; retired chapter routes use
small navigation pages excluded from search, with direct links and fragment-aware
forwarding. No new framework or dependency is needed. Validate the 116 illustrations
per locale, all old destinations, task dependencies and direct-entry browser flows.
The figure limit is nine; do not split a task solely to distribute screenshots evenly.
No game assets, engine behavior or publication changes.

### Reader interface

Place LL_ProjectIntro.astro between the homepage hero and guide list. Two short
localized paragraphs credit the Triad Tactics collective, welcome other communities,
link the full source using the existing GitHub social URL, and summarize the
operator-confirmed APL with its official license link. Keep the text in this shared
component, selected by route language, and use the existing typography and palette.
The Triad Tactics credit links to its website in the matching language. Credit,
source, and license links open new tabs. No new heading, card, or dependency is needed.

Use root English plus ru locale. Set both homepages to Starlight's splash template
with a native hero so they omit article sidebars, contents panels, and the separate
article-title divider. LL_GuideHome.astro renders five guide rows in the order
defined in contracts/site.md from one
shared template inside a single bounded list. Rows share padding, title alignment,
and small unboxed icons. Subtle rules separate them; individual card
boxes and ordinal numbers are omitted. The two API audience labels form a bulleted
list below its title at every width, with the row growing to fit. Localized titles and API labels
stay in the MDX pages; the component
owns their common structure. Omit slogans, descriptions, section prompts, eyebrow
labels, and preparation notices. Retain all seven
anchors in contracts/site.md. Mission creation, the example mission and Git are the
three active guide links. Unpublished entries have no fake links, buttons, or hover
effects implying an action. No procedural or API guide content is authored during
the current design/tooling stage.

LL_HomeHero.astro wraps the native Hero for homepage typography only.
LL_Header.astro composes the native Header with a homepage-only MobileMenuToggle
and compact #starlight__sidebar.ll-mobile-menu popover containing MobileMenuFooter.
Use the native localized button and preference controls. Reserve space for the 44px
menu button beside search below 50rem. Give both buttons centered 20px glyphs and
44px targets. Limit the homepage popover to 14rem with 0.75rem padding. Override the
desktop social links' negative margins inside it; use centered 44px targets with
20px icons and equal spacing. Native popover behavior handles keyboard and
outside/Escape dismissal; a small breakpoint listener closes the popover at desktop
widths. Article pages keep their native sidebar menu. At 64rem and wider,
use equal outer header columns around a responsive search field capped at the native
22rem width to center it in the viewport. Between 50rem and 64rem, use natural-width
search and preference columns with a flexible title column so the article sidebar's
width does not push language controls offscreen. Retain standard
article navigation. Scope the wrapper to splash pages so
future article hero usage keeps normal framework behavior.

Configure the three supplied community URLs once through Starlight's social option.
Use LL_SocialIcons.astro as Starlight's SocialIcons override, retaining the native
icons, accessible service names, and styling while opening links in a new tab with
target="_blank" and rel="me noopener noreferrer". The desktop header and article
mobile menu use this override, including the new homepage popover. Remove the social
row below the guide list. Allow desktop search to narrow enough
to keep all header controls clear.

Wrap the native Footer with LL_Footer.astro to place one compact help panel after
homepage and article content. Keep its English/Russian text together in the component,
selected by the route language. Read the Discord destination from the existing social
configuration. Name #lite-lobby-issues, mention the authors' quick responses, and use
one new-tab button. A subtle top rule separates the panel; let the button wrap below
the text on narrow screens. Exclude the repeated panel from search indexing and retain
the native footer beneath it only when route editUrl, lastUpdated, pagination, or
configured credits supply actual content. Set --sl-main-pad to zero so the content
panel's 24px bottom padding is the only trailing inset when help is last. No help
widget script or dependency is needed.

Keep the Triad Tactics neutral charcoal palette and #d2b853 gold, with darker gold in
light mode. Use a bounded homepage width, restrained title typography, consistent row
spacing, subtle borders, and small built-in icons. Shared CSS tokens set article heading/body
sizes and line-height; Expressive Code's supported style overrides align code sizes
and corner radius. Screenshot frames/captions use the same spacing and border rules.
From Starlight's 72rem desktop breakpoint, give the right contents panel a fixed 16rem
width. Center the article and contents together in a bounded layout sized from the
existing 46rem reading width, article padding, and contents width. Keep the contents
beside the article on large screens. Use native title dividers and mobile navigation.
No fonts, decorative bitmap assets, dependencies, or new interactive framework are added.

### Images

Preserve the source inventory before moving images to docs/src/assets/guide. Add
LL_Screenshot.astro with constrained Astro Image output and required localized strings.
Use Astro's built-in optimization and original src URL. Do not eagerly import the
library into the homepages.

### Validation

Use scripts/validate.mjs to check source locale parity, normal output paths, local
links/anchors/assets, screenshot hashes, and internal-file exclusions. Start the
production preview on loopback for browser checks. Exercise screenshots, code blocks,
and translated captions using temporary MDX articles with paragraphs, a list, a table,
an aside, and section headings; build them to a separate ignored
output, and remove temporary pages/output in finally cleanup. Fail rather than
overwrite a pre-existing temporary path. Verify normal build output is still clean.
Use real Chromium to test search results, keyboard and language/theme controls,
homepage list alignment and absent sidebars, article/mobile navigation, image intrinsic
sizing, and original bytes. Capture both themes/locales for homepage and article
visual review. Retain the existing image/hash/output-exclusion checks. All validation servers
must be stopped at the end.

### CI and release

Use the Node version in docs/.nvmrc on Ubuntu, npm 12.0.2, npm ci, Playwright Chromium
installation, and npm run validate.
Default permissions contents:read. A workflow_dispatch boolean publish defaults to
false. Only a successful manual main run with publish=true may upload the normal
dist artifact and run deploy-pages with pages/id-token write permissions. Pages
activation remains deferred.

### Guide authoring

After design acceptance, write the mission-creation guide with the operator using
the supplied example mission and screenshots as evidence. First review its outline
and prerequisites; then draft and verify one section at a time. Finalize English and
Russian after the operator checks the Workbench steps and terminology. Write supporting
guides when the walkthrough needs their procedures: GitHub Desktop, opening the example, character/group
prefab creation, Triad Tactics tuning, and API integration. Verify Enforce and website API examples against
their implementations. Add matching English/Russian sources and real homepage/sidebar
destinations as each guide is ready. Extend validation to the resulting routes and
images. Content work remains part of this feature, with its acceptance tasks open
until the operator review is complete.

The approved first section can enter the local site before the whole walkthrough is
finished. Add preparation in both create-mission.mdx files: the three prerequisites
and the official game-data setup link. Connect its homepage title and row using one
native anchor, with visible hover and keyboard focus, and add the localized sidebar
entry. Build the destination from the configured base and route locale. Keep other
topics as text until their pages exist. Verify keyboard entry, search destinations,
and article language switching; the remaining outline and guide tasks stay open.

### Mission guide outline for review

Use Starlight's native left sidebar to navigate between guide pages and its right-hand
table of contents to navigate chapters within the current page. Do not duplicate
chapter links in the left sidebar. The mission-navigation preview uses preparation
plus the eight sections below; their labels and procedures remain under review. Add
only authored destinations to the working site.

Prerequisites: Arma Reforger and matching Arma Reforger Tools installed, base-game
data visible in Workbench, and the Workshop installation of Lite Lobby available
as a project dependency. The operator confirmed Workshop as the main installation route.
The full example has additional dependencies; document those in the example-opening
guide instead of making them prerequisites for every Lite Lobby mission.

The proposed reader order follows the supplied screenshots but brings playable
characters and groups forward so the reader can check the lobby before adding objectives.

| Section | Result to verify | Screenshot evidence / supporting guide |
| --- | --- | --- |
| Create the project | Own addon with Lite Lobby dependency | 01–04 |
| Create the mission world | Saved sub-scene of Everon, with an own systems layer | 05–10 |
| Configure the mission | Scenario header, lobby game mode, map, AI/navigation setup | 11–22; navigation needs a verified step |
| Add playable squads | Roles, groups, callsigns, and a working lobby roster | 106–110; link character/group prefab guide, using 23–90 |
| Add vehicles and starting areas | Vehicle factions/group assignments and intended start-area behavior | 111–113 |
| Write the briefing | Readable mission description and named map zones linked from its text | 91–95, 105 |
| Add objectives and conditions | Capture area, flags, loss conditions, timer, and visible notifications | 96–104 |
| Test the mission | Lobby → briefing → game, spawn/freeze behavior, objectives, and round ending checked | 104, 110; editor and multiplayer checks |

Before preparation, write ordinary preface paragraphs linking to the example and Git
guides. Explain that the Lite Lobby authors learned by studying existing missions.
Recommend Git using a concrete recovery example: explicitly committed versions allow
readers to restore a working state after bad changes; pushing copies the history to
GitHub for recovery after computer or disk loss. Both workflows remain optional.
Use natural Russian technical-book prose, define unfamiliar terms, and keep the English
meaning equivalent. Avoid slogans, decorative tip boxes, and repeated optional labels. Use canonical
guide titles in contracts/site.md. Link Triad Tactics tuning and API integration as optional follow-ups where
the walkthrough needs those details. Preparation is approved; review the remaining
outline before drafting the procedural sections. Exact example settings and unsupported screenshot details remain
subject to the source checks recorded in research.md.

### Git and Arma Reforger guide

Explain Git in the preface after learning from example missions.
In the project-creation section, recommend a repository under
Documents/My Games/ArmaReforgerWorkbench/addons containing a separate addon folder.
Use the supplied screenshot's example: triad-shooting-star-ar is the repository root;
Triad Shooting Star is its addon folder and contains addon.gproj. Workbench's Location
points to the parent repository folder. Workbench creates the addon folder automatically.
Refer readers to Git and Arma Reforger for setup and rationale.
This folder arrangement is a recommendation for readers choosing Git, not a prerequisite.
Start the two supporting guides with short bilingual introductions: the example page
links the supplied mission source, and the Git page distinguishes saving, committing,
and pushing. Explain the recommended repository/addon folders in main-guide project
creation, with a link to Git for repository setup. Save the supplied repository screenshot
byte-for-byte as docs/src/assets/mission/repository-layout.png and render it with
LL_Screenshot and translated text in both main guides. The Git introductions link back
to this section without duplicating the screenshot. Link supporting pages from the
preface, homepage, and sidebar. Detailed procedures and screenshots remain
under collaborative review in T028/T034.
Explain Save in Workbench → review files → commit in Desktop → push to GitHub, including
how to confirm the remote commit. Cover the complete mission folder, source assets,
world layers, metadata, and an ignore list verified against generated project files.
Demonstrate recovery with a simple bad commit and a history-preserving revert, followed
by Workbench reload. Readers can complete the main mission guide without Git. Branching
and team workflows can be linked as further reading.

Prepare a separate demo mission repository for content verification and screenshots;
use the existing Desktop installation without resetting its accounts or repositories.
Proposed captures: correct folder selection, Changes/commit panel, pending Push origin,
and History/revert. Link official illustrated installation/sign-in documentation.
Draft and review the text before capturing these images; new captures belong under
docs/src/assets/git/ when available, preserving the original guide-image inventory.
This turn plans that workflow; it does not create, commit, or publish a demo repository.

### Current dependencies and local runtime

Audit official Node releases and npm registry metadata for every direct dependency.
Upgrade to the latest stable compatible set, record any peer-version constraint,
refresh the lockfile, and align package engines, .nvmrc, CI, and maintainer steps.
Install the current Node runtime locally where possible without changing unrelated
projects. Verify installation through the site's actual npm commands.

### Cleanup review

Review docs source/configuration/tests, its GitHub Actions workflow, and feature 002
artifacts once for correctness, simplicity, consistency, and reproducible cleanup.
Finish when demonstrated defects are fixed, generated scratch artifacts are excluded,
shared integration and image integrity remain intact, and production validation passes.
Keep the accepted design and dependencies unless a specific finding requires a change.
Do not author guide content or publish as part of cleanup.

## Execution Dependencies

Finish SDD artifacts and read-only analysis first. Root owns package/config/homepages
and image migration. Browser validation can be implemented independently once
contracts and package script names are fixed. Workflow implementation can proceed
independently against the command contract. Finish with integrated validation.


### Screenshot navigation and first illustrated steps (T046)

Use standard new-tab links for both LL_Screenshot actions, with rel="noopener noreferrer".
Keep the source guide open without adding a viewer dependency. Add original screenshots
01.png and 02.png to both main-guide routes beside the Launcher/new-project instructions.
Preserve the supplied repository-layout figure. Screenshot 02 correctly shows the
repository root as Location. Show the automatically created addon folder after Create.
Only cover the visibly supported menu, project name, Location, and Create action here;
Project Settings and dependency selection remain the next collaborative section.
Validate actual mouse and keyboard new-tab behavior and original image integrity.

### Correct creation order and naming (T047)

Keep the existing section anchors. Under create-project, put workbench-project first:
Launcher with 01.png; Project Name and parent Location with 02.png; Create. Explain
Git repository selection alongside Location, without instructing manual addon-folder
creation. Follow with project-folder: Workbench's generated addon directory and
addon.gproj, illustrated by the supplied Explorer screenshot. Remove the erroneous
warning about screenshot 02. The naming presentation is superseded by T048 below.

### Beginner audience and contextual explanations (T048)

Write for confident computer users without Arma Reforger or developer experience.
Remove the preface naming text (T049 supersedes the two-sentence T048 version).
Place one short naming tip
immediately after the Project Name instruction. Explain addon before creating one and
repository when selecting Location; do not introduce repository naming rules here.
Retain the verified parent-folder behavior, screenshot order, stable anchors, optional
Git/example workflows, and English/Russian parity. Check the build and rendered pages.

### Learning tip after the preface (T049)

Replace the rejected naming paragraph with a short bilingual Starlight tip before
preparation. Use a brief reader scenario to explain learning actively and asking AI
for help, including a plain explanation of a coding agent. Credit the human authors'
practical experience; omit commentary about how the text was prepared, as requested
in the subsequent copy edit. Keep the contextual Project Name tip and all procedures unchanged.

T050: extend the existing Project Name tip with one brief paragraph about following
screenshot naming patterns and conventions for each type of item. Preserve the approved
example and its placement. No additional preface advice or screenshots are needed.

T051: review all eight published MDX sources and shared homepage/help/navigation copy.
Edit repetitive mission/Git/example prose in both locales; retain already concise
navigation, help text, licensing conditions, and image captions. Keep approved naming
examples, screenshots, step order, and required explanations. Ask readers to supply
AI with the guide-page URL, step, and screenshot. Verify build, links, locales, image
integrity, and rendered desktop/mobile content. No additional guide procedures.

T052: append the bilingual lite-lobby-dependency subsection after project-folder.
Use screenshot 03 for File → Project Settings and 04 for Dependencies + / browse.
Select the .gproj inside the supplied Workshop-folder example; no invented filename.
Save with OK, ensure Lite Lobby appears in Launcher (Add Existing Project if needed),
and reopen the mission project. Verify LiteLobby in Resource Browser, as visible in
05; reserve image 05 for the next world-creation section. Preserve all prior steps.

T053 supersedes T052's proactive Launcher registration paragraph. Keep the normal
save/reopen/result flow, then add missing-addon-dependencies as a troubleshooting
subsection. Use new shared assets under docs/src/assets/mission in reading order:
missing-addon-dependencies.png, workshop-guid-search.png, scan-for-projects.png,
workshop-addons-folder.png. Preserve the four user attachments byte-for-byte outside
the fixed 113-image inventory. Explain installation and discovery as separate causes;
scan Documents/My Games/ArmaReforger/addons, not the Workbench project directory.
Use LL_Screenshot with translated text and new-tab originals. Validate links/anchors,
original hashes, mobile/desktop rendering, and both locales; rebuild the preview.

T054: add create-world with open-world-editor, create-subscene, and save-world sections
in both languages. Use 05 for Quick Launch, 06 for ArmaReforger/worlds/Eden/Eden.ent,
07/08 for File → New World and Sub-scene (of current world), and 09 for saving to
Worlds/Eden/ShootingStar_Ep1.ent. The save dialog has New Folder, allowing folder
creation without inventing a Resource Browser context-menu action. Use File → Save
World (Ctrl+S), supported by official docs. Prepare systems-layer with 10 only once
opening Rename layer is confirmed. Do not introduce Scenario Framework components.

T054 clarification resolved: right-click default under the own ShootingStar_Ep1 scene
and choose Rename; enter a_systems, confirm OK, and save. Explain a layer as a group
of scene objects and its intended use for mission systems. The locked Eden layer is
not the rename target. Use screenshot 10 with localized text.

T055: simplify the save instruction to Ctrl+S. Before screenshot 09, add a concise
bilingual tip explaining world-file naming (DesertStorm, SilentPort), the Worlds/Eden
directory structure, and the operator's warning that renaming can cause engine
problems. Distinguish the example's episode suffix from words separated by underscores.
Preserve the screenshot filename and path; rebuild and check the rendered text/order.
Operator clarification: Triad Tactics games have three episodes. Recommend its _Ep1
suffix convention for Triad Tactics missions and allow omission for other missions.

T056: append game-mode-setup with game-mode-template, generate-systems, and
create-mission-header anchors in both mission guides. Import original images 11–18
with localized captions/alt. Activate a_systems by double-clicking it; select Plugins
→ Game Mode Setup, Template → GameMaster.conf, then Scan world, Next, Create entities,
Next, Create header, Next, Close. Save the world with Ctrl+S in World Editor.
Verify against the supplied screenshots and the in-repository official plugin source.
Build and check images, links, locale parity, and mobile/desktop presentation.
T056 operator correction: omit NavMesh setup; only map textures remain to configure.
Keep screenshot 16 unchanged, without turning its navigation reminder into a task.

T057: remove the final wizard paragraph in both locales. Append mission-details
under the game-mode-setup section, using original 19.png. A short field table covers
World, Name, Author, Description and Player Count; a following instruction disables
all four Save Types flags and explains they control in-game progress saves. End with
Ctrl+S for the edited header. Verify SCR_MissionHeader attributes and the example
config; rebuild and check both locales. Game mode replacement remains next.

T058: append lite-lobby-game-mode in both mission guides. Return to World Editor,
select GameMode_Editor_Full under the own a_systems layer and press Delete. Use 20
to identify the removed root and children. Activate a_systems, find LL_GameMode_Lobby.et
in LiteLobby through Resource Browser search, and drag it into the viewport (21).
Explain the new instance and the three retained system objects, then save with Ctrl+S.
Verify the prefab path locally and placement/removal against official editor docs;
build and check screenshots, locale parity and responsive preview.

T059: add map-setup after lite-lobby-game-mode in both languages. Use 22.png and
the existing screenshot component. Explain Map Geometry Data and Satellite background
image, using the existing Eden resources selected with the resource picker. Verify
resource paths against the example's a_systems.layer and semantics against Bohemia's
2D Map Creation documentation. Clarify the highlighted component action separately;
do not infer removal from a red rectangle. Build and check both locale layouts.
T059 clarification: SCR_CompassComponent and SCR_WristwatchComponent should already
be present. If missing, use Add Component and search/select the missing component.
Assign resources on the selected scene instance's SCR_MapEntity root properties;
do not instruct Apply to prefab or modifications to the original game asset.

T060: create en/ru character-prefabs.mdx with an introduction and naming-and-folders
section. Add a character-prefabs section/link at the end of each mission guide.
Enable the existing fourth homepage entry and add the sidebar slug in the same order.
Update validation's actionable-guide lists from three to four; keep unwritten Triad
Tactics/API entries noninteractive. Use a concrete filename prefix example and the
vanilla Prefabs/Characters/Factions/BLUFOR/US_Army directory. No new assets or prefab
edits; validate routes, links, language switching, search, and responsive rendering.

T061: extend both character guides before naming-and-folders with authenticity,
unit-organization, and green-berets subsections. Add a lightweight shared Astro
organization-diagram wrapper around semantic nested lists, using existing theme
variables and responsive columns; no diagram library or client runtime. Show a short
selection chain plus ordinary command hierarchy, SF company/ODA hierarchy, and the
two six-person base split-team prefabs (roles sourced from the pinned example).
Store the supplied image unchanged in assets/mission, outside the 113 originals,
and render through LL_Screenshot. Cite historical claims near the relevant text.
Validate new headings, four diagrams, localization, image integrity, mobile/desktop
and light/dark rendering; full website validation follows the shared component change.

T062: use the built-in Aside caution for the authenticity examples; remove the flow
chart and photo imports/rendering in both locales. Remove the now-unused flow option
and styles from LL_OrgChart. Rebuild and check the callout, absent photo/links, and
three preserved organization diagrams at mobile/desktop sizes.

T063: edit both character-guide locales only. Remove premature group-prefab paths
and implementation captions; explain roles, direct ODA subordination and their
later use for character templates. Keep the three diagrams and existing advanced
Bohemia faction-guide link. Build and check bilingual rendered text and diagrams.
Analysis: operator corrections resolve the wording and scope; verified military
and example sources remain in research.md. No component or asset changes required.

T064: append matching create-base-prefab sections in both locales; import original
23/24 through LL_Screenshot. Follow the selected Character_US_Base.et shown in 23
and the exact filename shown in 24. Explain inherited settings in beginner language.
Build and check localized headings, image loading/source hashes and mobile/desktop
overflow. No actual .et creation or editing by the agent.

T065: append bilingual outfit instructions using LL_Screenshot and existing assets
25–30. Explain one drag-to-Prefab operation with Hat, then Jacket and a compact
folder/resource table for Pants, Boots and Back. Save with Ctrl+S in prefab edit
mode. Confirm folder workflow separately; build and inspect both locales at
390/1440 px, including exact original-image hashes. Do not alter addon assets.

T066: append radio-setup, initial-inventory and wristwatch sections in both
character guides using original 31–37 via LL_Screenshot. Use Duplicate to for the
radio and existing Edit Prefab instructions. Describe second-entry removal with
hovering over the second RadioTransceiver and pressing Delete, as confirmed by the
operator; retain one transceiver. Explain the mission's single-channel choice.
Add twelve inventory rows through
the plus control, with seven distinct prefab names and quantities totaling twelve.
Explain source addon selection for the custom radio versus vanilla items. Add
watch via the nested equipment storage component. Build and check both locales,
image hashes, anchors and responsive layout; no engine asset edits.

T067: add matching prefab-operations sections after wristwatch and before oda-base, linked from
create-base-prefab. Use three
short command definitions, an unchanged-hat example, and a concise inheritance
caution. Remove the duplicated inheritance definition from the next paragraph.
Build and check section order, locale parity and responsive presentation.

T067 revision: replace the dense definition list with three short subsections and
reuse LL_OrgChart for the inheritance example. Place the reference before oda-base and
retain its existing anchor; verify diagram structure and mobile/desktop rendering.

T068: insert oda-base, armor-and-webbing and primary-weapon sections in both locales
after prefab-operations. Import originals 38–42 via LL_Screenshot. Describe the
local Inherit action, existing slots ArmoredVest/Vest and their exact resource paths.
Identify the weapon slot by Weapon Slot Type primary / Weapon Slot Index 0 rather
than relying on component order. Build and check anchors, screenshot hashes,
matching locales and mobile/desktop output. No prefab edits by the agent.

T069: append grenade-slots and oda-ammunition sections, with short subsections for
inventory setup, flashlight, magazine pouches and reserve ammunition. Use 43–49
through LL_Screenshot; retain full storage identifiers where needed to distinguish
MagPouch/MagPouch2/Buttpack. Group repeated ammo filenames into a small key so the
instructions remain readable. Confirm flashlight placement before its prose.
Build and check bilingual routes/anchors, screenshots, quantities and overflow.

T070: append comms-sergeant with backpack-radio and comms-uniform subsections.
Reuse screenshots 52–56 and existing components. Retain current ODA/base names.
Duplicate Radio_ANPRC77.et to TSS_Radio_ANPRC77; hover the second transceiver and
Delete, then assign to Back on the sergeant only. Show hat/jacket/pants choices
seen in the images and confirmed in the example. Update pants Target Storage;
confirm clearing of the obsolete backpack entry. Build and check both locales,
images and responsive layout. No engine asset edits.

T071: append comms-role, character-localization, localization-entry,
register-localization and localized-character-name in both locales. Reuse images
57–65 and LL_Screenshot. Follow photographed String Editor File/New workflow;
include English/Russian runtime registration fields from official documentation
and the pinned addon.gproj. Preserve current naming and do not change icon settings
shown unchanged in the screenshots. Build and check anchors, new images/original
hashes and responsive layout. All instructions are documentation, no addon edits.

T071 hierarchy revision: promote character-localization to h2, titled localized
character names; add an h3 for creating the table. Keep the next three h3 sections
under localization. Verify the generated heading levels and rebuild the preview.

T073: append detachment-commander with commander-launcher, commander-binoculars
and commander-name subsections in both locales. Reuse original screenshots 66–71.
Identify launcher slot by primary/index 1; assign binoculars in the existing
equipment storage BinocularSlot. Link to the existing localization instructions
and reuse the table, rather than repeating its setup. Build and check responsive
output, original image hashes and matching anchors. No engine asset changes.

T074: append character-catalog with override-character-catalog and catalog-entries
subsections, using originals 72–75. Follow Override in → mission addon and duplicate
Character_US_SF_Sapper_S.et catalog entry, then change Entity Prefab. Reuse that
entry for the second completed role. Keep Entity Data List from the working entry.
Explain screenshot roster differences. Build and check both locales, image hashes,
links and responsive layout. Runtime checking/images 76 onward remain next slice.

T075: append test-characters with a short launch/browser/placement sequence using
originals 76–78. Link to mission-world setup for standalone readers. Confirm how
Game Master opens in the operator's mission; Tab and left-click placement are
supported by the official Game Master page and screenshot controls. Explain that
the larger roster shown is the completed example. Build and verify bilingual
anchors, images and responsive output. Group prefabs remain the next slice.

T076: append group-prefab with create-group-prefab, group-name and group-symbol
subsections using originals 79–84. Reuse folder creation and localization workflows.
Use SCR_EditableGroupComponent, UI Info Name, GROUPSIZE_LARGE, Use UI Info Name,
and BLUFOR/LAND/RECON as photographed and confirmed by the pinned Alpha prefab.
Keep roster filling separate until the missing specialist instructions are agreed.
Build and verify both locales, image hashes and responsive presentation.

T076 scope update: operator confirmed a brief by-analogy statement for the other
roles. Add group-members and group-formation using originals 85–86, six exact
prefab names from the pinned Alpha file, and AIFormationComponent/line. No empty
or temporary roster, no extra specialist sections. Registration follows later.

T077: append group-catalog with register-group and test-group subsections, using
originals 87–90. Override Groups_EntityCatalog_US.conf in the mission addon,
duplicate Group_US_EngineerTeam.et entry and assign Alpha. Preserve copied entry
data. Reuse Play/U/Y/Tab instructions by link; select US/Group, search localized
name and place once to verify six characters. Return link goes to the existing
main-guide character-prefabs anchor. Build and check both locales and assets.

T078: append map-markings to both mission pages, with map-line, draw-map-line
and port-marking subsections and originals 91–95. Use LL_MapZoneComponent as the
source for appearance/visibility, official Vector Tool documentation for point
editing, and screenshots for concrete settings. Update character return anchors.
Build, then check headings, links, original image hashes and mobile overflow.

T079: finish both create-mission pages in one pass, using originals 96–113 and
existing screenshot/Aside components. Group content into capture, other mission
conditions, briefing, playable forces, vehicles, freeze zones and final testing.
Explain ordinary AI versus playable slots, count thresholds rather than casualties,
AND conditions/reset, zero-based callsign indices, and notice-only mission triggers.
Use screenshot values where shown; avoid inventing absent specialist procedures.
Run full site validation plus focused responsive/image/anchor checks; rebuild the
normal preview after fixture validation. No game files, deployment or history edits.

Historical T080–T083 design (first superseded by T087–T090; current structure is T097):
this replaced the earlier single-page design. Keep hub URLs, rename the
character hub Characters and groups. Mission chapters: project, dependencies,
world, game-mode, scenario, map-markings, capture, conditions, briefing, forces,
vehicles, finishing. Visit character/group preparation after scenario, return to
map-markings. Character hub: planning, prefab-operations reference, characters and
groups subhubs. Character chapters: base, outfit, equipment, oda, inventory,
radioman, localization, commander, catalog. Group chapters: setup, roster, catalog.
Reuse MDX sections, preserve figures, prune unused imports. Add concise context and
native explicit prev/next per chapter; collapsed sidebar reveals current branch.
Small shared chapter metadata drives sidebar and validation. Hub-only old-hash
forwarding uses a static map. Rewrite internal links directly to new destinations.
Extend existing validation for conservation, chapters, locale and real navigation.
Full validation and mobile/desktop review; no new dependencies or game changes.
Place native chapter pagination before the Discord help panel so the reading
sequence follows the article. Suppress the native footer's empty metadata spacer
when it has no edit link or update date; retain populated native metadata.

T084–T086: establish one Russian editorial standard, then read/edit all 33 Russian
MDX pages and shared Russian site copy. Use Russian technical-book excerpts as
style references without copying their prose. Prefer complete sentences, explicit
objects of actions, familiar terminology and short connected paragraphs. Keep
precise Workbench strings and technical facts. Parallel edits own disjoint mission,
character and group/reference sets; overview/shared text and final consistency
review remain with the primary editor. Audit source invariants against a temporary
pre-edit snapshot, run existing validation, and rebuild the preview. No new tests,
features, dependencies or addon changes are required for this prose-only revision.
