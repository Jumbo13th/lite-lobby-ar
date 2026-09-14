# Research: Lite Lobby Guide Site

## Opening existing missions (T106)

The operator requested one Workshop and one GitHub example on the same supporting
page, and selected [Triad Danse Macabre](https://reforger.armaplatform.com/workshop/69F707F1824808A8-TriadDanseMacabre).
The supplied ReadyMission/06.png confirms the TriadDanseMacabre project and its
Worlds folder. Describe browsing that folder without inventing a particular world
filename. ReadyMission/07–13 document the GitHub route and come first, as requested;
01–04 and 06 document Workshop. The missing 05.png is not required for those steps.
Capture 13 shows Missing Addon Dependencies for 6A4545CCB3516E86: refer readers to
the existing recovery section rather than duplicating it. Capture 01 shows the mod
already installed, so its caption must not describe a download button.
GitHub's [archive instructions](https://docs.github.com/en/repositories/working-with-files/using-files/downloading-source-code-archives)
confirm Code → Download ZIP and the distinction between a branch snapshot and a
clone with history. Keep the established pinned course archive as an alternative.
Bohemia's [project setup guide](https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup)
confirms Add Existing Project and Scan for Projects, followed by opening a listed mod.

## Thematic regrouping (T087–T090)

The content audit found settings split by the character being demonstrated rather
than by reader task. Regroup character sections into eight topics, preserving all
54 illustrations. Mission settings become 15 topics with all 50 illustrations;
group creation remains three task-based chapters with 12 illustrations. Keep
continuous wizard steps together. Topic navigation and the beginner route serve
different entry points and use the same content.

Pre-implementation analysis covered the new requirement, plan, tasks, chapter
contract and content model: content regrouping is T088; metadata, overviews and old
links are T089; conservation, standalone entry and browser checks are T090. No
engine behavior changes or new dependencies are required. The prior eight-figure
limit was adjusted to nine to avoid splitting coherent equipment and translation
topics. No unresolved author decisions or constitution conflicts remain.

## SDD integration

**Decision**: Codex reads the existing .claude/skills/speckit-* instructions directly; AGENTS.md records the entry point.
**Rationale**: The operator selected one shared skill source after trying the additive Codex installation. Uninstalling Codex restored the integration state exactly; Claude remains default.
**Alternative**: Separate generated .agents skills provide automatic discovery but duplicate this project's skill instructions.

## Website runtime

**Decision**: Astro and Starlight with an npm lockfile. Operational pins live in [package.json](../../docs/package.json) and [.nvmrc](../../docs/.nvmrc); the table below records the compatibility audit.
**Rationale**: The operator requested the freshest stable compatible toolchain. Live registry/release checks on 2026-09-12 confirmed the site dependencies are already current. Node 26.8.2 is the latest Current release; npm 12.0.2 requires Node 26 or newer. TypeScript 7.0.2 is newer, but the latest Astro Check supports only TypeScript 5/6, so 6.0.3 is the newest compatible version. No forced peer overrides are used.
**Alternative**: A custom site adds navigation/search maintenance; Jekyll needs more assembly for bilingual illustrated guides.
**Sources**: [Starlight manual setup](https://starlight.astro.build/manual-setup/), [Astro installation](https://docs.astro.build/en/install-and-setup/).

| Package | Stable compatible version |
| --- | --- |
| Node.js / npm | 26.8.2 / 12.0.2 |
| Astro / Starlight | 7.3.2 / 0.42.0 |
| Markdown Remark / custom heading IDs | 7.3.1 / 2.0.0 |
| Astro Check / TypeScript | 0.9.10 / 6.0.3 |
| Cheerio / Playwright | 1.2.0 / 1.63.0 |

Locked rendering dependencies are also current: MDX 8.0.1, Expressive Code 0.44.2,
Pagefind 1.5.2, Vite 8.3.0, and Sharp 0.35.4. Styling is native Starlight CSS plus
project CSS, so no separate styling framework is needed. Node bundles npm 11.19.1;
install npm 12.0.2 separately in local/CI environments.
**Version sources**: [Node release](https://nodejs.org/en/blog/release/v26.8.2), [npm stable metadata](https://registry.npmjs.org/npm/latest), and each package's official npm registry metadata.

**Installation finding**: npm 12 blocks dependency install scripts by default. Permit
the pinned esbuild installation script through package.json's allowScripts field so
clean local/CI installs can prepare its platform binary. Do not enable scripts globally.
See [npm install-scripts](https://docs.npmjs.com/cli/v12/commands/npm-install-scripts/).

## Localization and navigation

**Decision**: Root English locale and ru locale, with six homepage guide entries and two API audience headings retaining real anchor destinations. Article pages retain translated sidebar and contents navigation.
**Rationale**: This previews the structure without unwritten pages. Starlight supplies translated interface controls and Pagefind search.
**Alternative**: Empty guide pages would mislead readers and require replacement during content work.
**Implementation finding**: Raw MDX HTML headings do not enter Starlight's contents list. Article pages use Markdown headings with remark-custom-heading-id for stable anchors and contents navigation. Homepage HTML anchors belong to LL_GuideHome.astro, which has no contents panel.
**Sources**: [Localization](https://starlight.astro.build/guides/i18n/), [Site search](https://starlight.astro.build/guides/site-search/).

## Project license

**Decision**: The operator identifies Lite Lobby's license as APL. The homepage links
the full source and [official APL terms](https://www.bohemia.net/en/licenses/arma-public-license),
summarizing reuse, modification, and sharing for noncommercial Arma use with attribution.
The [official license list](https://www.bohemia.net/en/licenses) does not rank APL as
Bohemia's most permissive license. No LICENSE file was found in this checkout; this
website description records the operator's declaration without changing repository licensing files.

## Colors

**Decision**: Reuse the palette from the [Triad Tactics website](https://triad-tactics.com/ru): #0a0a0a background, #171717 raised surfaces, neutral gray borders/text, and #d2b853 gold. Keep Starlight's layout and controls. Use #806b1d gold in light mode.
**Rationale**: The operator requested the same colors with a simple presentation. The darker light-mode accent provides approximately 4.98:1 contrast against #fafafa; the original gold provides approximately 10.11:1 against #0a0a0a.
**Verification**: Inspected the live site's stylesheet and the existing sibling website's src/app/globals.css on 2026-09-12. No sibling website files are changed.
**Alternative**: Recreating the marketing site's layout would add complexity beyond this guide foundation.

## Homepage and article design

**Decision**: Use Starlight's splash template and native hero on both homepages. Render six aligned rows in LL_GuideHome.astro. LL_HomeHero.astro controls typography; LL_Header.astro adds a compact native popover menu for homepage preferences below 50rem. Use custom CSS tokens and supported Expressive Code style overrides for the article design.
**Rationale**: Installed Starlight 0.42.0 removes both sidebars for splash pages. Its header hides theme/language controls below 50rem and normally places them in the sidebar, so the homepage needs its own menu. Compose native Header, MobileMenuToggle, and MobileMenuFooter instead of duplicating controls. One list accommodates six topics without an empty grid cell; semantic headings preserve their anchors.
**Alternatives**: A custom site layout would duplicate search/navigation behavior. A separate preferences row moves controls out of the navbar. Empty article pages and decorative screenshot assignments remain outside this phase.
**Spacing finding**: The empty native footer contributes 72px from its metadata margin and child gap, plus 24px sibling spacing. Main padding adds 3vh after the content panel's 24px inset. At a 960px viewport height the total trailing gap was about 149px. Conditional footer rendering and removal of the main padding leave 24px.
**Sources**: [Starlight frontmatter](https://starlight.astro.build/reference/frontmatter/), [component overrides](https://starlight.astro.build/guides/overriding-components/); verified against installed Page.astro, Hero.astro, Header.astro, and Card.astro.

## Screenshots

**Decision**: Move the original 113 PNGs into docs/src/assets/guide; record their hashes in a test manifest. Use a constrained Astro Image figure and original-resolution link. Import only images used by a page.
**Rationale**: Optimized variants reduce transfer size while originals preserve small UI text. Temporary validation pages exercise image handling without becoming guide content.
**Alternative**: A public asset folder publishes the whole unused library; eager image imports have the same drawback.
**Source**: [Astro image API](https://docs.astro.build/en/reference/modules/astro-assets/).

## Mission guide evidence

**Reference**: [Shooting Star at 0288431](https://github.com/Jumbo13th/triad-shooting-star-ar/tree/0288431eae2c423e31baaba295176a28be1ef873), verified 2026-09-12. Its addon project, Worlds/Eden/ShootingStar_Ep1.ent, Missions/ShootingStar_Ep1.conf, and world layers establish the reference mission. All 113 supplied screenshots were inspected; the proposed chapter mapping lives in plan.md.

**Prerequisites**: Official [project setup](https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup) requires the game, Workbench, and visible game data; [Arma Reforger Tools](https://community.bistudio.com/wiki/Category%3AArma_Reforger/Modding/Official_Tools) is the Steam tools package. Lite Lobby's addon.gproj depends only on the base game. The full example adds other mods; its dependency 69FF2C2B633BBC25 needs identification before writing the example-opening guide.

**Checks before detailed instructions**:

- The setup screenshots leave AI navigation configuration outstanding. Use the appropriate terrain's source settings and verify the Workbench step.
- Prefab screenshots include evolving names, unfinished labels/catalog previews, and radio edits. Verify final values and distinguish mission-specific choices before writing the supporting guide.
- Screenshot 100 shows a 2:1 capture ratio; the example source uses 3:1. Confirm which recipe the guide should teach when drafting objectives.
- The briefing describes five minutes of preparation plus 80 active minutes, but the world inherits a two-minute freeze from current Lite Lobby. Confirm intended timing and the round-ending procedure: trigger firing records/notifies an event rather than ending the round automatically.

## GitHub Desktop guide

T105 uses the eleven operator-supplied screenshots in `docs/src/assets/guide/Git`
without editing them. The naming convention is project guidance, not a GitHub naming
restriction. Screenshots 6–8 already include DemoWorld; the text explains that a bare
addon is enough for the reader's first commit. Screenshots 10–11 undo the commit that
introduced the project, so warn about its file deletions before presenting the steps.
The operator's new request explicitly adds AI-help advice to this supporting guide.

Rechecked official GitHub Desktop instructions for [creation and publication](https://docs.github.com/en/desktop/overview/creating-your-first-repository-using-github-desktop),
[reviewing and committing changes](https://docs.github.com/en/desktop/making-changes-in-a-branch/committing-and-reviewing-changes-to-your-project-in-github-desktop),
[ignore rules](https://docs.github.com/en/get-started/git-basics/ignoring-files), and
[reverting a commit](https://docs.github.com/en/desktop/managing-commits/reverting-a-commit-in-github-desktop).
They confirm the screenshot workflow: commits remain local until pushed; reverting
records an inverse commit rather than restoring an entire historical snapshot.
Pre-implementation review: the existing screenshot-folder assertion must admit the
new Git subfolder; Git figures must be checked separately from the unchanged course.
No unresolved input, engine changes, new packages or publication actions are needed.

**Decision**: Add an optional beginner supporting guide focused on saved mission files,
local checkpoints, remote backup, and recovery, referenced through the preface in the main
guide. Use a fresh demo mission project for capture
and verification; the existing Desktop installation does not need resetting.

**Evidence**: GitHub's [first repository guide](https://docs.github.com/en/desktop/overview/creating-your-first-repository-using-github-desktop) distinguishes local commits from published/pushed commits and explains that the repository name is appended to its parent path. The [revert workflow](https://docs.github.com/en/desktop/managing-commits/reverting-a-commit-in-github-desktop) reverses a selected commit with a new commit. Teach verification on GitHub after pushing and demonstrate reverting one recent bad change, not arbitrary rollback.

**Arma files**: Keep project source, world layers, assets, and their metadata together.
Bohemia's [metadata documentation](https://community.bistudio.com/wiki/Arma_Reforger%3AWorkbench_Metadata) distinguishes resource GUID/import metadata from the regenerated resourceDatabase.rdb. Verify a narrow ignore list before teaching it; do not ignore all .meta files. Recommend closing Workbench before pulls or recovery and reopening to verify the result.

**Screenshots**: Plan four captures in a separate demo project: source folder selection,
Changes/commit, Push origin, and History/revert. Use official illustrated setup links
for installation and authentication. The existing 113 screenshots are unaffected.

## Publication

**Decision**: Validate on relevant pushes and pull requests; a manual publish input, main-branch guard, and successful validation are all required for deployment. Pages stays inactive in this feature.
**Rationale**: The operator wants a deployable foundation without public launch. Standard public-repository runners and Pages avoid recurring hosting charges within published allowances.
**Alternative**: Automatic deployment on main would violate the agreed release boundary.
**Sources**: [Astro Pages deployment](https://docs.astro.build/en/guides/deploy/github/), [Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits).

### Russian editorial references (T045)

Read the Russian edition of [Pro Git, About Version Control](https://git-scm.com/book/ru/v2/Введение-О-системе-контроля-версий)
and A. V. Lyamin / E. N. Cherepovskaya, [Object-Oriented Programming: Computer Practicum, ITMO, 2017](https://books.ifmo.ru/file/pdf/2256.pdf)
(opening exercises). Use these as editorial references without copying their prose.
Our editorial synthesis: introduce a term through its purpose, explain a concrete
operation and its result, and use short connected paragraphs. Avoid promotional
imperatives, awkward literal translations, and decorative introductory callouts.
Git recovery applies to recorded versions; a GitHub copy requires an explicit push.
The operator's correction supersedes the earlier optional-tip presentation and
singular-author attribution. The supplied folder screenshot belongs in main-guide
project creation, not the Git supporting guide.

SDD analysis before implementation: FR-015/FR-017, the site contract, plan, and T045
agree on preface placement, optional workflows, authors plural, screenshot ownership,
and bilingual parity. No new feature, framework changes, or addon changes are needed.
Detailed Git/Desktop and project-creation procedures remain open for joint review.
T046 analysis: the request clarifies that opening originals must preserve the guide
and that the existing guide screenshots should illustrate the current steps. Spec,
plan, contract, and task agree on standard new-tab anchors and bilingual use of 01/02.
Both images were visually inspected: 01 shows Add Project → Create New Project; 02
shows Project Name, Location, and Create. Its Location shows the repository root;
T047 corrects the earlier interpretation: Workbench creates the addon subfolder automatically.
No image modification, new viewer package, or dependency-setup claims are needed.

T047 analysis: operator-confirmed Workbench behavior resolves the mistaken inference
from the repository screenshot. 02.png correctly specifies the parent directory;
Workbench creates the project-named subfolder. Spec, plan, and contract now require
instructions before the result. The new naming tip is explicitly requested and does
not change the ordinary-prose example/Git preface. Naming formats are editorial
conventions, not newly asserted engine constraints. Existing anchors remain stable.

T048 analysis: the operator explicitly defines readers as confident computer users
without Arma Reforger or developer knowledge. This supersedes the dense T047 naming
callout. The current spec, plan, and site contract agree: two plain principle sentences
in the preface, one example at Project Name, and definitions only where needed.
Technical-book references remain editorial aids, not a reason to use academic jargon.
The verified Workbench behavior and screenshot sequence are unchanged. Acceptance
checks can verify placement and rendering; the operator reviews clarity of the prose.

T049 analysis: the latest request replaces T048's general naming paragraph with a
learning tip. The spec, plan, and contract agree on placement, concise narrative,
and beginner vocabulary. The guide is collaboratively drafted with AI, so a literal
claim that AI did not write it would be inaccurate; credit the authors' experience
and state AI assistance. Project Name advice and verified Workbench steps are retained.

T050 analysis: official indexed Bohemia documentation distinguishes editor-entity
naming conventions, scripting conventions, and allowed project-name characters:
- https://community.bistudio.com/wiki/Arma_Reforger%3AEditor_Entity_Naming_Conventions
- https://community.bistudio.com/wiki/Arma_Reforger%3AScripting%3A_Conventions
- https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup
The project setup excerpt allows spaces, dashes, and underscores. Direct article opens
returned 403, so do not assert finer rules beyond the indexed excerpts. The reminder
can emphasize following conventions for each kind of item, while the Desert Storm
format remains the guide's project-name example, not a universal engine requirement.
Spec, plan, contract, and T050 agree on preserving placement and keeping this concise.

T051 analysis: current MDX pages, homepage introduction, footer, and navigation labels
were reviewed. Repetition occurs in the mission preface, learning/naming tips, folder
result explanation, Git introduction, and example introduction. Shared homepage/legal
and help copy already carries distinct information and remains intact. The spec, plan,
and contract preserve beginner context and existing steps while asking AI-help readers
to include this guide's page URL. This is an editorial pass, not completion of the
unfinished supporting guides or authorization to publish the site.

T052 sources and analysis:
- Supplied 03.png: File → Project Settings. 04.png: Dependencies +, new-row browse,
  game-project file filter, Workshop LiteLobby_699319AAA5BFC57F folder, and OK.
- Lite Lobby/addon.gproj confirms GUID 699319AAA5BFC57F and sole dependency on
  Arma Reforger (58D0FB3206B6F859). Do not use the source filename as proof of the
  installed Workshop filename; refer to its .gproj file.
- Bohemia Resource Manager Options confirms plus/browse → select gproj → OK,
  dependencies loaded after restart, and required visibility in Launcher:
  https://community.bistudio.com/wiki/Arma_Reforger%3AResource_Manager%3A_Options
- Bohemia Mod Project Setup confirms Add Existing Project for registering dependencies:
  https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Project_Setup
- Supplied 05.png shows LiteLobby loaded in Resource Browser. It illustrates the
  following world-creation step and is not duplicated here.
Spec, plan, contract, and T052 agree on a single dependency-setup slice. No game files
are modified; no Workshop/profile directories are browsed. Full Workbench execution
remains the operator's verification; website build/browser checks verify presentation.

T053 analysis: the operator supplied the missing-dependency dialog (699319AAA5BFC57F),
Scan for Projects menu, Documents/My Games/ArmaReforger/addons folder picker, and a
Workshop search resolving that GUID to Lite Lobby. These verify the illustrated flow.
Previously checked official Mod Project Setup documentation confirms scan-based
registration and explains that installed dependencies must be discoverable by Launcher.
Only the current conversation's four latest user-image attachments are extracted;
no game installation or profile directories are accessed. Spec, plan, and contract
agree that this conditional troubleshooting replaces proactive Add Existing Project
advice. The new assets do not change the original 113-image inventory.

T053 attachment preservation (PNG originals):
| Asset | Dimensions | Bytes | SHA-256 |
| --- | --- | --- | --- |
| missing-addon-dependencies.png | 920x693 | 167172 | ef07c1b27dd54cbf060ef2e0896456be579c7d93ebace6fb9f00206c24378594 |
| scan-for-projects.png | 937x795 | 151525 | 8b569b310994c5445a8c4ba05a02b63460aaa14a1775b8a37b9741a84a3aabbd |
| workshop-addons-folder.png | 1186x645 | 68387 | df897cecf80faf91b6ed9eba7240377531e9801b6efd6afa4d05bce9bf322b28 |
| workshop-guid-search.png | 1317x1067 | 1395749 | 5f7c69f592eef5cdc0d9b857b05bbad9fe6b26515d587689de423d6b7e6da914 |

T054 pre-implementation analysis:
- 05: World Editor in Quick Launch and the example's Worlds/Eden folder.
- 06: Resource Browser at ArmaReforger/worlds/Eden with Eden.ent selected.
- 07/08: File → New World, Sub-scene (of current world), OK.
- 09: Save world as, project-local Worlds/Eden, filename ShootingStar_Ep1,
  .ent extension, and New Folder button.
- 10: separate Eden and ShootingStar_Ep1 entries in Hierarchy; editable default
  layer under ShootingStar_Ep1; Rename layer dialog with a_systems.
Official World Editor and Scenario Framework Setup Tutorial confirm world loading,
sub-scene creation, File → Save World / Ctrl+S, and own-addon world directories:
https://community.bistudio.com/wiki/Arma_Reforger%3AWorld_Editor
https://community.bistudio.com/wiki/Arma_Reforger%3AScenario_Framework_Setup_Tutorial
The latter also documents Set as active/double-click for layers, but does not establish
the Rename layer invocation. An operator question is pending; do not use Layout Editor
F2 documentation to infer World Editor behavior. Independent 05–09 work can proceed.

T054 operator reply: right-click the layer and select Rename. This resolves the only
pending editor interaction. Add the systems-layer subsection with original 10.png;
no unverified keyboard shortcut is included.

T056 source review: originals 11–18 show Game Mode Setup, GameMaster.conf selection,
world scan, missing entities, automatic creation, follow-up map/navigation reminders,
and mission header generation. Official in-repository SCR_GameModeSetupPlugin.c
confirms the complete button sequence including the final Close button.
GameModeSetupConfig.GenerateMissionHeader derives Missions/<world name>.conf and
opens it in Resource Manager. Bohemia's Scenario Framework Setup Tutorial confirms
double-clicking a layer makes it active; only this editor interaction is used from
that tutorial, not its ScenarioFramework.conf template or Skip choice:
https://community.bistudio.com/wiki/Arma_Reforger%3AScenario_Framework_Setup_Tutorial

T056 analysis: spec, plan, task and contract agree on the bounded wizard section.
FR-006, FR-014 and FR-015 are covered; no unresolved editor interactions or
constitution conflicts. Checklist has no unchecked items. Existing original-image
inventory is unchanged; no engine assets are edited or deployment requested.

T057 analysis: screenshot 19 and the example mission at 0288431 agree on World,
Name, Author, Description, Player Count 128 and disabled Save Types (m_eSaveTypes 0).
Official scripts/Game/Mission/SCR_MissionHeader.c defines these editable fields and
states that disabling all save types disables persistence. No server-capacity claim
is derived from Player Count. The user explicitly removes the wizard ending paragraph;
the next Ctrl+S applies to the newly edited config. Spec/plan/task cover one header
subsection with original 19.png and matching locale anchors; no unresolved questions.
Example: https://github.com/Jumbo13th/triad-shooting-star-ar/blob/0288431eae2c423e31baaba295176a28be1ef873/Triad%20Shooting%20Star/Missions/ShootingStar_Ep1.conf

T058 analysis: screenshots 20–21 show GameMode_Editor_Full and its children removed,
replaced by LL_GameMode_Lobby1, while MapEntity, PerceptionManager and SCR_AIWorld
remain siblings. Local Prefabs/MP/Modes/Editor/LL_GameMode_Lobby.et confirms the asset.
Official Prefabs Basics documents dragging a prefab into the World Editor viewport;
Game Master: Composition Configuration Tutorial distinguishes Delete (instance removal)
from Delete from prefab (asset modification). Only instance removal is instructed.
Sources:
https://community.bistudio.com/wiki/Arma_Reforger%3APrefabs_Basics
https://community.bistudio.com/wiki/Arma_Reforger%3AGame_Master%3A_Composition_Configuration_Tutorial
The existing active-layer procedure applies. Spec/plan/task agree on this two-image
slice; no unresolved interactions, addon modifications or NavMesh requirements.

T059 analysis: screenshot 22 and a_systems.layer in the example at 0288431 confirm
worlds/Eden/Eden.topo and UI/Textures/Map/worlds/EveronRasterized.edds. Bohemia's
2D Map Creation explains geometry (roads/buildings) and background (terrain/forests).
The operator confirms the highlighted compass/watch components normally exist and
must only be added when missing. Official Weapon Modding documents Add Component
and searching the component list. Use scene-instance properties, not Apply to prefab.
Sources:
https://community.bistudio.com/wiki/Arma_Reforger%3A2D_Map_Creation
https://community.bistudio.com/wiki/Arma_Reforger%3AWeapon_Modding
Spec/plan/task now agree; clarification resolved, no NavMesh or terrain-export work.

T060 analysis: operator requests the supporting-guide introduction, not the full
prefab procedure yet. Original 23 shows the vanilla US_Army path and Character_US_Rifleman.et;
24 demonstrates the TSS_ addon prefix. The prefix definition and US-only scope are
operator decisions. Official Faction Creation covers new characters/factions and
integration with Game Master/Conflict; link it as further advanced work:
https://community.bistudio.com/wiki/Arma_Reforger:Faction_Creation
Spec, plan, task and contract agree on two new locale routes and four actionable
homepage entries. No missing engine interaction, new feature or asset modification.

T061 pre-implementation analysis:
- Russian term: организационно-штатная структура (ОШС), also used by military
  simulation developer RusBITech: https://rusbitech.ru/products/sppr/imod/
- AK-103 belongs to the 1990s AK-100 family; it postdates the 1979–1989 Soviet-Afghan
  war. US Army ODIN: https://odin.t2com.army.mil/WEG/Asset/fa7477a642d5936bcf0c9a4c2aa1886a
- USMC 2003 uniform publication documents desert/woodland patterns, not a blanket
  MultiCam uniform for regular USMC infantry. Avoid claims excluding all special-unit
  exceptions across every Iraq deployment: https://www.marines.mil/Portals/1/Publications/NAVMC%202932.pdf
- SOF Reference Manual (primary manual mirrored by FAS) documents SF company HQ and
  six ODAs: https://irp.fas.org/agency/dod/socom/sof-ref-2-1/SOFREF_Ch3.htm
- US Army describes 12-person ODA and leadership/specialists:
  https://www.army.mil/article/112428/what_a_legion_detachment_is_made_of
- Example at 0288431: CompanyHQ prefab has CompanyCommander and CompanyXO.
  SplitTeamAlpha has DetachmentCommander, OperationsSergeant, WeaponsSergeant_M249,
  EngineerSergeant, MedicalSergeant, CommsSergeant. SplitTeamBravo has
  AsstDetachmentCommander, IntelSergeant, WeaponsSergeant_M14, EngineerSergeant,
  MedicalSergeant, CommsSergeant. Each base prefab has six slots. US.layer overrides
  AB_S9 to eight slots, so diagrams must describe base organization, not claim every
  deployed group has six people or assign invented ODA numbers to world groups.
- The supplied photo has no verified date/unit. Preserve it as a user-supplied visual
  reference, not historical proof of the full organization or a specific year.
  assets/mission/green-berets-reference.png: 916909 bytes,
  SHA-256 9971f0b53d213b65b4b33a0fa2ff98895d8158c0ec0bc5d1a3413695040c9848.
Spec/plan/T061 cover requested content and static accessible diagrams; no added
runtime dependency or engine modifications. No unresolved source conflict.

T064 pre-implementation analysis:
- Screenshot 23 selects Character_US_Base.et (the earlier T060 note mentioning
  Rifleman described the filename example, not the selected inheritance source).
  Context menu: Inherit in → TriadShootingStar, from World Editor Resource Browser.
- Screenshot 24 shows TSS_Character_US_GB_BaseLoadout in Create inherited file.
- Bohemia Data Modding Basics confirms inheritance creates a child resource with
  parent attributes in the selected addon, retaining the original folder structure:
  https://community.bistudio.com/wiki/Arma_Reforger%3AData_Modding_Basics
- Screenshot 25 confirms the resulting file in the addon's US_Army folder. Moving
  it into Green Berets and editing equipment are deferred to the next content slice.
Spec, plan and T064 agree on this bounded continuation; no unresolved interaction.

T065 analysis:
- Original 26 shows Resource Manager's Edit Prefab button; 27 shows the character
  selected in prefab edit mode, component search `load`, BaseLoadoutManagerComponent
  and Slots/Hat. 28–30 show drag-to-Prefab assignments for hat, jacket and backpack.
- Bohemia Faction Creation, Changing character outfit, confirms inherited slots
  and assignment through Prefab. Resource Manager documents Edit Prefab mode:
  https://community.bistudio.com/wiki/Arma_Reforger%3AFaction_Creation
  https://community.bistudio.com/wiki/Arma_Reforger%3AResource_Manager
- Pinned example 0288431, TSS_Character_US_GB_BaseLoadout.et, confirms Hat_Boonie_US_TigerStripe.et,
  Jacket_US_BDU_TigerStripe.et, Pants_US_BDU.et, CombatBoots_US_01.et and
  Backpack_ALICE_Medium_assembled.et. Use those exact filenames rather than infer
  pants from appearance. Radio and inventory configuration are outside this slice.
- Screenshot 25 shows the folder but not its creation/movement UI. Asked operator
  to confirm the Resource Browser workflow; outfit authoring can proceed separately.
- Operator confirmed: right-click to create the folder, then drag and drop the
  prefab into it. No remaining clarification for T065.

T066 pre-implementation analysis:
- 31/32 explicitly use Duplicate to and Duplicate File for TSS_Radio_ANPRC68.
  33 crosses out the second RadioTransceiver; 34 confirms one remains. The pinned
  radio prefab also has one transceiver. Its range is now 4000 while screenshots
  show 1300; do not add a range-tuning instruction or assert identical settings.
- Official Data Modding Basics confirms Duplicate retains copied data and parent
  inheritance in a new addon-local resource. Faction Creation documents initial
  items, Target Storage, Prefabs To Spawn and plus controls:
  https://community.bistudio.com/wiki/Arma_Reforger%3AData_Modding_Basics
  https://community.bistudio.com/wiki/Arma_Reforger%3AFaction_Creation
- 35 shows an intermediate 11 empty entries, 36 the final 12. Pinned base loadout
  confirms Pants/Pants_US_BDU.et: custom radio, compass, folded US map, four field
  dressings, two morphine injectors, two tourniquets and ALICE entrenching tool.
- 37 and base prefab confirm SCR_CharacterInventoryStorageComponent →
  SCR_EquipmentStorageComponent → InitialStorageSlots → WristwatchSlot with
  Prefabs/Items/Equipment/Watches/Watch_SandY184A.et. No map-component changes.
Spec/plan/T066 align on this slice. UI and resource evidence support the steps;
no radio-range decision is required because tuning is outside this slice.

T066 correction: operator specifies hovering over the second RadioTransceiver and
pressing Delete, replacing the inferred minus-button interaction. Public-content
search found the minus instruction only in the EN/RU radio paragraphs. Operator's
intent: prevent each soldier simultaneously using a unit channel and side-wide
channel. Later backpack-radio removal is for simplification; defer its prose until
that section is authored.

T067 analysis: Bohemia Data Modding Basics distinguishes a new child, a duplicate
with existing inheritance retained, and an override sharing original identity:
https://community.bistudio.com/wiki/Arma_Reforger%3AData_Modding_Basics
Weapon Suppressor Creation documents base changes propagating to child variants:
https://community.bistudio.com/wiki/Arma_Reforger%3AWeapon_Suppressor_Creation
Level Of Detail confirms inherited values can be changed for a particular child:
https://community.bistudio.com/wiki/Arma_Reforger%3ALevel_Of_Detail
Use common outfit examples; no claim that duplication severs all dependencies or
that overrides create isolated variants. Existing hat assignment already illustrates
using an unchanged vanilla prefab without duplication. No unresolved behavior.

T068 analysis: screenshots 38–42 show local Inherit, the older name
TSS_Character_US_ODA_Base, ArmoredVest/Vest slots, and the primary weapon slot
(type primary, index 0). Current pinned example 0288431 names the file
TSS_Character_US_GB_ODA_Base.et and inherits TSS_Character_US_GB_BaseLoadout.et.
Its resources match the screenshots: Prefabs/Characters/Vests/Vest_PASGT/Vest_PASGT.et,
Prefabs/Characters/Vests/Vest_ALICE/Variants/Vest_ALICE_rifleman.et and
Prefabs/Weapons/Rifles/M16/Variants/Rifle_M16A2_carbine_OliveGreen_Solid.et.
Use current naming with an explicit short screenshot note; no renaming workflow.
Bohemia Faction Creation describes inherited character variants and outfit/weapon
configuration: https://community.bistudio.com/wiki/Arma_Reforger%3AFaction_Creation
Image 42 supplies the exact Weapon Template/Type/Index UI labels. Base inheritance,
resources and task scope agree; ammo and grenade setup are not part of this slice.

T069 analysis: 43/44 and pinned ODA base agree on Grenade_M67.et for type grenade
(index 3 shown) and Smoke_ANM8HC.et for type throwable. Do not infer changes to
Enabled from the screenshot; only assign Weapon Template as highlighted.
Bohemia Faction Creation confirms assignment and inventory workflow:
https://community.bistudio.com/wiki/Arma_Reforger%3AFaction_Creation
45–49 show inherited pants plus five added inventory records. MagPouch contains
2 M855 magazines + 2 M67; MagPouch2 contains 2 M855 + 2 AN-M8; Buttpack and Back
each contain 3 M855 + 1 M856. Counts and resource paths agree with pinned ODA base.
Flashlight differs: screenshots use Jacket/Jacket_US_BDU_TigerStripe.et with
PURPOSE_DEPOSIT; current base uses Vest/Vest_ALICE_rifleman.et/Suspenders/
Vest_ALICE_suspenders_2.et with TargetPurpose 64. Asked operator which to teach.
Ammo and grenade prose can proceed independently of that choice.

T069 clarification: operator has no preference for flashlight location. Use the
photographed jacket/PURPOSE_DEPOSIT workflow for consistency and simplicity.
This is an intentional guide variant from the current mission's attached light.

T070 analysis: 50–51 only rename the ODA base to the name already used in the guide;
keep assets, omit unnecessary action. 52 inherits the base; 53 duplicates ANPRC77;
54 removes its second transceiver; 55 assigns custom radio to Back on CommsSergeant;
56 updates pants storage after changing trousers. Pinned CommsSergeant confirms
Hat_Patrol_US_01_TigerStripe.et, Jacket_US_BDU_rolledup_TigerStripe.et,
Pants_US_BDU_TigerStripe.et, Back/TSS_Radio_ANPRC77.et, and inherited backpack
PrefabsToSpawn cleared. Current radio has one transceiver, with range changed
since screenshot; no range-tuning instruction. User previously specified hover
and Delete and simple gameplay rationale for backpack radio.
Asked how to clear inherited inventory entries. As the guide stores the flashlight
in the jacket, its Target Storage must also follow the rolled-sleeve jacket.
Other optional appearance/weapon overrides and editor labels remain later content.

T070 clarification: operator confirms clearing inherited Prefabs To Spawn by
hovering each entry and pressing Delete. Apply only to the obsolete backpack record.

T071 analysis: screenshots 57–65 show SCR_EditableCharacterComponent UI Info,
ROLE_RADIOOPERATOR, String Editor quick launch, File/New, Language/tss_localization.st,
CustomStringTableItem, TSS-Characters_CommsSergeant, Target En Us, runtime build,
project registration and the final hash-prefixed Name. The pinned example's
addon.gproj registers StringTableSource and Languages/Code/StringTableRuntime;
its source table contains the same key and English text. Add Russian text as the
guide's bilingual variant (the example leaves translations empty).
Official Mod Localisation confirms new-file runtime creation, explicit runtime
language registration, hash usage, English fallback and rebuilding after edits:
https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Localisation
Official String Editor confirms inserting/selecting a row, saving and Ctrl+B:
https://community.bistudio.com/wiki/Arma_Reforger%3AString_Editor
Current File/Project Settings entry is already shown by original screenshot 03;
use this instead of the older Options wording in official docs. No icon change:
57/65 show Custom while the current prefab uses Radio_Scout. Scope is role/name,
not icon selection. Checklist gate has no unchecked items; no extension hooks.

T073 analysis: originals 66–71 show Inherit from ODA_Base, commander name, M72A3
in CharacterWeaponSlotComponent primary/index 1, M22 in nested equipment storage
BinocularSlot, translation insertion/build and ROLE_LEADER/Name assignment.
Pinned TSS_Character_US_GB_DetachmentCommander.et agrees on parent, launcher,
binoculars, label and localization key. Use those photographed changes; current
example additionally changes rifle optic and editor icon, outside this slice.
Resource paths: Prefabs/Weapons/Launchers/M72/Launcher_M72A3.et and
Prefabs/Items/Equipment/Binoculars/Binoculars_M22/Binoculars_M22.et.
Russian translation is the guide's addition, consistent with the previous role.
Existing localization workflow covers runtime building and table registration.
No unresolved operator actions; screenshot 72 begins a separate catalog task.

T074 analysis: 72 overrides Configs/EntityCatalog/US/Characters_EntityCatalog_US.conf
in the mission addon. 73 duplicates Character_US_SF_Sapper_S.et's catalog entry;
74 assigns Entity Prefab, and 75 shows the expanded finished roster. Pinned catalog
contains the two taught roles plus later specialists, each with EditorData modes 33
and SpawnerData GROUP_SMALL. Retain copied entry data; no new field values needed.
Local official SCR_EntityCatalog.c confirms m_aEntityEntryList; SCR_EntityCatalogEntry.c
confirms Entity Prefab, Enabled and Entity Data List. SCR_EntityCatalogEditorData.c
defines editor availability data. Online Asset Browser Mod Integration currently
describes the older PlaceableEntitiesRegistry approach; do not substitute that for
the operator's photographed/current catalog workflow. Source reviewed:
https://community.bistudio.com/wiki/Arma_Reforger%3AAsset_Browser_Mod_Integration
Catalog opening uses Resource Manager, no prefab-edit mode or agent asset edits.
Scope ends with saved catalog; runtime verification follows with images 76–78.

T075 analysis: 76 launches the mission world with the green Play control; 77 shows
Entity Browser with US filtering, translated names and placeholder previews; 78
shows placed characters and the Tab browser control. Official Game Master docs
confirm Tab, faction/type filters, left-click placement and right-click cancel:
https://community.bistudio.com/wiki/Arma_Reforger%3AGame_Master
The official Prop Creation tutorial confirms green Play from World Editor, but
uses GM_Eden rather than this mission's lobby mode. Asked operator whether their
Game Master opens automatically or requires an action; do not substitute another
world or invent a key. Character appearance checks can be described independently.

T075 clarification: operator confirms advancing the lobby with its Play button
until gameplay, then opening Game Master with Y. U toggles the lobby; Y toggles
Game Master. Include these controls before the Tab/search/placement instructions.

T076 analysis: originals 79–84 and pinned TSS_Group_US_GB_SplitTeamAlpha.et agree
on Group_US_Base parent, Prefabs/Groups/BLUFOR/Green Berets folder, localization
key TSS-Groups_SplitTeamAlpha, GROUPSIZE_LARGE, Use UI Info Name, BLUFOR and RECON.
Screenshot 84 also shows LAND. Local SCR_EditableGroupUIInfo.c confirms that
Use UI Info Name selects the explicit Name over the dynamically generated name.
Do not present GROUPSIZE_LARGE as a military echelon; it is an editor label.
Original 85 uses six specialists, four not taught yet. Asked operator whether to
teach those individually or summarize their setup by analogy. Foundation steps
79–84 do not depend on that choice; roster filling does. Russian display name is
an added translation, as in the preceding character examples.

T076 operator clarification: state only that the other specialists were created
by analogy. Proceed with 85–86: Unit Prefab Slots in order commander, operations,
weapons M249, engineer, medic, communications; Default Formation line. Both list
and formation match the pinned Alpha prefab. Screenshot 85 shows SCR_AIGroup →
Group Members → Unit Prefab Slots and the plus control; 86 shows the formation
component/search. No separate specialist walkthroughs required.

T077 analysis: 87 overrides Configs/EntityCatalog/US/Groups_EntityCatalog_US.conf;
88 duplicates Group_US_EngineerTeam.et's entry; 89 assigns CompanyHQ to Entity
Prefab; teach the same action using the already prepared Alpha. 90 shows all
three example groups in Entity Browser. Pinned catalog confirms CompanyHQ, Alpha
and Bravo with identical EditorData modes 33 and SpawnerData slot flags 7.
No changes to those copied settings needed. Placement/search uses the prior
operator-confirmed workflow and official Game Master controls already researched.
Screenshot 91 starts mission-map content, outside this character-guide slice.

### T078: map markings

Originals 91–95 show the overview, LL_MapPolyZone.et placement, Vector Tool,
RedLine naming and Port outline. RedLine uses red/10; Port uses green/7. Both
have Line Mode and Show For Any Faction enabled. The local
Lite Lobby/scripts/game/Zones/LL_MapZoneComponent.c confirms open-line mode
draws no fill and does not join ends, and only draws map UI. It imposes no
movement restriction or objective. The prefab is a PolylineShapeEntity with that
component; edit its existing shape instead of creating an unrelated New Polyline.

[Official Vector Tool documentation](https://community.bistudio.com/wiki/Arma_Reforger%3AWorld_Editor%3A_Vector_Tool)
confirms Ctrl + left click adds the next point and Snap to terrain grounds new
points. [Official setup tutorial](https://community.bistudio.com/wiki/Arma_Reforger%3AScenario_Framework_Setup_Tutorial)
confirms creating a layer from the subscene context menu and activating it by
double-click. Screenshot 94 shows the entity name field at the top right.
Screenshot 96 starts a separate trigger shape and is reserved for the next slice.
Checklist gate passed (17/17); no extension hooks. No new component or game-asset
changes are needed, and the continuation stays within the approved guide scope.

### T079: remaining mission walkthrough

Reviewed originals 96–113. Local trigger sources confirm a separate named shape,
all conditions required continuously, timer reset on failure, and one-way capture.
CriticalLoss counts surviving playable slots and arms only after exceeding the
threshold; bots in slots count too. MissionEndTimer counts after freeze ends.
Supremacy waits for both sides and compares living slots. Base Fire broadcasts
and records statistics; none of these triggers ends the round automatically.
Screenshots specify USSR loss 5, US loss 7 (104), timer 4800 s, USSR supremacy 4,
capture US >= USSR × 2 and US >= 1 for 120 s. Unshown reverse supremacy values
are designer choices, not a claimed exact reproduction.

LL_MissionDescription documents rich-text tags, entity links and faction visibility.
LL_PlayableComponent registers grouped characters as slots. LL_GroupCallsign uses
three zero-based faction-list indices, not literal military organisation. Images
107–109 specify HQ 0/0/0, Alpha 0/1/0, Bravo 0/1/1. LL_VehicleSquadLinkComponent
resolves a world group name; vehicle faction is set separately. Teach linking to
the already placed AB_S2; AB_S9 in 111 is a later example group.

Official in-repo SCR_EditorRestrictionZoneEntity and restriction manager confirm
warning/lethal radii; LL_GameModeCoop.OnFreezeTimeEnded_S removes registered
vanilla zones at the end of freeze. Image 113 uses E_EditorRestrictionZoneLarge.et.
Freeze Time and Hard Freeze Time are milliseconds, confirmed by attributes/code.
Do not describe a permanent combat boundary or confuse it with map lines.

[Official mod publishing process](https://community.bistudio.com/wiki/Arma_Reforger%3AMod_Publishing_Process)
confirms Workbench → Link, Publish Project, separate Working Dir and publish/update
workflow. [Official Capture & Hold setup](https://community.bistudio.com/wiki/Arma_Reforger%3ACapture_%26_Hold_Setup)
calls for multiplayer testing before Workshop release. Link to the detailed
publishing instructions instead of guessing unseen dialog controls.
Analysis: continuation follows approved scope, with original assets preserved,
no new dependencies and no engine mutations. Existing checklist remains complete.

### T080–T083: chapter audit and analysis

Read-only audit counted 50 mission and 66 prefab figures. Splitting these is more
useful than aggressive shortening. Preserve inventory storage updates, deletion
instructions, screenshot name/role differences, locale compilation and trigger
semantics. Replace cross-page "above/below" with links. Installed Starlight 0.42
supports translated collapsed sidebar groups and explicit prev/next even with
global pagination false. Links are literal; use locale-preserving relative URLs.
Existing footer renders native pagination. Static hosts cannot redirect fragments,
so use a hub-only script and static old-id map. Example page needs verified opening
instructions. Cross-artifact analysis: approved scope and title/order clarification
covered by T080–T083; no new engine behavior, package, addon edit or approval hook.

Implementation review: splitting preserved 116 exact screenshot components/import
targets, eight configuration tables, two code blocks and four organisation
diagrams in each locale. Intro-only headings became retained anchor spans; action
sections retained useful headings. The native footer contains an empty metadata
row with top margin even when only pagination is enabled. The site footer now
places native pagination before support and hides only that empty row.

The example-opening companion uses the existing pinned Triad Shooting Star commit
0288431eae2c423e31baaba295176a28be1ef873. Its addon.gproj lives inside Triad Shooting
Star, with Worlds/Eden/ShootingStar_Ep1.ent as the example world. A pinned GitHub
archive provides a reproducible starting point without requiring Git; cloning the
current repository remains an alternative for readers already using it.

Final analysis: all T080–T083 acceptance criteria satisfied. Full production and
isolated fixture validation passed; no blocking cross-artifact inconsistency or
change to engine behavior was found.

### T084–T086: Russian language revision

Style references: the authorized Russian excerpt of Tony Gaddis, Beginning Python
(BHV, 5th edition), https://bhv.ru/wp-content/uploads/wpallimport/files/pdfki/view_2839_978-5-9775-6803-6.pdf,
and the Russian Pro Git chapters at https://git-scm.com/book/ru/v2. These are style
references only, not sources for Arma behavior. Use connected explanation before
instructions, identify what a field or operation affects, and keep exact interface
names distinguishable from Russian prose. Avoid both telegraphic summaries and
formal filler such as “осуществите”, “данный”, or “в рамках”.

Pre-implementation analysis: the existing chapter structure and feature 002 cover
this correction. User authorization includes every Russian page; no clarification
or engine decision is needed. Disjoint editing ownership prevents conflicting
rewrites. Acceptance is editorial review plus preservation of procedures/assets
and existing production checks, not a sentence-length or word-count target.

### T099: Russian editorial review and mission links

Reviewed the available Russian excerpts, not entire books:

- Robert Martin, *Clean Architecture*, introduction and opening discussion of design:
  https://litres.com/book/robert-martin/chistaya-arhitektura-iskusstvo-razrabotki-programmnogo-obesp-39113892/read/
- Martin Fowler, *Refactoring*, Russian publisher's introduction:
  https://www.williamspublishing.com/PDF/978-5-9909445-1-0/intro.pdf
- Kernighan and Ritchie, *The C Programming Language*, Russian introduction:
  https://djvu.online/file/OZoSGIQQzgMhN
- *Pro Git*, Russian explanation of version control:
  https://git-scm.com/book/ru/v2/Введение-О-системе-контроля-версий

Editorial application: explain through the mission example; introduce a term where
the reader uses it; name the affected object and the result of an action. Retain
causes and troubleshooting, but remove repeated stage summaries and abstract
phrases. These references inform exposition only; no book passages were copied
and no Arma behavior was inferred from programming books.

Reviewed all 14 active Russian pages and the common text of 37 retired routes.
Revised 11 active pages; the approved course introduction, homepage and Git page
already follow the requested approach. Linked meaningful prose references to
Triad Shooting Star in both locales, leaving filenames, interface values and image
descriptions literal. The current repository link remains distinct from the pinned
download used to reproduce the tutorial. No chapter restructuring or game changes.

Compared Russian content with HEAD: headings, unique inline-code values and all
116 screenshot components are preserved, as are organization diagrams and retired
routes. Existing website validation supplies the build, link and rendering checks.
