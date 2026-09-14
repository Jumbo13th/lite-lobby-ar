# Tasks: Lite Lobby Guide Site

- [x] T115 Prepared the reviewed guide changes for PR: reconciled current requirements and task status, corrected quickstart counts and trailing whitespace. Clean installation, full website validation, dependency audit and mergeability review passed before submission.

- [x] T114 Normalized all 144 PNGs under assets/screenshots/topic folders with lowercase kebab-case names and three-digit numbering. Consolidated inventories with original/current paths and unchanged hashes; updated imports, fixtures and validation. Confirmed all 20 illustrated locale-page image sequences unchanged, all 921 original/responsive HTTP URLs valid, and 564 image occurrences decode across RU/EN desktop/mobile. Full validation passed (107 production routes). Preview on 4322 rebuilt; unused source references remain unpublished.

- [x] T113 Audited navigation on all 16 active pages in RU/EN. Added top return links to Git and existing-mission articles, aligned all four supplementary entries in the overview/sidebar, preserved old anchors while aligning translated heading IDs, and removed the empty EN prefab heading. Extended existing validation for sidebar titles, supplementary navigation and heading parity. Full validation passed (107 routes); verified all four return journeys in RU/EN at 1440/390px on production preview port 4322. No local-development URLs or retired-page links found in active article navigation.

- [x] T112 Added the large-squad exception, at least two backpack radios per squad and separate administrator approval in RU/EN. Production build passed (107 pages).

- [x] T111 Added the supplied rotation URL and localized links to slot setup, unit planning, briefing formatting and triggers. Addon names remain unlinked. Full validation passed, including all local link and anchor destinations (107 pages).

- [x] T110 [US4] Published the supplied requirements by topic in RU/EN and linked point settings directly to admin approval requirements. Full site validation passed (107 pages); checked five sections, reciprocal anchor navigation and layout at 1440/390px in both locales. Additional suggested rules remain outside the pages pending discussion.

- [x] T109 [US4] Added both TriadMission setup examples in RU/EN with completed-mission introduction, requirements links and original-image inventory. Full validation passed (107 pages); both images, anchors, links and layout checked at 1440/390px in both locales.

- [x] T108 Removed the postponed API homepage entry and both labels in RU/EN; updated the four-entry contract and existing validation. Full validation passed (107 pages), including bilingual desktop/mobile homepage checks.

- [x] T107 [US4] Created the bilingual Triad Tactics missions section with exactly two introductory pages, homepage child links and a matching sidebar group. Full validation passed (107 pages). Verified both destinations, sidebar entries and reciprocal links on desktop/mobile in RU/EN. Server settings and requirements remain for collaborative authoring.

- [x] T106a [US4] Renamed the existing-mission guide and referring homepage/Git labels in both locales. Added Code → Download ZIP, extraction and launcher steps while preserving the archived example and anchors. Build passed (103 pages).
- [x] T106b [US4] Illustrated both examples with all 12 ReadyMission captures, ordered GitHub first and Workshop second. Linked Triad Shooting Star to the main course and both dependency errors to its existing recovery section. Used the shown TriadDanseMacabre Worlds folder without inventing a filename. Full validation passed (103 pages, preserved course and supporting images). Browser checks passed for both locales at 1440/390px: image order, original PNG bytes, opening in a new tab, recovery links and no overflow.

- [x] T105 [US4] Expanded RU/EN Git guide using all eleven supplied screenshots: repository/addon naming, folder layout, resourceDatabase.rdb ignore, clear frequent commits, publication/push, revert and AI help. Explained the optional revert demonstration and preserved save-commit-push. Full validation passed (103 pages, 116 course illustrations plus 11 Git illustrations per locale). Inspected desktop/mobile pages; all Git images loaded and all originals matched source bytes on port 4321.

- [x] T104 [US4] Fixed both original-image anchors by prefixing dev /@fs URLs with BASE_URL. Browser regression reproduced the 404 before the fix and passed afterward on port 4321. Full validation passed, including real screenshot/caption clicks in both locales on dev and production servers, with byte-for-byte original PNG verification.

- [x] T103 [US4] Added the operator-confirmed L shortcut tip after creating the base prefab in RU/EN. Built 103 pages and verified both rendered tips, inline key formatting and live pages on port 4321.

- [x] T102 [US4] Moved group creation/catalog/testing and screenshots 79–90 to chapter 5 in «Персонажи и группы»; chapter 6 now starts with world placement. Updated both locales, titles, overview, navigation, links and legacy fragment forwarding. Explained screenshot 91 as the intended orange play area and red prohibited boundary. Preserved all 116 screenshots per locale and literal chapter 5/6 settings. Full validation passed (103 pages; navigation, redirects, desktop/mobile and fixtures); restarted port 4321 to refresh cached sidebar metadata and visually verified both edited sections with loaded images.

- [x] T101 [US4] Audited all 51 Russian routes (14 active, 37 forwarding), captions, diagrams, shared component text and navigation. Corrected four pages: use «Альфа»/«Браво» in prose with English equivalents for reference, clarify faction abbreviations and align one remaining addon alternative description. Kept prefab definitions and exact software/UI names. Pre-edit comparison confirmed unchanged code values, UI names, headings, links and 116 screenshot sources. Built 103 pages and verified all four edited pages in production HTML and on port 4321.

- [x] T100 [US4] Named Arma Reforger Tools explicitly in the overview; reviewed Russian terminology and used «лаунчер», «аддон» and «мод» contextually while preserving product/UI names, «префаб» and brief definitions. Built 103 pages and verified all four edited pages in production HTML and on the running dev server. No new tests or dependencies.

- [x] T099 [US4] Reviewed all 14 active Russian pages and 37 retired routes using the operator's corrections and Russian excerpts of Martin, Fowler, K&R and Pro Git. Revised 11 Russian pages and added appropriate mission-name links in both locales. Preserved headings, code values, diagrams and all 116 screenshot components. Full website validation passed (103 production pages, both locales, desktop/mobile and isolated fixtures); final prose cleanup rebuilt successfully. Verified updated text and 11 mission-name links on eight pages served at port 4321. Sources and editorial decisions are recorded in research.md.

- [x] T098 [US4] Apply the six operator corrections to the course introduction in RU/EN: link the mission name, describe recreation from scratch, revise audience and reading advice, shorten Git rationale and remove AI-help prose. Production build passed (103 pages); rendered paragraphs, repository/example/Git links and four existing anchors verified in both locales. Port 4321 serves the updated introduction.

- [x] T097a [US4] Design one sequential course after the user chose 6–8 book-like chapters; map all lesson content to eight completed-work milestones.
- [x] T097b [P] [US4] Consolidate RU/EN text into eight chapters with coherent H2/H3 sections, removing repeated page scaffolding and preserving technical material and illustrations.
- [x] T097c [US4] Replace competing navigation trees with one course, rebuild overviews/metadata/pagination and forward prior routes/anchors to the new owners.
- [x] T097d [US4] Replace fragmented-layout assertions, review transitions and navigation, run website checks and inspect the rebuilt preview.

T097 analysis: a dependent tutorial is presented as a reference tree. User explicitly
chose 6–8 chapters; the design has eight. Existing Starlight navigation, headings and
fragment forwarding suffice. Remove the conflicting 9-image ceiling. All content
is mapped in plan.md, including two optional references. Checklist remains 17/17;
no engine uncertainty or launch decision. Read-only audits are complete.

T097 completed: one «Как создать миссию» introduction, three parts and eight
numbered chapters, with H2/H3 sections inside each chapter. Removed the separate
character/group navigation trees and homepage row; supporting explanations remain
optional. Preserved all 116 illustrations per locale and the 113 original PNG hashes.
Updated 38 legacy route maps, current links, localized pagination and companion pages.
Reviewed chapter dependencies and corrected missing returns to World Editor after
scenario editing and character testing. The introduction describes learning from
the example rather than promising a step for every specialist and opposing unit.

Astro check passed with zero errors/warnings and the production build contains
103 pages. Full static/browser validation passed (105 isolated fixture pages,
both locales and 390/768/1440/1920 px). The browser assertion for the example link
was updated to its canonical URL after the first run; no content defect was hidden.
Visually reviewed the Russian overview and mobile/desktop course navigation.
The existing preview on port 4321 returns the rebuilt overview with all three parts.
No addon changes, live Workbench verification or publication were performed.

- [x] T096 [US4] Separate character equipment from editor display in both locales: prepare both variants, translate their names, configure shared UI Info, then register/test in Game Master. Update overview, route/anchor compatibility and existing checks; rebuild and verify preview.

T096 analysis: the user identifies an ownership error, not a missing engine feature.
The bounded review found the same error for the commander. Existing roles route,
native navigation and screenshot components suffice; no new dependencies or game
facts. Plan records each stage's input/output and screenshot state. Requirements
checklist passes (17/17); no clarification or publication decision is needed.

T096 completed: equipment chapters now end with equipment; the shared translation
table covers both prepared characters, followed by Name/Authored Labels on roles
and then catalog registration. Retained all 27 affected figures and 21 prior IDs
per locale, with old destinations forwarded and roles reactivated without redirect.
Astro check/build passed; full website validation passed after correcting the
browser test's language-switch starting route. Validated 93 normal/95 fixture pages,
116 illustrations per locale, 113 original PNG hashes and both languages at all four
viewport widths. Reviewed the live roles page at 390/1440 px. No game assets changed.

- [x] T092 [US4] Audit learning dependencies and design complete practical chapters plus a topic index in existing feature 002 artifacts before content changes.
- [x] T093 [P] [US4] Reassemble bilingual character tasks and repair mission/group prerequisites in docs/src/content/docs, preserving all illustrations and known procedures.
- [x] T094 [US4] Rebuild overview reading routes, topic lookup, chapter metadata and legacy destinations for complete tasks.
- [x] T095 [US4] Replace rejected partition assertions with dependency/order and topic-entry checks; review transitions, run website validation and rebuild the preview.

T092 analysis: the preceding checks proved link and asset integrity but did not
prove teachability. The plan now records each task's input, output and screenshot
state. T093 owns content, T094 owns routes, T095 owns the bounded review and checks;
all requirements have coverage. No unresolved technical choice or engine change.

T093–T095 result: eight character tasks complete each parent before its children;
clothing/radio replacement includes storage corrections. Four overviews connect
32 chapters, with direct topic links to the same instructions. Early map setup and
playable-force setup precede their dependent checks. The bounded reading review
also corrected the ready-prefab entry to playable slots and an English wizard
instruction that prematurely sent readers to map setup.

Full `npm run validate` passed: 93 normal/95 isolated-fixture routes, all links and
anchors, 116 illustrations per locale, 113 original PNG hashes, and both languages
at 390/768/1440/1920 px. New browser journeys cover the base→equipment→weapons route,
topic lookup and historical chapter/fragment forwarding. Reviewed the character
overview and the corrected base-page ending in the live 4321 preview at 390/1440 px.
No game assets changed or Workbench execution claimed.

- [x] T091 [US4] Group capture, critical loss, timer and superiority under Triggers in both locales; move shared settings to conditions, synchronize navigation and old links, validate and rebuild docs.

T091 analysis: the author has supplied the grouping and terminology. The existing
chapter model and fragment forwarding support it; no new component or dependency
is needed. Scope is guide content/navigation and existing integrity checks.

T091 completed: five chapters under Triggers; map and briefing retain their own
category. All nine trigger illustrations and existing anchors preserved. Full
validation passed (89 normal / 91 fixture pages, 116 illustrations per locale,
113 original image hashes, locale/links and browser checks at all four widths).
Verified the rebuilt live preview and the new sidebar category visually.

- [x] T087 [US4] Specify and analyze thematic content organization in the existing feature 002 artifacts; preserve prior author decisions and the Russian editorial baseline.
- [x] T088 [P] [US4] Regroup bilingual mission and character chapters by task in docs/src/content/docs; retain all illustrations and standalone prerequisites.
- [x] T089 [US4] Update overview routes, sidebar metadata, crosslinks and legacy forwarding in docs/src/data, components and astro.config.mjs.
- [x] T090 [US4] Update chapter/journey validation, review the new topic boundaries and Russian transitions, run npm run validate and rebuild the local preview.

T087–T090: four overviews and 28 thematic chapters per locale, including 15 mission
topics and eight character topics. Related settings were moved together; six retired
routes retain navigation pages with fragment-aware forwarding and search exclusion.
The shared metadata separates mission sidebar categories from the beginner route.
All 116 illustration blocks per locale and original image hashes are preserved.
Full validation passed: 83 normal / 85 fixture HTML pages; old and new navigation,
independent radio-topic entry, language switching and mobile/desktop controls.
Reviewed Russian topic transitions and menu captures. Final small wording/return-link
corrections were followed by a production rebuild and chapter-contract validation.
No game assets, repository history or publication were changed.

- [x] T084 [US4] Establish the Russian editorial standard and review all 33 Russian MDX pages and shared Russian copy against it.
- [x] T085 [US4] Revise unnatural phrasing and restore explanatory connections while preserving technical content, navigation and assets.
- [x] T086 [US4] Review consistency and source invariants, run existing website validation and rebuild the preview.

T084–T086 completed: all 33 Russian pages read in full; 32 MDX sources revised,
with the homepage wording updated in LL_ProjectIntro. Instructions now identify
their working object, explain prerequisites and preserve the connection between
actions. Captions, descriptions, inheritance/inventory/localization explanations
and mission-condition prose were edited. Four chapter titles now name their tasks
or objects explicitly; navigation labels remain synchronized. The approved Discord
wording and English content are unchanged.

Validation: compared the pre-edit snapshot to all Russian pages; preserved imports,
116 screenshot references in order, explicit IDs, link destinations, inline-code
values and English UI labels. No numeric values disappeared from mission chapters.
Full npm run validate passed: 67 normal / 69 isolated fixture pages, 113 original
image hashes, locale parity, links/assets and Chromium at 390/768/1440/1920 px.
Reviewed desktop/mobile captures. Normal preview rebuilt; no addon assets or
repository history changed. Automated checks verify integrity, not prose quality;
language was reviewed by reading the edited chapters and overviews.

**Input**: [plan.md](plan.md), [spec.md](spec.md), research.md, data-model.md, contracts/site.md.
**Tests**: Production/browser validation is explicitly required by the approved plan.

## Phase 1: Setup

- [x] T001 Complete specification, clarification, and quality checklist in specs/002-guide-site/spec.md and checklists/requirements.md (FR-002).
- [x] T002 Complete design artifacts and this task list in specs/002-guide-site/ (FR-002).
- [x] T003 Run read-only cross-artifact analysis of specs/002-guide-site/spec.md, plan.md, and tasks.md (FR-002).

## Phase 2: Foundation

- [x] T004 Add AGENTS.md directing Codex to the shared .claude/skills and constitution; verify original integration state is preserved (FR-001, FR-002).
- [x] T005 Create docs/package.json, lockfile, TypeScript/content/Astro configuration and scoped ignore rules with the agreed runtime and script contract (FR-008).

## Phase 3: User Story 1 - Bilingual preview

**Goal**: Read and navigate the six planned topics in either language.
**Independent test**: Open both root/ru homepages under the project prefix at mobile and desktop widths.

- [x] T006 [US1] Create localized homepages in docs/src/content/docs/index.mdx and ru/index.mdx with the singular site titles in contracts/site.md, matching metadata/home navigation, shared stable anchors, and two API audience headings (FR-003, FR-004).
- [x] T007 [US1] Configure translated sidebar, built-in search/theme/navigation, community links, and minimal Triad Tactics palette overrides in docs/astro.config.mjs and docs/src/styles/custom.css; expose native social icons on mobile homepages through LL_Header.astro (FR-003, FR-005).
- [x] T035 [US1] Share LL_SocialIcons.astro through the Starlight component override and mobile homepage; open all three social links in new tabs, preserve their appearance, and verify production links in both locales and layouts (FR-005).
- [x] T036 [US1] Add the bilingual help panel through docs/src/components/LL_Footer.astro and the Footer override in docs/astro.config.mjs; verify its channel text, new-tab Discord button, and desktop/mobile presentation on homepages and temporary articles (FR-018).
- [x] T037 [US1] Apply the corrected Russian help text in LL_Footer.astro and add LL_ProjectIntro.astro to both localized homepage sources; use natural Russian license wording and a clickable Triad Tactics credit, and verify the community message, external links, accurate APL summary, and responsive presentation (FR-018, FR-019).
- [x] T038 [US1] Add the Git entry and apply the six-guide order in contracts/site.md to LL_GuideHome.astro and both homepage sources; update existing production validation for the ordered headings and eight anchors, then verify both languages and responsive layouts (FR-004, FR-012; SC-001, SC-007).
- [x] T039 [US1] Add the mobile navbar popover through LL_Header.astro, remove homepage preference/social rows, and retain native article controls; update existing browser checks for keyboard dismissal, breakpoint reset, and translated controls (FR-005, FR-020).
- [x] T040 [US1] Render the native footer only when it has content and remove redundant main padding; validate at most 32px between the final help panel and the main box end, then review mobile/desktop output (FR-021).
- [x] T041 [US1] Center and size navbar glyphs consistently in custom.css, narrow LL_Header.astro's popover, and center its social icons in equal touch targets; extend existing geometry checks and visually verify both languages/themes (FR-020).
- [x] T008 [P] [US1] Implement route/translation/link checks and desktop/mobile browser scenarios in docs/scripts/validate.mjs and docs/tests/ (FR-003, FR-004, FR-005, FR-008, FR-010).

## Phase 4: User Story 2 - Screenshot authoring support

**Goal**: Preserve original assets and verify illustrated content without publishing sample guide prose.
**Independent test**: Compare all original hashes and render small/large figures in isolated fixture output.

- [x] T009 [P] [US2] Record original filenames/lengths/SHA-256 values in docs/tests/screenshots.json and migrate all PNGs to docs/src/assets/guide/ (FR-006).
- [x] T010 [US2] Add docs/src/components/LL_Screenshot.astro with required localized text, constrained optimization, lazy loading, and original links (FR-007).
- [x] T011 [US2] Extend docs/scripts/validate.mjs and docs/tests/ with asset integrity and isolated screenshot/code fixtures, cleanup, and normal-output exclusions (FR-006, FR-007, FR-010).

## Phase 5: User Story 3 - Validation and publication preparation

**Goal**: Repeatable local/CI validation and a dormant manual publication path.
**Independent test**: Run validation and inspect workflow triggers/permissions without deploying.

- [x] T012 [P] [US3] Add .github/workflows/docs.yml for relevant PR/push validation and explicit manual main-only publication after successful validation (FR-009, FR-011).
- [x] T013 [US3] Verify local npm commands and maintainer steps in specs/002-guide-site/quickstart.md (FR-008).

## Phase 6: Integrated acceptance

- [x] T014 Run npm run validate from docs; inspect desktop/mobile output and resolve demonstrated failures (FR-003 through FR-010).
- [x] T015 Verify shared integration health, original screenshot preservation, clean normal artifact, and no game changes/publication; mark completed tasks in specs/002-guide-site/tasks.md (FR-001, FR-006, FR-010, FR-011).

## Phase 7: Design refinement setup

- [x] T016 Update the existing specification, clarifications, quality checklist, plan, research, content model, and interface contract in specs/002-guide-site/ for the accepted design pass (FR-002, FR-012, FR-013).
- [x] T017 Run read-only cross-artifact analysis of the revised specs/002-guide-site/spec.md, plan.md, and tasks.md before further site edits (FR-002).

## Phase 8: User Story 1 - Guide-list homepage

**Goal**: Present six clear guide topics in a centered homepage with accessible controls.
**Independent test**: Both homepages show six aligned rows, retain eight anchors, omit article navigation, and expose working controls at 390/768/1440/1920 px.

- [x] T018 [P] [US1] Adapt docs/tests/production.mjs and docs/scripts/validate.mjs to verify six aligned guide rows, absent sidebars, stable anchors, header navigation, search, and mobile language/theme access across the expanded viewport matrix (FR-003, FR-004, FR-005, FR-010, FR-012; SC-001, SC-002, SC-007).
- [x] T019 [US1] Refine docs/src/components/LL_GuideHome.astro and LL_HomeHero.astro using one compact list with six aligned guide rows and two API bullets below the API heading; center desktop header search in docs/src/styles/custom.css and update localized homepage sources (FR-003, FR-004, FR-005, FR-012).

## Phase 9: User Story 2 - Consistent article design

**Goal**: Verify a readable article using the same typography, palette, and spacing as the homepage.
**Independent test**: Temporary bilingual articles render prose, lists, a table, an aside, code, and two screenshots while retaining native article navigation.

- [x] T020 [P] [US2] Extend docs/tests/fixtures.mjs and docs/tests/production.mjs with representative article elements, native article navigation checks, and both-theme captures; preserve fixture cleanup and normal-output exclusion (FR-005, FR-007, FR-010, FR-013; SC-004, SC-008).
- [x] T021 [US2] Refine shared article typography and surfaces in docs/src/styles/custom.css, supported Expressive Code style overrides in docs/astro.config.mjs, and figure/caption styling in docs/src/components/LL_Screenshot.astro (FR-005, FR-007, FR-013).

## Phase 10: Refined design acceptance

- [x] T022 Run npm run validate from docs; visually review homepage/article captures in both languages/themes at narrow and wide widths, resolve demonstrated issues, and refresh the local production preview (FR-003 through FR-013; SC-001 through SC-008).
- [x] T023 Verify feature 002 remains active, no feature 003 remains, shared integration health, all 113 original screenshots, and excluded temporary artifacts; record final evidence and complete tasks in specs/002-guide-site/ (FR-001, FR-002, FR-006, FR-010, FR-011).

## Phase 11: Current tooling

- [x] T024 Audit and upgrade stable compatible runtime/package versions in docs/package.json, docs/package-lock.json, docs/.nvmrc, and .github/workflows/docs.yml; update specs/002-guide-site/research.md and quickstart.md and verify local installation (FR-008).

## Phase 12: User Story 4 - Complete guide content

**Goal**: Complete the mission course and agreed supporting guides inside feature 002; API content is deferred.
**Independent test**: Review each guide against the example/source and use its actual route, images, and language counterpart.

- [x] T033 [US4] Completed the mission outline and prerequisites through the reviewed eight-chapter course (T097 and subsequent operator review).
- [x] T034 [US4] Completed and reviewed the bilingual Git guide using the operator-supplied demo-project screenshots for commits, push and recovery (T105). Captures now live under assets/screenshots/git.
- [x] T025 [US4] Completed the bilingual mission-creation course and operator content review; final structure, links and images verified through T113–T114.
- [x] T042 [US4] Add the approved preparation section in both create-mission.mdx pages, link the Lite Lobby prerequisite to the operator-supplied Workshop listing, connect its homepage row and sidebar entry, and verify keyboard navigation, search destinations, and language counterparts. This completes the first slice of T025/T030; later guide sections remain open (FR-014, FR-015; SC-010).
- [x] T044 [US4] Create bilingual example/Git introductory pages and connect them from the main guide, homepage, and sidebar. Preserve the supplied repository screenshot unchanged. Detailed supporting-guide procedures remain in T028/T034; the subsequent editorial revision is tracked in T045 (FR-014, FR-015, FR-017; SC-010).
- [x] T045 [US4] Review Russian technical-book style references; rewrite both mission-guide prefaces as ordinary prose with authors plural, optional example/Git links, and concrete Git recovery benefits. Move the supplied screenshot and folder explanation from Git to main-guide project creation, preserving bytes. Align SDD requirements, check routes and rendering, and rebuild the preview (FR-015, FR-017; SC-010).
- [x] T046 [US2] [US4] Keep the guide open when either screenshot link is activated; add original screenshots 01.png and 02.png beside bilingual project-creation instructions and describe the Location field (corrected in T047). Verify new-tab mouse/keyboard behavior, original bytes, responsive rendering, and production output (FR-006, FR-007, FR-014).
- [x] T026 [US4] Completed character and group prefab creation within course chapters 3–5; supporting explanations remain separate (T097–T113).
- [x] T027 [US4] Completed the agreed Triad Tactics setup and requirements pages, including trigger points, StatZone and server rules (T107–T112).
- [x] T028 [US4] Completed and reviewed the illustrated GitHub and Workshop mission-opening guide (T106b).
- T029 [US4] Deferred by the operator in T108: API pages are outside the current delivery and hidden from navigation.
- [x] T030 [US4] Connected the completed guides through localized homepage, sidebar, search and course navigation; verified through T113–T114.
- [x] T031 Operator accepted the guide and authorized PR preparation. Clean-snapshot npm ci and full validation passed for 107 routes; public deployment remains a separate manual action.

## Phase 13: Cleanup review

- [x] T032 Review docs source, configuration, dependencies, validation scripts, .github/workflows/docs.yml, and feature 002 artifacts; fix demonstrated dead code, inconsistencies, stale references, and cleanup defects; run production validation and record the result (FR-016; SC-001 through SC-008).

## Phase 14: Compact article contents panel

- [x] T043 [US2] Constrain the right-hand contents panel to 16rem and center the bounded article/contents layout in docs/src/styles/custom.css. Restore the native title divider. Verify panel width, readable article width, chapter links, and mobile navigation in both languages/themes (FR-005, FR-013; SC-002, SC-008).

## Dependencies and Parallel Work

T001 → T002 → T003 → T004/T005. After the interfaces in T005 are fixed, root handles
T006/T007/T009/T010 while validation work T008/T011 and workflow T012 can proceed in
parallel on separate files. T009 supplies the manifest consumed by T011. T013–T015
run after the site, validator, and workflow are integrated.

For the design refinement: T016 → T017, then T018/T020 (one validation owner) may
run alongside T019/T021 (one UI owner). Tasks sharing a file stay sequential within
their owner's work. T022 follows both streams; T023 completes acceptance. No new
feature number is created. The existing directory and branch are renamed 002-guide-site
at the operator's request. T024 precedes final design validation. T025–T031 remain open
for the collaborative content phase after design acceptance; feature 002 is not complete
until that phase is complete.
T032 reviews and cleans the implemented site before collaborative guide authoring.
T033 precedes the remaining T025 sections; the approved preparation section can proceed
as T042 while the wider outline remains under review. During T025, supporting-guide tasks T026–T029 run when their
procedures are needed, and T030 connects each reviewed destination as it becomes available.
T034 supplies the optional Git workflow referenced by T025; using Git or opening the example
is not a prerequisite for the reader's mission-creation steps.

## Implementation Strategy

First build and preview the bilingual homepages. Then integrate image support and
the isolated fixture checks. Finally run the complete validation and verify the
manual publication contract. No commit, push, or deployment is included.

## Design and Tooling Acceptance — 2026-09-12

- Feature directory, branch, active pointer, and references use 002-guide-site. SDD analysis found no remaining scope or coverage gaps; T025–T031 remain open for collaborative guide writing.
- Both homepages use six aligned rows, a smaller title, and two API bullets below the API heading. Row padding is consistent and text wraps without clipping. The guide list contains only titles and API audience labels.
- All six titles and their order match contracts/site.md in both languages, including the Git entry. Static and browser validation enforce the ordered headings and eight unique anchors; production validation and desktop/mobile visual review passed.
- Desktop search is centered in the viewport from 1024 px. Browser checks at 800/1023/1024/1280 px confirm it remains clear of the logo and language/theme controls across the breakpoint.
- Mobile navbar menus expose theme, language, and social links without duplicate content rows. Keyboard entry, Escape focus return, outside dismissal, and the 799/800px transition passed on homepages and articles in both languages. Natural-width tablet header columns prevent the article language selector from clipping.
- Mobile search/menu icons share centered 20px glyphs in 44px targets. The 224px homepage popover centers three evenly spaced social targets and fits a 320px viewport. Both-language/theme visual review, Astro Check, production build, and full website validation passed.
- Empty native footer markup and redundant main padding no longer extend the page. The trailing content inset is 24px, down from about 149px; both-theme homepage/article checks enforce the 32px limit. Production validation and mobile-menu/desktop visual review passed.
- Discord, GitHub, and Telegram icons use the supplied URLs. English/Russian checks at 390/800/1024/1440 px confirmed visible named links, keyboard focus, and clear header spacing with centered desktop search.
- Social links open new tabs through the shared component. Keyboard activation verified all three destinations in English/Russian at 390/1440 px while preserving the source page; Astro Check, production build, and full website validation passed.
- The bilingual help panel names #lite-lobby-issues and the authors' quick responses. Its Discord action passed keyboard/new-tab checks at 390/1440 px in both languages; homepage/article captures show readable, aligned content without overlap. Full production validation passed with all 113 original screenshots preserved.
- Both homepages credit Triad Tactics, welcome other Arma communities, and link the full source and official APL terms with an accurate permission summary. The corrected Russian help text matches the operator's wording. Production validation passed; final homepage checks covered text, links, keyboard access, and both themes at 390/1440 px.
- Article headings, code, asides, figures, and captions share the site palette and spacing. Both-theme homepage/article captures are available in the ignored docs/.validation-results directory.
- The right contents panel is 256px wide from the 1152px desktop breakpoint and stays 24px from the article text. Browser geometry and chapter-link checks passed at 390/1151/1152/1440/1920/2560px in both languages/themes; desktop and mobile captures were visually reviewed. The native title divider is restored. Full production validation passed and the local preview serves the rebuilt site.
- Current compatible versions are recorded in research.md: Astro 7.3.2, Starlight 0.42.0, Node 26.8.2, and npm 12.0.2. TypeScript stays at 6.0.3 within Astro Check's supported peers. The quickstart explains selecting the installed per-user runtime.
- Clean npm ci succeeded with zero audit vulnerabilities. Package engines, runtime pin, lockfile, and CI agree; npm permits only the pinned esbuild installation script required by the build.
- npm run validate passed with zero Astro diagnostics, successful production builds, link/asset/locale checks, and Chromium homepage/article scenarios at 390, 768, 1440, and 1920 px in English and Russian. Search, keyboard navigation, language switching, and both themes passed.
- All 113 original filenames, lengths, and SHA-256 hashes match the inventory (71,401,274 bytes). Representative screenshots passed responsive sizing, translated text, lazy loading, and original-byte checks.
- Temporary article sources/output and validation servers were removed; fixture checks did not change normal output hashes. SDD files, addon files, unused screenshots, and temporary content are excluded from publication output.
- Shared Claude integration remains the healthy default with zero missing/modified managed files. Codex uses .claude/skills through AGENTS.md; no .agents collection or additional feature remains.
- Local development and production-preview commands serve both locales. CI validation and main-only manual publication are prepared; Pages remains inactive and nothing was committed, pushed, or deployed.
- Cleanup review fixed partial-write ownership and best-effort fixture removal, exact screenshot filename matching, and original-dimension checks. Focused regression checks and full production validation passed; obsolete exclusions, unused captured fields, and stale setup/SDD text were corrected.
- Dependency tree/lock/runtime/workflow checks passed; npm audit reported zero vulnerabilities. Automatic approval review blocked removal of old generated captures; those files remain ignored and excluded from publication.

## Content Acceptance — 2026-09-12

- The approved preparation section is available in both mission-guide routes. Homepage row clicks, keyboard focus/activation, sidebar navigation, localized search, and language counterparts passed production checks. Astro Check and build passed; mobile/desktop visual review passed and all 113 original screenshots remain unchanged. Remaining sections and supporting guides are still under collaborative review.

- Bilingual example/Git introductions are linked from the main guide, homepage, and sidebar. Their initial implementation passed validation and preserved the supplied PNG. The rejected tip presentation and screenshot placement are superseded by T045. Detailed project-creation and supporting-guide procedures remain under review.

- T045: ordinary English/Russian prefaces replace the rejected tips, with authors plural and explicit commit/recovery/push explanations. The supplied PNG is rendered in main-guide project creation from docs/src/assets/mission/repository-layout.png; SHA-256 remains c5f95489ad1ae1777ad954bddb0b34471d7dc34741edee76e684d18c9f6a416b. Full validation passed (9 publication routes, 11 with isolated fixtures, all 113 original screenshots). Focused browser checks verified preface links, main-only screenshot placement, original image bytes, Git return anchors, and no overflow at 390/1440 px in both languages. Production preview rebuilt; wording remains available for operator review.

- T046: both screenshot actions now open original PNGs in new tabs. Original guide images 01/02 illustrate Launcher and New Project in both languages, with the initial Location interpretation superseded by the operator correction in T047. Full validation passed (9 normal routes, 11 fixture routes, all 113 original hashes). Focused Chromium checks at 390/1440 px in English/Russian verified all three article images load and both mouse/keyboard actions preserve the guide URL and scroll. Preview rebuilt; dependency setup remains for the next collaborative section.

- [x] T047 [US4] Correct Location to the parent folder and automatic addon-folder creation; move the resulting layout after Workbench steps; add a bilingual preface naming tip with examples and rationale. Remove stale SDD claims, preserve section anchors and screenshots, rebuild, and validate (FR-014, FR-015).

- T047: Location corrected to the parent repository directory, automatic Project Name folder creation explained, and the result moved after Create in both locales. A preface naming tip gives project/repository examples and rationale. Full validation passed, preserving all 113 originals. Focused 390/1440 px browser checks confirmed section/image order (01, 02, repository layout), naming examples, image loading, and no page overflow. Production preview rebuilt. Workbench behavior is operator-confirmed; no Workbench execution was performed by the agent.

- [x] T048 [US4] Record the non-developer beginner audience in feature 002. Replace the long preface naming callout with a short principle, place one example beside Project Name, and explain addon/repository at first use. Verify bilingual content, step order, preserved screenshots, and rebuild the preview (FR-014, FR-015).

- T048: feature audience now explicitly requires no Arma Reforger, Workbench, Git, or developer experience. The preface naming callout is replaced by two plain sentences; one short example sits directly after Project Name. Addon and repository are explained at first use in the steps. Astro Check and production build passed; focused English/Russian Chromium checks at 390/1440 px confirmed placement, brevity, preserved screenshot order, and no overflow. All 113 original image hashes matched. Preview rebuilt; prose remains subject to operator review.

- [x] T049 [US4] Remove the general naming paragraph; add a short bilingual learning tip after the preface with beginner audience, accurate authorship, understanding the steps, and AI/coding-agent help. Check build and rendered placement (FR-014, FR-015).

- T049: removed the general naming paragraph and added the bilingual learning tip after the preface. It states the beginner audience, authors' experience and AI drafting assistance, illustrates why understanding a step matters, and suggests AI/coding-agent explanations. Astro Check and build passed; focused browser checks verified the new text, placement, attribution, and no overflow at 390/1440 px in both languages. Preview rebuilt.

- [x] T050 [US4] Extend the approved Project Name tip as the first naming-consistency reminder, referring to screenshot examples and type-specific Arma conventions. Verify wording against official references, update both languages, and rebuild (FR-014, FR-015).

- T050: preserved the approved Desert Storm paragraph, expanded its tip into the first naming-consistency reminder, and tied screenshot conventions to readability and collaboration. Official indexed Bohemia references support type-specific conventions. Both locales built successfully and the running preview serves the revised text.

- [x] T051 [US4] Review all published bilingual page/component copy; remove redundant prose without losing instruction, context, or beginner definitions. Explicitly ask readers to supply the guide-page URL when asking AI for help. Validate and rebuild (FR-014, FR-015).

- T051: reviewed all eight published MDX files and shared homepage/help/navigation copy. Trimmed repeated prose in six guide files, preserving beginner definitions, recovery requirements, optional workflows, names, links, images, and action/result order. The AI tip explicitly asks for this guide's page URL, step, and screenshot. Full validation passed (9 publication routes, 11 fixture routes, all 113 original hashes); focused checks at 390/1440 px passed in both locales. Preview rebuilt. Detailed supporting-guide procedures remain for joint authoring.

- [x] T052 [US4] Draft bilingual Lite Lobby dependency instructions using screenshots 03/04 and official documentation, including save/restart, Launcher availability, and Resource Browser check. Validate new anchors, image originals, build, and preview; world creation remains next (FR-014, FR-015).

- T052: drafted the dependency subsection in both mission-guide locales using screenshots 03/04 and official dependency/Launcher documentation. Full site validation passed (9 normal routes, 11 fixture routes, all 113 original hashes). Focused 390/1440 px checks verified the new anchor, four selection steps, screenshot bytes/loading, new-tab originals, and no overflow. Preview rebuilt. This section is ready for operator content review; no live Workbench run was performed. World creation is the next slice.

- [x] T053 [US4] Replace the redundant dependency reminder and proactive Launcher check with bilingual missing-addon troubleshooting. Save the four supplied screenshots unchanged; explain GUID lookup, in-game installation, Scan for Projects, game-addon folder selection, and reopening. Validate and rebuild (FR-006, FR-007, FR-014, FR-015).

- T053: removed the redundant Arma Reforger reminder and proactive Add Existing Project paragraph. Added bilingual missing-dependency recovery with GUID search, in-game installation, Scan for Projects, game-addon folder selection, and reopening. Four supplied PNGs preserved unchanged (hashes in research.md). Full validation passed, including all 113 original inventory hashes; focused English/Russian 390/1440 px checks passed for new image order, source hashes, links, loading, and no overflow. Production preview rebuilt.

- [x] T055 [US4] Use Ctrl+S for saving and add the operator-requested world naming/folder tip before screenshot 09 in both languages. Rebuild and verify text and placement (FR-014, FR-015).

- [x] T056 [US4] Draft bilingual Game Mode Setup instructions with screenshots 11–18, active layer, template selection, scan/generation, and mission header creation. Validate original images, anchors, locale parity, and mobile/desktop preview (FR-006, FR-014, FR-015).

- [x] T057 [US4] Remove the unnecessary wizard ending and explain mission-header fields using screenshot 19 in both languages. Verify sources, build, field table, screenshot and responsive layout (FR-014, FR-015).

- [x] T058 [US4] Explain replacing the generated Game Master instance with Lite Lobby using screenshots 20–21, preserving separate system objects. Verify sources, bilingual output, image originals, and mobile/desktop layout (FR-006, FR-014, FR-015).

- [x] T059 [US4] Explain MapEntity map resources with screenshot 22; clarify the compass/watch component action, then validate the bilingual section, image original and responsive build (FR-006, FR-014, FR-015).

- [x] T060 [US4] Add bilingual character-prefab introductions covering US scope, TSS_ naming and vanilla folder conventions, with the advanced faction-creation link. Connect main guide, homepage and sidebar; validate navigation and production output (FR-014, FR-015).

- [x] T061 [US4] Add sourced authenticity/unit-organization guidance, four accessible diagrams and the supplied reference photo to both character guides. Verify the example's split-team roles, distinguish game adaptations, and validate layout, localization and original-image preservation (FR-006, FR-014, FR-015).

- [x] T062 [US4] Replace the authenticity flow/examples with a concise caution without links, remove the displayed photo, and retain the three organization diagrams in both locales. Rebuild and check presentation (FR-014, FR-015).

- [x] T063 [US4] Refine bilingual unit-organization prose, remove military-document/group-prefab links, explain the role-to-character-prefab connection, and add an online-research tip. Build and verify both locales (FR-014, FR-015).

- [x] T064 [US4] Explain creating the first inherited character prefab using original screenshots 23–24. Verify official inheritance behavior, bilingual build and screenshot presentation (FR-006, FR-014, FR-015).

- [x] T065 [US4] Explain folder organization and base outfit with screenshots 25–30, confirm folder interactions, and verify bilingual production rendering (FR-006, FR-014, FR-015).

- [x] T066 [US4] Add bilingual radio, initial inventory and wristwatch instructions with screenshots 31–37; verify sources, build, image integrity and responsive output (FR-006, FR-014, FR-015).

- [x] T067 [US4] Explain inheritance, duplication and overrides after wristwatch setup and before the ODA base, linked from the first prefab operation; verify concise bilingual guidance, build and responsive layout (FR-014, FR-015).

- [x] T068 [US4] Explain the inherited ODA base, armor, webbing and primary weapon with screenshots 38–42; verify sources and bilingual production rendering (FR-006, FR-014, FR-015).

- [x] T069 [US4] Explain ODA grenade templates, flashlight and ammunition with screenshots 43–49; resolve flashlight placement and validate both locales (FR-006, FR-014, FR-015).

- [x] T070 [US4] Explain the communications-sergeant variant and backpack radio with screenshots 52–56; clarify inventory cleanup and validate bilingual output (FR-006, FR-014, FR-015).

- [x] T071 [US4] Explain character role, bilingual string-table creation, runtime build/registration and prefab name assignment with screenshots 57–65; validate both locales (FR-006, FR-014, FR-015).

- [x] T072 [US4] Separate addon-wide localization from the communications-sergeant heading in both locales; preserve links and rebuild.

- [x] T073 [US4] Add the ODA commander walkthrough with original screenshots 66–71, launcher, binoculars and localized role/name; verify both locales (FR-006, FR-014, FR-015).

- [x] T074 [US4] Explain US character-catalog override and adding the completed character prefabs with originals 72–75; validate both locales (FR-006, FR-014, FR-015).

- [x] T075 [US4] Explain in-game checks with screenshots 76–78; confirm entering Game Master and validate both locales (FR-006, FR-014, FR-015).

- [x] T076 [US4] Introduce the group prefab and configure Alpha's folder, translated name, military symbol, roster and formation using originals 79–86; validate both locales (FR-006, FR-014, FR-015).

- [x] T077 [US4] Add group catalog registration and Game Master check with originals 87–90, plus return link to the mission guide; validate both locales (FR-006, FR-014, FR-015).

- [x] T078 [US4] Continue the mission guide with map markings using originals 91–95; update character return links and verify both locales (FR-006, FR-014, FR-015).

- [x] T079 [US4] Complete the main mission walkthrough with screenshots 96–113, final testing and official publishing handoff; validate both locales and rebuild preview (FR-006, FR-014, FR-015).

- [x] T080 [US4] Restructure docs/src/content/docs into bilingual mission, character and group hubs/chapters; preserve figures and trim repeated prose.
- [x] T081 [US4] Add native chapter navigation in docs/astro.config.mjs and chapter metadata, rewrite crosslinks and preserve old fragments.
- [x] T082 [US4] Expand docs/src/content/docs/{example-mission,ru/example-mission}.mdx into a practical companion path.
- [x] T083 [US4] Extend docs/scripts/validate.mjs and docs/tests/{chapters,production}.mjs for chapter navigation/structure; validate and rebuild preview.

T080–T083 completed: four overviews and 26 chapters per locale. Characters and
groups contains separate Characters and Groups overviews. Both main overviews
offer sequential reading or inspection of the example; its companion page now
includes pinned ZIP, project/dependency/world opening and inspection links.
Native sidebar branches and explicit previous/next preserve the reading sequence;
chapter navigation precedes support. Existing 94 fragments forward to their new
destinations. Repeated introductions/headings were shortened, cross-page references
made explicit, and the prefab reference remains linked after the watch step.

Validation: npm run validate passed with 67 normal and 69 isolated fixture pages,
RU/EN parity, internal links/assets, all 113 original PNG hashes, and Chromium at
390/768/1440/1920 px. New checks cover overview/chapter reachability, sequence,
legacy initial/hashchange redirects, mobile current branches, example reading
route and footer order. Independent source comparison preserved all 116 exact
screenshot blocks and asset mappings, eight tables, two code blocks and four
diagrams per locale. Final rebuild and structural checks passed after adding the
watch-to-reference link; both localized links verified on preview port 4321.
No addon assets changed or live Workbench test performed.

- T079: completed both mission pages through original 113, with capture, loss,
  timer, supremacy, briefing, playable groups, callsigns, vehicles, freeze zones,
  final multiplayer checks and the official Workshop publishing handoff. Full
  npm run validate passed: Astro check, 11 normal routes, 13 isolated fixture
  routes, links/locales/assets, all 113 original hashes and browser matrix.
  Focused Chromium checks passed at 390/1440 px in both languages and themes:
  18 new headings/images, original hashes and new-tab links, rich-text example,
  in-page anchors and no overflow. Reviewed Russian mobile/desktop captures.
  Fixture validation preserved the normal build byte-for-byte; preview serves
  the completed pages. No live Workbench/multiplayer test, addon edits, commit
  or publication. Supporting-page expansion remains separate from this task.

- T078: added map markings, RedLine/Port settings and Vector Tool point editing
  to both mission pages with originals 91–95. Character guide links now return
  to map-markings. Production build passed (11 routes); Chromium checks passed
  at 390/1440 px in both locales for four headings, five loaded images and their
  original SHA-256 hashes, new-tab originals, return navigation and no overflow.
  Preview rebuilt. No addon assets changed or live Workbench test performed.

- T077: added group-catalog override/entry steps, Alpha search/placement check and
  return link to the main guide in both locales, with unchanged originals 87–90.
  Build passed (11 routes). Chromium checks passed at 390/1440 px in both locales:
  headings, four loaded images/original hashes/new-tab links, reference links,
  return navigation and no overflow. Preview rebuilt; no live game test or asset edits.

- T076: added both locales with eight original images, group inheritance/folder,
  translated name, military symbol, six-person roster and line formation. Operator
  requested only a by-analogy statement for remaining specialists. Build passed
  (11 routes); Chromium checks passed at 390/1440 px in both locales for headings,
  images loading/hashes/new-tab links, six table rows, anchors and no overflow.
  Preview rebuilt; no engine assets edited or live Workbench/game test performed.

- T075: added bilingual launch, lobby Play progression, U/Y controls confirmed by
  the operator, Tab/search/placement and appearance checks, with originals 76–78.
  Build passed (11 routes). Chromium checks passed at 390/1440 px in both locales:
  anchors, controls, three images loading/original hashes/new-tab links, mission
  link and no overflow. Preview rebuilt; instructions not executed in live game.

- T074: added bilingual catalog override/entry instructions for the two completed
  characters, with original screenshots 72–75 and explicit full-roster context.
  Build passed (11 routes). Chromium checks passed in both locales at 390/1440 px
  for hierarchy, four images loading, original hashes/new-tab links, reference link
  and no overflow. Preview rebuilt. No engine assets edited or runtime game test.

- T073: added both locales with six original screenshots, inheritance from ODA,
  launcher slot primary/index 1, M22 binoculars and translated name/ROLE_LEADER.
  Build passed (11 routes). Chromium checks passed at 390/1440 px in both locales:
  h2/h3 hierarchy, images loading, SHA-256 equality, new-tab originals, localization
  link and no overflow. Preview rebuilt; no engine assets edited or live test.

- T072: localization is now a separate h2 with four h3 steps; the sergeant retains
  three h3 steps. Production build passed (11 routes), and Chromium verified the
  exact heading hierarchy in both locales. Existing anchors and images preserved.

- T071: added role selection and the complete name-localization workflow in both
  locales, using unchanged originals 57–65. Production build passed (11 routes).
  Chromium checks passed at 390/1440 px in both locales for five headings, all nine
  new images loading, original SHA-256 equality, new-tab links and no overflow.
  Preview rebuilt. No addon assets changed or live Workbench test performed.

- T070: added both locales with sergeant inheritance, one-channel backpack radio,
  outfit changes, updated trouser/jacket storage and operator-confirmed backpack
  item cleanup. Originals 50–51 preserved but not displayed; 52–56 used unchanged.
  Build passed (11 routes); browser checks passed at 390/1440 px in both locales
  for three headings, five images loading, original hashes/new-tab links, updated
  storage paths and no overflow. Reviewed mobile capture. No engine assets edited.

- T069: added bilingual grenade slots, inherited inventory, jacket flashlight and
  four ammunition storage assignments, using originals 43–49. Operator delegated
  flashlight placement; retained screenshot workflow. Build passed (11 routes).
  Browser checks passed at 390/1440 px in both locales for five anchors, seven new
  images in intended order, loading/source hashes/new-tab links, and no overflow.
  Reviewed mobile capture. No engine assets edited or live Workbench test.

- T068: added both locales with current prefab naming, screenshot-name note, armor,
  webbing and primary weapon. Build passed (11 routes); focused browser checks
  passed at 390/1440 px in both locales for three headings, the closing reference
  order, five new images loading, new-tab originals with matching SHA-256 and no
  page overflow. Reviewed mobile capture. No engine assets edited or live test.

- T067: added three command definitions, unnecessary-duplication examples and a
  parent-change caution in both locales. Removed the repeated inheritance definition
  from the creation step. Build passed (11 routes); browser checks passed at
  390/1440 px for both locales, confirming section order, all three commands and
  no page overflow. No asset or component changes.

- T067 readability revision: rewrote the end reference around rifleman/medic
  examples and added a shared-base diagram using LL_OrgChart. Build and browser
  checks passed in both locales at 390/1440 px for three subsections, diagram
  structure and no overflow; reviewed the mobile capture.

- T066: added radio duplication/single transceiver, twelve initial items and watch
  in both locales with originals 31–37. Build passed (11 routes). Focused Chromium
  checks passed at 390/1440 px in both locales for three new anchors, all seven
  images loading, original SHA-256 equality, new-tab links, inventory quantities
  and no page overflow. Preview rebuilt; no engine assets edited or live test.

- T065: added both locales with operator-confirmed folder creation/drag-and-drop,
  Edit Prefab, loadout slots, five outfit assignments and save. Production build
  passed (11 routes). Browser checks passed at 390/1440 px for both locales:
  four headings, six new screenshots loading, new-tab originals with matching
  SHA-256, three table rows, and no page overflow. Reviewed mobile capture.
  All 113 original screenshot hashes passed. No addon edits or live Workbench test.

- T064: added bilingual base-prefab creation with original 23/24, inheritance explanation and filename meanings. Build passed (11 routes); Chromium checks passed in both locales at 390/1440 px for the new heading, image loading, original SHA-256 equality, new-tab links and no overflow. Preview rebuilt. No live Workbench execution or addon asset edits.

- T063: revised both locales and rebuilt successfully (11 routes). Rendered-output checks passed for three diagrams, twelve split-team roles, the research tip, removed military/group-prefab links, the 2003 example, and preserved advanced faction-guide link. No component or asset changes.

- T062: replaced the broken flow chart/examples with a built-in caution and removed the photo from both pages. Removed unused flow styling. Production build and focused 390/1440 px browser checks passed for both locales: two examples without links, no displayed photo, three preserved diagrams and no overflow. Original photo remains in source assets; references remain in research.md.

- T061: added bilingual historical/unit guidance, four semantic diagrams, and the unchanged supplied photo. Verified base HQ/Alpha/Bravo role lists against the pinned example and distinguished world-instance adaptations. Full website validation passed (11 normal/13 fixture routes, 113 original screenshot hashes); focused browser checks passed at 390/1440 px in both themes/locales for headings, diagrams, 12 split-team roles, loaded photo and no overflow. Reviewed desktop/mobile captures. Final build and output checks confirmed diagram structure and supplied photo SHA-256 after formatting cleanup.

- T060: created both introductory pages and linked the main walkthrough, homepage and sidebar. Full validation passed with 11 normal routes, 13 fixture routes, and all 113 original hashes. Focused 390/1440 px browser checks verified keyboard navigation from the mission guide, the official external link, naming content, no overflow, and matching-page language switching. Detailed prefab creation remains for joint authoring.

- T059: added bilingual map configuration with original 22.png and the operator-confirmed conditional addition of compass/watch components. Resource paths match the example mission. Build and 390/1440 px browser checks passed for both locales, including resource text, image loading/hash, new-tab originals and no overflow. Preview rebuilt; no live Workbench run or NavMesh changes.

- T058: added the bilingual replacement section and prefab definition with original images 20–21. Build passed; focused browser checks at 390/1440 px in both locales passed for the new anchor, image loading, original hashes, new-tab links and no overflow. Preview rebuilt. No addon assets changed; Workbench execution remains operator verification.

- T057: removed the wizard ending and added the mission-details section in both locales. Build passed; focused Chromium checks at 390/1440 px verified the anchor, five field rows, screenshot loading, original 19.png SHA-256, and no page overflow. Preview rebuilt. Content follows the official MissionHeader attributes and reference mission; no live Workbench test.

- T056: added the bilingual wizard section with original images 11–18. Full website validation passed. Removed navigation setup as requested by the operator and rebuilt successfully. Focused browser checks passed in both locales at 390/1440 px for four anchors, eight images loading, no overflow, and corrected copy. No live Workbench run was performed.

- T055: added world naming, folder structure, renaming concerns, and the optional Triad Tactics episode suffix convention in both locales. Production build passed; output checks confirmed the tip directly precedes screenshot 09, contains the examples, and replaces the menu save instruction with Ctrl+S. Original images unchanged.

- [x] T054 [US4] Write and verify bilingual mission-world creation from screenshots 05–10: editor launch, base world, sub-scene, project-local save, and systems layer. Confirm Rename layer interaction before writing that step. Validate original images, links, locale parity, and production preview (FR-014, FR-015).

- T054: added bilingual world creation with original screenshots 05–10 and the operator-confirmed right-click → Rename instruction. Full website validation passed, including all 113 original hashes, production links/assets, locale parity, and browser checks. Focused checks at 390/1440 px verified the five new anchors, six original images, image loading, new-tab originals, and no overflow in both locales. Production preview rebuilt. No live Workbench run was performed; wording remains subject to joint content review.
