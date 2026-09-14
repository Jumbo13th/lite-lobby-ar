# Feature Specification: Lite Lobby Guide Site

**Feature**: 002-guide-site
**Status**: Implemented; publication review in progress.

## Scope

A bilingual illustrated guide for mission makers who have not used Arma Reforger
Tools. Programming experience is not required. The site teaches one mission from
opening Workbench through Workshop publication. It also provides supporting guides
and server-specific setup and requirements.

Website code and content live in docs/. This feature owns its specification,
GitHub workflow and agent entry instructions. It does not change the game addon,
shared SDD integrations or templates. API documentation is outside this feature.

## Reader journeys

1. A new reader opens either language homepage, follows the mission course and
   completes each chapter before starting the next. Chapter sections explain the
   actions, prerequisites and expected results beside relevant screenshots.
2. A reader opens a finished mission alongside the course and consults the chapter
   for the setting they want to understand. Supporting pages link back to the course.
3. An author with a completed mission checks server requirements and applies the
   server-specific configuration. General Lite Lobby instructions stay in the course.
4. A maintainer installs the supported toolchain, validates both languages and
   submits changes. A successful main-branch workflow publishes the validated site.

## Functional requirements

- **FR-001**: Codex and Claude share the existing Spec Kit skills and specifications; Claude remains the default integration.
- **FR-002**: AGENTS.md points to the constitution and shared instructions; maintenance follows the approved feature specification and plan.
- **FR-003**: Every reader-facing page has English and Russian versions, with translated navigation and a language switcher.
- **FR-004**: The homepage exposes the entries and destinations defined in [the site contract](contracts/site.md#homepage). Project introduction and help are outside that list.
- **FR-005**: Navigation, search, theme switching and community links remain keyboard-accessible at desktop and mobile widths. Header controls do not overlap.
- **FR-006**: Every retained screenshot has an inventory entry and is used by a page. Moving an image preserves its bytes and updates all references.
- **FR-007**: Figures have translated alternative text, captions and original-image actions. Images are responsive and lazy-loaded; opening an original preserves the reading position.
- **FR-008**: Local development and CI use the same supported Node major and dependency policy from [quickstart](quickstart.md#toolchain-and-dependencies). The committed lockfile supports npm ci.
- **FR-009**: Pull requests validate the website. Successful relevant pushes to main publish it through GitHub Pages. Manual runs can validate or publish main.
- **FR-010**: Publication contains only reader pages and required assets; specifications, addon source, tests and temporary fixtures are excluded.
- **FR-011**: Website maintenance leaves game assets unchanged. Deployment is restricted to validated main-branch content.
- **FR-012**: The homepage uses aligned rows in a bounded reading area without article sidebars. Article pages retain chapter navigation and a contents panel.
- **FR-013**: Headings, lists, tables, asides, code and captions use consistent typography and spacing in both themes. Temporary fixtures exercise these elements without entering publication.
- **FR-014**: The mission course has one introduction, three parts and eight sequential chapters. On-page sections continue work on the same mission; chapter boundaries follow completed tasks.
- **FR-015**: Supporting guides own their detailed procedures. Contextual links connect them to the course without duplicating instructions. Russian uses plain technical-book prose, concrete verbs and brief explanations of unfamiliar terms.
- **FR-016**: Validation detects broken links, locale-crossing article links, missing images and output contamination. Failed or interrupted runs close their processes and remove only their own temporary content.
- **FR-017**: The Git guide covers repository creation, addon folders, ignore rules, clear commit messages, frequent commits, pushes and recovery. It distinguishes saving files, committing locally and pushing to GitHub.
- **FR-018**: A localized help panel follows the content, links to the configured Discord and is excluded from search indexing.
- **FR-019**: The homepage introduction retains the project attribution, source link and official APL link and description.
- **FR-020**: The mobile homepage menu exposes language, theme and community controls; Escape, outside clicks and returning to desktop close it. Article menus retain native navigation.
- **FR-021**: Empty native footers are omitted. When the help panel is last, the main content box ends within 32px of it without creating unnecessary page scroll.

## Acceptance criteria

- **SC-001**: Both homepages contain four ordered entries and four unique topic anchors; all destinations resolve.
- **SC-002**: At 390, 768, 1440 and 1920 px, both languages and themes have usable controls and no horizontal overflow.
- **SC-003**: All 141 retained screenshots match the inventory's byte lengths and SHA-256 hashes; translated pages retain matching image order.
- **SC-004**: Normal builds contain no fixture routes or internal project files, including after a failed or interrupted validation.
- **SC-005**: Shared SDD integrations and templates remain unchanged.
- **SC-006**: Clean installation and the complete validation command pass on the supported runtime; failed checks cannot publish.
- **SC-007**: Homepage rows align, article navigation identifies the current chapter and previous/next links follow the single course sequence.
- **SC-008**: Browser checks cover original-image navigation, search, menus, theme/language controls and representative article elements.
- **SC-009**: The course and supporting pages contain complete instructions in both languages without placeholders.
- **SC-010**: Every active page appears in the appropriate navigation; localized heading anchors agree and supplementary pages link back to the course.
- **SC-011**: CI retains failure evidence as an artifact and uploads only normal production output for publication.

## Constraints

Community-specific guide titles and website tooling are explicitly authorized for
this feature despite the constitution's general naming restriction. The existing
license wording, named addon recommendations, Workshop example and Russian use of
«зона» remain unchanged. No repository license file is added by this feature.

Engine steps come from the existing mission example, official sources or operator
confirmation. Do not infer new Workbench behavior or alter addon assets. New material
requires corresponding English and Russian content and review of the actual steps.
