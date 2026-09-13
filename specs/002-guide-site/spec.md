# Feature Specification: Lite Lobby Guide Site

**Feature Branch**: 002-guide-site
**Created**: 2026-09-12
**Status**: Design and tooling verified; mission-guide preparation approved, remaining sections under collaborative review
**Input**: Finish the bilingual Lite Lobby guide website and the agreed guide content within feature 002, using SDD. Complete the design and current tooling first; author guide content with the operator. Public launch remains a separate decision. T097 incorporates the former character/group guide into the mission course.

## Audience and writing requirements

The guide is for confident computer users who have no prior knowledge of Arma Reforger,
Workbench, modding, Git, or software development. Do not assume programming experience.
Explain an unfamiliar term briefly when it first becomes necessary. Show the action
before its result, and place examples beside the field or step they explain.
Use short, direct explanations: what to do and why it matters here. Avoid walls of
text, abstract terminology, repetitive advice, and examples of future steps.
The optional API sections can introduce programming concepts when needed; the main
mission walkthrough must remain usable without developer knowledge.

Acceptance (T098 editorial correction): the introduction promises recreating Triad
Shooting Star from opening the editor to Workshop publication and links its name
to the existing mission repository. Address readers new to Arma Reforger Tools;
programming is not required to create a mission. Recommend reading the connected
chapters in order and opening the example alongside them, with selective reading
for those who do not need the beginner explanations. Explain Git through restoring
working versions and saving time and effort. Remove the sidebar explanation,
authors' learning anecdote, GitHub-backup/optional sentences and AI-help paragraph.
This supersedes the earlier preface wording requirements in both locales.
Keep general naming advice out of the preface. One short project-name example
appears beside Project Name. Repository naming examples
belong in repository setup, not the mission preface. Explain addon and repository
in plain language on first use in the current steps. Apply the same rules in both locales.

## Current State

### One sequential course (T097; supersedes the fragmented chapter layout)

The operator selected 6–8 consecutive book-like chapters. The mission tutorial is
one course: eight numbered chapters, each ending at a meaningful work milestone.
Existing small pages become sections within those chapters. Do not imply that a
dependent worked example is a standalone reference task. Remove repeated article
introductions and intermediate overview stops; retain natural transitions and all
technical instructions, screenshots, and old links. Parent prefabs must be finished
before their children, and editor display settings remain separate from equipment.

The operator further clarified: everything belongs to «Как создать миссию», organized
as parts → chapters → sections. The left sidebar follows three parts (preparation,
characters, mission assembly/completion) and eight chapters; the right contents
lists sections of the current chapter. A single introduction owns the route.
Retire the standalone Characters and groups overview and its homepage row. Git, the
example mission, unit planning and prefab operations remain supporting material.
Reference documentation will be written separately later. There is no per-page
screenshot ceiling; this supersedes the earlier limit and mandatory overview hubs.
The homepage has five entries, seven topic/audience anchors and three working guide
links: mission creation, the example mission and Git. Character/group content remains
in the course; its former routes forward to the corresponding chapters or sections.

### Historical connected tasks and topic lookup (T087–T096; layout superseded by T097)

The decisions below record earlier implementations. Their page partitions, topic
indexes, sidebar categories and separate overview requirements are superseded by
T097. Their technical prerequisites and screenshot-integrity requirements remain.

T096 clarification: equipment tasks do not own editor display settings. Finish both
character variants before the shared translation-table chapter; configure UI Info
Name/Authored Labels for both in one chapter before catalog registration/testing.
Preserve parent-before-child dependencies, all screenshots and old anchor links.

T092–T095 correction supersedes the component-based character grouping: the reader
must finish a parent prefab before creating its children. A practical chapter owns
a complete task on a character; changing equipment includes correcting its storage
references. Screenshots must match the state reached by the lesson. Keep task-based
titles understandable without knowing the example, with a topic index linking into
the same canonical instructions. Do not duplicate a tutorial as reference prose.

The complete reading route prepares the game map before Game Master tests and
playable groups before testing triggers. State uncreated inputs honestly: extra
specialists and groups are prepared by analogy, not silently assumed to exist.
Preserve the Triggers category, bilingual content, all images and previous links.
Acceptance requires a manual dependency review in addition to link/count checks:
follow one worked object to its stated result without obligatory forward jumps.

T091 clarification: the operator groups capture, critical loss, time and numerical
superiority under Triggers. Name each chapter by its trigger type, with shared
settings and briefing display explained once. Keep the existing technical content,
illustrations and old section links in both languages.

Readers must find a task without knowing the example mission: radio setup, weapons,
inventory, character roles, capture areas and freeze time. Example names such as
Communications Sergeant, ODA Commander and Port Capture belong inside the chapters.
Preserve the Characters and groups overview with Characters and Groups beneath it.
Every task states its purpose and links to prerequisites. Retain illustrations,
technical explanations, mismatch notes and old chapter/anchor destinations.

Acceptance: a reader can follow the overview route without creating a child prefab
from an unfinished parent, or use the topic index to reach personal/backpack radio
settings, weapons and inventory directly. Capture and freeze-time chapters are
findable without knowing the example mission. Both locales, old links and screenshot
integrity pass the existing website checks.

### Shared website presentation

The bilingual website uses five aligned guide rows, shared article styling, centered
desktop search, and community links. Production validation and manual deployment
configuration are implemented. The shared SDD integration and 113 original PNG
screenshots are preserved. Cleanup is verified. Content work starts with the mission
guide's outline and prerequisites, followed by one reviewed section at a time within
feature 002. Publication requires the later launch decision.

## Clarifications

### Session 2026-09-12

Earlier answers are retained as a decision record. T097 above supersedes their
six-entry homepage and separate character/group navigation requirements.

- Q: Which site presentation? → A: Adapt Starlight with minimal custom styling.
- Q: Which languages? → A: English and Russian together.
- Q: Which API audiences? → A: Enforce scripting and website integration, separately.
- Q: What content belongs in this phase? → A: Design and tooling first; write the mission walkthrough and its supporting guides together afterward within this same feature.
- Q: What is the release boundary? → A: Ready to publish, with public launch deferred.
- Q: Which SDD skill source should Codex use? → A: Reuse .claude/skills directly and remove the generated Codex copies.
- Q: Which colors should the site use? → A: Use the Triad Tactics website's charcoal, neutral gray, and muted gold palette; keep Starlight's presentation simple.
- Q: Design or content next? → A: Refine the homepage and shared article design first, using temporary sample text and existing screenshots; author the mission-creation guide together afterward.
- Q: How should the homepage be organized? → A: Five guides in a centered layout, without the nearly empty sidebar or on-page contents panel. Replace the rejected two-column cards with one compact list of aligned rows.
- Q: Should this design pass create another feature? → A: No; update the SDD artifacts and implementation within 002-guide-site.
- Q: What does feature 002 cover? → A: The complete guide site and guide content, not only its foundation; rename it to 002-guide-site.
- Q: Which software versions should be used? → A: The freshest stable compatible packages and Node.js runtime; upgrade the local tools where practical and report any required user action.
- Q: Should this turn also draft the guides? → A: Finish design and upgrades now; write the guides together next.
- Q: What needs correction in the guide section? → A: The API section looks inconsistent; make its presentation consistent with the other guides instead of using a separate nested layout.
- Q: What guide-list text should remain? → A: Only the guide titles and API audience labels; remove slogans, decorative labels, prompts, descriptions, and preparation notices. The separately requested help panel sits outside this list.
- Q: How should the guides relate? → A: Mission creation is the main walkthrough. The character-prefab, Triad Tactics, example-mission, and API guides support it through links at the steps where they are needed.
- Q: How should content collaboration work? → A: Propose the outline and prerequisites from the example and screenshots first. Then draft one section at a time, have the operator verify its Workbench steps and terminology, and finalize both languages and site validation after review.
- Q: Where should readers get Lite Lobby for the main guide? → A: Install it from Workshop, matching the supplied screenshots.
- Q: How should beginners preserve their work? → A: Add a sixth guide about Git and Arma Reforger using GitHub Desktop, linked near project creation in the mission walkthrough. Cover saved files, local commits, remote backups, and recovery. Its screenshots still need preparation.
- Q: Are the example and Git required? → A: No. Both are optional recommendations explained in the main guide preface. Recommend opening an existing mission as the best way to learn and state that the authors learned this way. Use the exact Russian guide names recorded in contracts/site.md for headings and references.
- Q: Where can readers ask for help? → A: In #lite-lobby-issues on the supplied Discord server. Add a compact bilingual help panel with a new-tab Discord button and mention that the authors respond quickly.
- Q: How should Lite Lobby be introduced? → A: Credit the Triad Tactics collective, welcome the wider Arma community, link the full source, and identify the operator-confirmed APL license. Describe its verified reuse/modification permissions and conditions without an unsupported license ranking. Use the operator's corrected Russian help wording.
- Q: What is missing from the homepage list? → A: Add Git and Arma Reforger and use the six-guide order in contracts/site.md, with opening the example and Git directly after mission creation. Both remain optional recommendations in the main guide preface.
- Q: How should mobile controls and bottom spacing work? → A: Put language, theme, and community links in a navbar menu below 50rem, remove the separate homepage control rows, and remove empty footer space after the help panel.
- Q: How should guide authoring begin? → A: Add the approved preparation section to the mission guide and make its homepage entry clickable. Add links as real bilingual guide pages become available; continue reviewing the remaining sections together.
- Q: Where should guide links and chapter links appear? → A: Use the left sidebar to navigate between guide pages. Keep the current page's chapter links in the right-hand contents panel.
- Q: Which section should be narrower? → A: The right-hand “On this page” contents panel. Keep it compact beside the article, with page navigation on the left.
- Q: What comes before project creation? → A: An ordinary preface: learn by studying other authors' missions, as the Lite Lobby authors did, and recommend Git before starting. Explain recovery from bad changes using explicitly committed versions and a GitHub copy for computer or disk loss. Recommend a repository under ArmaReforgerWorkbench/addons containing a separate addon folder. Workbench's Location points to the parent (repository) folder; Workbench automatically creates the addon folder using Project Name; the main guide explains the layout at project creation using the supplied repository screenshot.
- Q: Where should the preface link while supporting guides are unfinished? → A: Create short bilingual introductory pages now and expand them together later.

## User Scenarios & Testing

### User Story 1 - Preview the bilingual guide home (Priority: P1)

A mission maker can preview the guide site's structure in English or Russian and
recognize the five guide entries without being sent to unwritten pages.

**Why this priority**: Establishes the reader experience before guide authoring.
**Independent Test**: Open both homepages at the intended project URL prefix on desktop and mobile.

**Acceptance Scenarios**:

1. Given the local site, when a reader opens either homepage, then the five guide titles appear in that language and in the order defined in contracts/site.md.
2. Given either language, when the reader switches language, then the corresponding homepage opens with translated site navigation.
3. Given the five guide entries, when a reader scans the guide list, then only the topic titles and two API audience labels are shown, and the three active guide links have real destinations.
4. Given the production preview, when the reader searches in either language, then results lead to existing content in the selected language.
5. Given any screen width, when the reader opens the homepage, then a centered list contains five aligned guide rows without an empty grid cell or clipped text.
6. Given either homepage below 50rem, when a reader opens the navbar menu, then theme, language, and community links are available in a compact popover. Search stays in the navbar. Keyboard activation, Escape, outside dismissal, and resizing to desktop work without leaving duplicate controls visible.
7. Given a homepage or guide article, when a reader needs help, then a localized panel identifies #lite-lobby-issues and the responsive authors, and its Discord button opens a new tab without leaving the guide. When no footer content follows, only a small bottom inset remains.

### User Story 2 - Prepare illustrated content (Priority: P2)

An author can reuse supplied screenshots in either language, preview readable images,
and expose full-resolution originals without duplicating source assets.

**Why this priority**: Future guides depend on screenshots with small editor text.
**Independent Test**: Render representative screenshots in temporary validation pages excluded from normal output.

**Acceptance Scenarios**:

1. Given a small dialog image, when displayed, then it is not enlarged beyond its natural width.
2. Given a large screenshot, when displayed on a phone, then it fits the page and its original can be opened.
3. Given translated captions and alternative text, when changing language, then the strings change while the original image bytes remain identical.
4. Given the supplied assets, when consolidated, then all 113 names and file hashes match the originals.
5. Given the temporary article preview, when reading headings, paragraphs, lists, a table, an aside, code, and figures, then the typography, spacing, surfaces, and accents form a consistent hierarchy in both themes and languages.
6. Given a temporary article, when using its navigation, then the standard article sidebar and contents controls work even though the homepage omits them.
7. Given an article page on a wide screen, when using the right-hand contents panel, then it stays compact beside the readable article column instead of expanding with the viewport. Mobile chapter navigation remains available.

### User Story 3 - Maintain and prepare publication (Priority: P2)

A maintainer can follow shared SDD rules with either agent, validate the site,
and prepare a later publication without making it public during this feature.

**Why this priority**: Preserves existing collaboration and a repeatable release path.
**Independent Test**: Check the SDD setup, run site validation, and inspect publication triggers.

**Acceptance Scenarios**:

1. Given the existing SDD setup, when Codex follows the same workflow, then Claude remains functional and the shared constitution and specifications remain intact.
2. Given a clean dependency installation, when validation runs, then it checks languages, links, assets, browser behavior, and output exclusions.
3. Given a pull request or push, when checks run, then they validate without deploying.
4. Given the publication workflow, when invoked outside main or without explicit publication selection, then it cannot deploy.

### User Story 4 - Follow complete guides (Priority: P2, content phase)

A mission maker or developer follows a complete guide, with screenshots and verified
examples, in either language. The operator and agent author and review the guide
content together after the design pass.

**Why this priority**: The site exists to help readers complete real mission-making tasks.
**Independent Test**: Follow each guide against the example mission and the relevant addon or integration source; verify equivalent English/Russian routes and instructions.

**Acceptance Scenarios**:

1. Given the agreed topics, when the content phase is complete, then each is covered in English and Russian with prerequisites, actionable instructions, and a verifiable result. Character and group creation belong to the eight-chapter mission course, not a separate walkthrough.
2. Given a screenshot or code example, when reviewing its associated step, then it illustrates that step and matches the real example mission or addon interface.
3. Given the API guide, when choosing an audience, then readers find distinct Enforce scripting and website-integration guidance based on the implemented interfaces.
4. Given complete guides, when using the guide list, sidebar, language switching, or search, then readers reach the corresponding real guide and the correct language.
5. Given a beginner's mission project, when following the GitHub Desktop guide, then the reader can create a local checkpoint, confirm it is on GitHub, and reverse a demonstrated bad change without rewriting published history.
6. Given the main mission guide, when the reader skips the optional example-opening and Git workflows, then the core instructions remain complete and no step requires either optional workflow.

### Edge Cases

- Direct URLs must work under the repository prefix, including Russian pages and search assets.
- Missing translations, broken image references, and local links to unwritten guides fail validation.
- A missing image or empty alternative text is an authoring error.
- Temporary verification pages and internal project files never enter normal site output.
- Failed builds cannot deploy. Guide steps are authored during the collaborative content phase, after the current design/tooling pass.
- Long Russian titles wrap naturally; unpublished entries do not imitate active links or buttons.
- Narrow homepages expose language and theme through their navbar menu even without article navigation. The menu closes when the viewport crosses to desktop.

## Requirements

### Functional Requirements

- **FR-001**: Make the existing SDD workflow usable by Codex while preserving Claude, its default selection, the constitution, existing specifications, and shared templates.
- **FR-002**: Provide project instructions and complete the approved SDD sequence before site implementation.
- **FR-003**: Provide English and Russian homepages with a language switcher and translated navigation.
- **FR-004**: Present the five localized guide titles in the order defined in contracts/site.md and the API's two audience labels, without guide-list slogans, descriptions, prompts, decorative text, or preparation notices. Retain their seven stable anchors and avoid links or button affordances for unwritten guide pages; mission creation, the example and Git are the three active entries. The help panel and project introduction specified in FR-018/FR-019 sit outside the guide list.
- **FR-005**: Provide responsive navigation, keyboard access, theme switching, code rendering, and local full-text search. Center the search field in the viewport on desktop headers (at least 64rem wide), retaining native compact controls below that width. Expose the supplied Discord, GitHub, and Telegram links as accessible icons on desktop and mobile; each opens a new tab while preserving the guide page. Use the Triad Tactics palette with minimal color overrides and readable light/dark variants.
- **FR-006**: Preserve all supplied screenshot filenames and original bytes in one shared asset location.
- **FR-007**: Provide reusable figures with translated captions/alternative text, optimized responsive images, lazy loading, and an original-resolution link. Both the image and its original link open a new tab, keeping the guide and reading position intact.
- **FR-008**: Provide repeatable development, validation, build, and production-preview commands. Use the latest stable compatible runtime and direct dependencies available during implementation, document compatibility limits, and make local and CI runtimes agree.
- **FR-009**: Prepare validation automation and manual publication restricted to main at the agreed free project address.
- **FR-010**: Exclude SDD files, addon files, unused source screenshots, and temporary validation pages from normal publication artifacts.
- **FR-011**: Deliver a locally verified guide site; keep publication behind the explicit content-launch decision and do not change game assets. Complete the design/tooling pass before collaborative guide authoring.
- **FR-012**: Center one compact list of five guide rows within a bounded reading area. Use consistent alignment, typography, and row spacing; omit article sidebars, separate card boxes, and empty grid cells. Place the two API audience labels in a bulleted list below the API heading at every screen width. Retain search/language/theme controls at all supported widths.
- **FR-013**: Use consistent typography, spacing, borders, and accent colors across the homepage and article body, including headings, lists, tables, asides, code, and screenshot captions. Preserve standard article navigation and verify the design with temporary article content excluded from publication.
- **FR-014**: Complete the bilingual mission-creation course, including character/group creation, and the agreed supporting content with prerequisites, instructions, expected outcomes, relevant screenshots, and examples verified against the example mission, actual addon/integration source, or official tool documentation. The course has one introduction, three parts and eight numbered chapters with on-page sections; supporting material stays outside its required reading sequence. Review the outline and prerequisites first, then have the operator verify each section before finalizing both languages.
- **FR-015**: Link supporting guides from the relevant steps of the main mission walkthrough without duplicating their detailed instructions. Explain opening the example and using Git in ordinary preface paragraphs, with both workflows optional; recommend studying existing missions as the best learning route and attribute that approach to the authors' experience. Explain Git through a concrete recovery example, distinguish explicit local commits from pushes to GitHub, and place the supplied folder screenshot in main-guide project creation. Use natural Russian technical-book prose: define unfamiliar terms, explain actions and results, and avoid slogans or redundant text. Use the exact Russian titles in contracts/site.md. Connect completed guides to the homepage list, localized sidebar navigation, language counterparts, and search; retain the API's two distinct audiences. Create supporting content when the walkthrough needs it.
- **FR-016**: Review the website source, configuration, dependencies, validation scripts, workflow, and feature artifacts for dead code, stale references, inconsistent settings, and reproducibility or cleanup defects. Resolve demonstrated issues while preserving the agreed design, original screenshots, and deferred content/publication scope.
- **FR-017**: Teach beginner GitHub Desktop use for a mission project: select the correct project folder, retain source assets/layers/metadata, ignore verified generated files, distinguish Workbench saves from local commits and remote pushes, verify a remote checkpoint, and demonstrate recovery on a separate tutorial project. Link this guide from the preface before project creation; any checkpoint reminders apply only to readers who choose Git. Screenshots may be added after text review; an existing configured Desktop installation must not need resetting.

- **FR-018**: Show a compact English/Russian help panel after homepage and article content, directing readers to #lite-lobby-issues on the configured Discord server and noting that the authors respond quickly. Its accessible button opens the supplied invite in a new tab. Match the existing typography and palette, and keep the panel usable on mobile without an overlay or external widget script.
- **FR-019**: Add a short bilingual homepage introduction outside the guide list, crediting the Triad Tactics collective and explaining that Lite Lobby serves the wider Arma community. Link the full GitHub source and official Bohemia APL terms. Summarize permission to reuse, modify, and share for noncommercial Arma use with attribution; do not imply unrestricted use or an unsupported license ranking.

- **FR-020**: Below 50rem, expose homepage theme, language, and community links through a keyboard-accessible navbar menu button, with native popover dismissal and desktop breakpoint reset. Center equally sized search/menu icons inside matching touch targets. Keep the popover compact with centered, evenly spaced social icons. Remove the separate homepage controls and social row, retain visible search and native article navigation, and preserve centered desktop search.
- **FR-021**: Omit the native footer when it has no edit link, update date, pagination, or credits. Remove redundant main bottom padding; when help is the last content, the main content box ends within 32px of it. A short page may still fill the viewport without creating extra scroll.

### Key Entities

- **Guide entry**: One of five topic titles with a stable anchor; the API entry also contains two audience labels. Mission creation, the example and Git have destinations; unwritten entries remain informational.
- **Localized page**: Same destination in English and Russian with translated interface labels.
- **Screenshot**: Original numbered image shared across languages, referenced by a localized figure.
- **Publication artifact**: Generated website containing only reader-facing content and required assets.

## Success Criteria

### Measurable Outcomes

- **SC-001**: Both homepages expose all five guide titles in the agreed order and their three active guide links resolve.
- **SC-002**: At 390, 768, 1440, and 1920 px viewport widths, navigation, language switching, theme switching, and search work without horizontal page overflow or clipped controls.
- **SC-003**: All 113 screenshots retain their filenames, lengths, and SHA-256 hashes after consolidation.
- **SC-004**: Repeated normal builds contain no temporary validation pages, internal specifications, or addon files.
- **SC-005**: The retained SDD integrations report healthy, with no modified or missing managed files.
- **SC-006**: Validation succeeds locally; publication remains inactive and requires an explicit manual action on main.
- **SC-007**: Both homepages display exactly five full-width guide rows with no article sidebar/contents panel. Rows share the same left edge, title position, and padding; the API row grows to fit its two bullets below the heading. Contents fit at every tested width. All seven topic/audience anchors remain unique.
- **SC-008**: Desktop and mobile captures of the homepage and representative article are visually reviewed in both languages/themes; headings, captions, and controls do not overlap or clip, and original images remain accessible.
- **SC-009**: At feature completion, the mission course and all agreed supporting content exist in English and Russian, contain no unfinished steps, and have been reviewed with the operator against their actual source/example/tool workflow. The course remains one introduction followed by eight chapters grouped into three parts.
- **SC-010**: At feature completion, all completed guide links and language counterparts resolve, and API examples match the implemented scripting and website interfaces.
- **SC-011**: The GitHub Desktop guide is verified on a separate demo mission project: required source files are committed, the remote commit is confirmed, and reverting a demonstrated bad commit restores the expected project state after Workbench reload. The existing 113 screenshots remain unchanged.

## Assumptions

- The operator's implementation request accepts the decisions recorded in Clarifications.
- Readers use current desktop/mobile browsers; authors have the supported local runtime.
- Public hosting and standard build runners remain within GitHub's free allowances.
- Community-specific guide titles and website tooling are explicitly authorized for this feature despite the constitution's general naming restriction.
- Engine UI, string-table, replication, and Workbench acceptance rules do not apply to this static website; addon behavior and assets remain unchanged.


### Project creation and naming clarification (T047)

The operator confirms that Location selects the parent folder and Workbench creates
an addon subfolder named after Project Name. Screenshot 02 is correct. Teach the
Launcher/New Project/Create actions before showing the resulting folders and gproj.
Remove the general naming principle from the preface (T049). Place a short
tip beside Project Name using Desert Storm rather than Desert_Storm or DesertStorm.
Explain its purpose in everyday language; omit the rejected lists of project and
repository examples from the preface. This is the operator's T048 correction to T047.
Present this as the guide's naming convention, not an unverified engine restriction.
Keep example/Git explanations as ordinary prose. Apply the correction in both locales.

### First naming-consistency reminder (T050)

Keep the approved Desert Storm example beside Project Name and use it to introduce
consistent naming. Ask readers to follow the naming patterns in screenshots throughout
the guide and explain why this matters for recognizable names and collaboration.
Mention Arma naming conventions while distinguishing conventions for different kinds
of names; do not imply that spaces are mandatory for every project/file/object.
Keep the reminder short, beginner-friendly, and equivalent in both languages.

### Published-copy review (T051)

Review all current English/Russian guide pages and shared reader-facing copy for
redundancy. Remove repeated explanations and generic encouragement; preserve actions,
reasons, beginner definitions, recovery limitations, optional workflows, references,
and screenshot context. Brevity must not turn the guides into unexplained fragments.
In the learning tip, explicitly tell readers to give AI a link to the relevant page
of this website, identify their step, and provide a screenshot. Retain coding-agent
help and explain its role briefly. Keep this editing principle for future sections.

### Connect Lite Lobby (T052)

The next collaborative slice adds Lite Lobby as the mission project's dependency,
after project creation. Explain why the dependency is needed, use supplied screenshots
03/04, and identify the Workshop folder by its confirmed GUID 699319AAA5BFC57F. Cover
Project Settings, plus/browse, gproj selection, OK, Launcher visibility, restart, and
a Resource Browser result check. Keep existing Arma Reforger dependency. Do not assume
the Workshop gproj filename or add unrelated example-mission dependencies.

### Missing addon recovery (T053)

Remove the redundant reminder to keep Arma Reforger from the dependency instructions.
Replace the proactive Launcher-list/Add Existing Project paragraph with a conditional
Missing Addon Dependencies explanation after reopening Workbench. Use the four supplied
screenshots: error, Workshop GUID search, Scan for Projects, and addon-folder selection.
Explain GUID as the missing addon's identifier, link the Workshop search page, install
the missing mod through the game if needed, scan the game's addons folder, and reopen
the mission. A missing dependency may be uninstalled or simply undiscovered by Launcher.
Preserve original image bytes and provide matching English/Russian instructions.

### Mission world creation (T054)

Continue the main guide with world creation using supplied screenshots 05–10. Explain
World Editor and sub-scene in beginner terms; load the example's Eden.ent base world,
create a sub-scene, and save ShootingStar_Ep1.ent in the addon's Worlds/Eden directory.
Show actions before results and preserve the base-game/project folder distinction.
The systems layer should be named a_systems as shown in 10; the operator confirms right-clicking the layer and choosing Rename.

### World naming reminder (T055)

Use Ctrl+S as the save instruction. Before screenshot 09, explain world filename
conventions and Worlds/Eden folder structure in a short bilingual tip. The operator
reports problems with engine renaming, so recommend choosing final names from the
start. Explain that Triad Tactics games have three episodes: follow the _Ep1 naming
pattern there; other missions can omit this suffix. Preserve the example filenames.

### Initial game mode setup (T056)

Continue the main guide through the Game Mode Setup wizard using originals 11–18.
Explain the active systems layer, GameMaster.conf template, scan results, generated
entities, and mission header in beginner terms. Show each action before its screenshot.
End with the generated Missions/ShootingStar_Ep1.conf and saved world. Explain that
map configuration remains to be completed. NavMesh setup is not required for this
guide, as confirmed by the operator. Header fields and replacement
with the Lite Lobby game mode belong to the next collaborative slice.

### Scenario details (T057)

Remove the wizard Close/return-to-World-Editor/save paragraph as requested. Continue
with the already-open mission header using screenshot 19: World, Name, Author,
Description, Player Count, and disabled Save Types. Explain fields for beginners;
128 is the example's value, not a claimed server limit. Save the edited config.

### Lite Lobby game mode (T058)

Continue with originals 20–21: remove the generated GameMode_Editor_Full instance
and its children from a_systems, then place LL_GameMode_Lobby.et in the active layer.
Explain prefab as a ready-made object template at first use. Retain the separate
MapEntity, PerceptionManager and SCR_AIWorld objects. Save the world; do not add
NavMesh instructions. Map configuration belongs to the following content slice.

### In-game map setup (T059)

Explain MapEntity configuration with original screenshot 22: select the scene object,
open its root properties, and assign Eden.topo and EveronRasterized.edds to the two
map fields. Explain each field briefly. The operator confirms compass/watch components
normally already exist; add them manually only if missing. NavMesh remains outside scope.

### Character prefab guide introduction (T060)

Historical route decision: the separate guide introduced here is incorporated into
the mission course by T097. Its technical content and old links are retained.

Create bilingual character-prefabs pages and link them from the main mission guide,
homepage and sidebar. Introduce character prefabs briefly, limit this walkthrough to
the existing US faction, and explain a consistent addon prefix (Triad Shooting Star →
TSS_) and original-game folder/name conventions. Link Bohemia Faction Creation for
the more advanced new-faction workflow. Detailed prefab creation will be authored next.

### Authenticity and unit organization (T061)

Before naming/creation, explain historical period, service/branch, equipment and
organizational structure for character design. Define Russian ОШС as
организационно-штатная структура in plain language. Treat company-versus-company
or several-platoon scale as the Triad Tactics convention, not a universal game rule.
Illustrate ordinary Company HQ → Platoon HQ → Squad, Special Forces Company HQ →
ODA directly, and the example's Alpha/Bravo split-team roles. Clarify ODA means
Operational Detachment Alpha (A-Team), and distinguish base-prefab composition from
world-instance changes and historical establishments. Include the supplied reference
photo without inventing its unit/date. Explain the AK-103/Afghan-war mismatch and
avoid generic USMC MultiCam for Iraq, with period/unit-specific sourcing. Acknowledge
game compromises while retaining the intent to represent the unit faithfully.

T062 presentation correction: replace the authenticity flow diagram and examples
with one short caution callout containing the two mistakes without external links.
Remove the reference photo from both pages. Preserve the three organization diagrams
and research citations in SDD; keep the supplied original asset for later use.

T063 editorial correction: teach unit organization before game group prefabs.
Explain that the Special Forces company equivalent consists of ODAs without a
platoon level. Keep the diagrams, describe Alpha/Bravo as the illustrative roster,
and connect personnel roles to future character prefabs in plain language. Remove
military-document links from public prose, retain research sources, and end the
organization section with a tip to research real unit structures online rather
than invent them. Scope the USMC MultiCam mistake to Iraq in 2003.

T063 follow-up: introduce Triad Shooting Star as the mission addon used by this
standalone guide. Apply authenticity guidance to other projects too, include
equipment and weapons in the research tip, and omit repeated prefab-reuse advice.
The naming section must also identify Triad Shooting Star as the example mission
addon and show how its initials form TSS_, with an equivalent reader-owned example.

### First inherited character prefab (T064)

Continue the character guide with original screenshots 23–24. Explain inheritance
briefly, then create TSS_Character_US_GB_BaseLoadout from Character_US_Base.et via
the World Editor Resource Browser's Inherit in action. Explain the filename's GB
and BaseLoadout parts and the resulting addon-local folder. Equipment editing and
subfolder organization remain the next jointly authored slice.

### Base outfit (T065)

Continue with screenshots 25–30: organize the prefab under Green Berets, open Edit
Prefab, select BaseLoadoutManagerComponent, and assign Hat/Jacket/Pants/Boots/Back
resources. Explain slots as places for worn equipment; use the example's exact
resources. Radio and inventory contents remain a later slice. Confirm folder
creation/movement interactions with the operator before writing those instructions.

### Radio and initial inventory (T066)

Continue with screenshots 31–37: duplicate Radio_ANPRC68 into the mission addon,
remove its second transceiver, assign initial items to the character's pants,
and equip a wristwatch. Explain duplicate, target storage and one entry per item
briefly. Use the photographed twelve-item list and the example prefab's watch.
Do not invent radio range changes: screenshot and current example differ there.

Operator clarification: hover over the second RadioTransceiver and press Delete.
Explain that two channels let each soldier communicate with their unit and the
whole side simultaneously; this mission removes the second to simplify gameplay.
For the later backpack-radio section, explain its second-channel removal simply
as a gameplay simplification. Record that decision now; do not draft that section yet.

### Choosing prefab operations (T067)

After wristwatch setup and before the ODA base, explain Inherit in, Duplicate to and Override in
in beginner language. Distinguish new variants from changing the original wherever
used with the addon loaded. Explain why unchanged equipment needs no duplicate,
why shared character variants benefit from inheritance, and how parent changes
propagate unless a child has its own value. Duplication retains existing parents;
do not describe it as removing all dependencies. Use short examples, no new diagram
component or external-document links in public prose. Link from the first
inheritance step so the explanation is available without delaying the walkthrough.

T067 readability revision: use concrete soldier/outfit examples, one short section
per command, and a diagram of the shared base and its character variants. Explain
parent terminology through that example. Preserve the distinction between copied
settings and retained parent relationships without abstract dependency terminology.

### ODA base character (T068)

Continue the walkthrough with screenshots 38–42 after the prefab-operation
explanation. Inherit from the shared base loadout, add PASGT armor and ALICE rifleman
webbing, and assign the example's M16A2 carbine to the primary weapon slot. Use the
current example filename TSS_Character_US_GB_ODA_Base, briefly noting the older
name without GB in screenshots. Explain the shared base's role; ammunition and
individual role variants remain later slices.

### ODA ammunition and equipment (T069)

Continue the ODA base with screenshots 43–49: grenade/throwable templates, inherited
pants inventory, flashlight and four ammo storage locations. Explain selecting
grenade slots by type and adding inventory entries in the child prefab. Match the
photographed ammo counts and source example. Clarify the flashlight location with
the operator because screenshots and the current prefab differ; do not infer it.

### Communications sergeant (T070)

Create TSS_Character_US_GB_CommsSergeant as a child of the ODA base. Use screenshots
52–56 for inheritance, backpack-radio duplication, second-channel removal,
assignment to Back and outfit/storage changes. Explain the backpack-radio change
as simplifying play, as previously requested. Skip the now-redundant rename in
50–51 while preserving those originals. Resolve how to clear the inherited
backpack inventory before writing that UI instruction. Editor labels/localization
remain the following slice.

### Character role and translated name (T071)

Continue with original screenshots 57–65. Set the communications-sergeant role,
create the addon's string table, add English and Russian names, build runtime
tables, register them in the project, and reference the string from the prefab.
Explain the difference between the translation key and displayed text, including
where the leading hash belongs. Keep this a complete beginner workflow; other
ODA specialists remain the next slice.

T071 hierarchy revision: localization is an addon-wide section alongside the
communications sergeant, not a subsection of that character. Group table creation,
translation entry, registration and name assignment under it. Preserve anchors.

### ODA commander (T073)

Continue with screenshots 66–71: inherit the ODA base, equip the example commander
with M72A3 and M22 binoculars, add his translated name to the existing table and
set ROLE_LEADER. Treat this equipment as a mission example, not a prescribed real
unit loadout. Keep the commander at the same heading level as the sergeant.

### Character catalog (T074)

Continue with originals 72–75: override the US character catalog in the mission
addon and duplicate a working entry to register the two completed role prefabs.
Explain the purpose of the catalog and why its override retains the original
filename. Distinguish the full roster shown in screenshots from the two characters
created so far; do not register the unfinished shared bases as playable roles.

### In-game character check (T075)

Use screenshots 76–78 to explain launching the mission world, opening Entity
Browser, finding the two created roles, and placing characters to inspect their
appearance and equipment. Explain placeholder thumbnails without inventing an
image-generation workflow. Confirm the operator's transition into Game Master.

### Group prefab foundation (T076)

Use screenshots 79–84 to introduce a group prefab, inherit Group_US_Base into the
mission addon, place it in the Green Berets group folder, and set its translated
name and military symbol. Explain Split Team Alpha using the earlier ODA diagram.
Do not assume the remaining four specialist prefabs already exist; decide their
instruction depth with the operator before populating the six-person roster.

T076 clarification: operator wants only a statement that the remaining specialists
were created by analogy. Include the six-person Alpha roster and formation from
85–86 in this slice; no separate specialist walkthroughs.

### Group catalog and verification (T077)

Use originals 87–90 to register Alpha in the US group catalog and find/place it
in Game Master. Explain that Company HQ and Bravo in the images are other groups
from the example, built the same way. Reuse the earlier game-launch instructions
and link back to the main mission guide after the check.

### Mission map markings (T078)

Resume the main mission guide with original screenshots 91–95: a map overview,
the RedLine polyline and the Port marking. Explain adding LL_MapPolyZone.et,
editing its points and setting line appearance. Keep map drawings distinct from
gameplay triggers. Point the character guide's return link to this continuation.

### Complete the mission walkthrough (T079)

The operator requests the entire remaining walkthrough in one pass. Cover original
screenshots 96–113: capture shape/flags/conditions, loss/timer/supremacy notices,
briefing text, playable groups and callsigns, vehicle affiliation/linking, and
spawn restriction zones. Finish with a practical multiplayer verification checklist
and the official Workshop publishing handoff. Keep bilingual parity and beginner
language. This completes the main walkthrough, not unrelated API/supporting pages
or public launch. Do not claim live game verification from website tests.

### Historical connected chapters and overview pages (T080–T083; superseded by T097)

The page boundaries, separate hubs and eight-figure ceiling below are retained only
as a record of this earlier design; T097 replaces them with one course and no
per-chapter figure limit.

Replace long walkthroughs with concise overviews and task-sized chapters. Mission
overview offers sequential creation or opening the example and studying it through
chapters. Operator clarified: Characters and groups is the top hub, Characters and
Groups are subordinate overviews with their own chapters. Do not reorder item work
before character work. Preserve confirmed steps, original figures and useful
screenshot discrepancies. Remove repetitive narration, not prerequisites.
Chapters identify their working context, link prerequisites, previous/next and
their overview. Keep left page navigation/right headings. Existing fragments reach
moved content. Expand example opening into a usable companion path. RU/EN parity,
no addon changes or publication. Acceptance: hubs have no procedural screenshot
walls, chapters have at most eight figures, all 116 prior figure occurrences per
locale survive, crosslinks/locale switching/old hashes/mobile navigation work.

### Russian editorial revision (T084–T086)

Review every Russian page after the chapter split. Restore fluent, connected
technical prose for confident computer users without development experience.
Correct calques, ambiguous shorthand, abrupt transitions and unnatural captions;
retain concise explanations of purpose and prerequisites. Brevity is not a reason
to omit the subject or turn prose into notes. Preserve structure, links, confirmed
procedures, UI labels, filenames, values and screenshot assets. Review shared
Russian homepage/support text too. English needs no editorial rewrite in this task.
