<!--
Sync Impact Report
- Version: 1.0.0 (initial ratification)
- Written after the open-source cleanup of 2026-09-07/08, from the operator's standing
  instructions and the engine lessons that shaped the addon.
- Templates requiring updates: none; plan-template.md's constitution check reads this
  file at runtime.
- Follow-up TODOs: none.
-->

# Lite Lobby Constitution

Lite Lobby is an Arma Reforger addon written in Enforce Script under the `LL_` prefix. It
gives a mission the classic flow: slot selection, map briefing, freeze time, spectator,
voice rooms, triggers. `ARCHITECTURE.md` at the workspace root owns the entity map and the
replication contract; this constitution owns how the code is written. Where the two
overlap, the architecture document is more specific and this document is more binding.

## Core Principles

### I. Engine Fidelity — Never Invent Arma Code

Every engine call, component, attribute, event, and replication construct MUST be
traceable to one of these sources, in this order of authority:

1. The official Arma Reforger scripts and game data for the pinned game version
   (`Arma-Reforger-Script-Diff/`, tagged per release; the addon targets 1.8.0.10).
2. The official Enfusion documentation for replication, components, serialisation,
   configuration and the REST client.
3. The vanilla game modes inside those official sources (Game Master, Conflict, the
   deploy menu, the map, the VoN menus) for how a screen, a manager or a player flow is
   assembled in this engine.

Rules:

- If a pattern does not exist in those sources, it is not known to work. Do not
  extrapolate an API from its name, from another engine, or from how it "should" behave.
- Before implementing any feature, search the official scripts for the vanilla
  equivalent (a manager, a user action, a serializer, a UI flow) and follow its shape.
  Behaviour lives in `scripts/`, structure in `GameData/`; a UI task needs both halves.
- Code that runs in production on a 127-slot server is copied as it is. Do not "fix",
  restyle or improve a pattern that already works; its quirks may be what makes it work.
- Never browse the game installation, the workshop cache, or the player profile
  directory. The in-repository sources above are the only sanctioned references.

Rationale: two earlier attempts at this lobby died on assumptions about how the
replication layer behaves. The engine is the ground truth, and the game scripts are the
only complete documentation of it.

### II. Ask, Never Assume

When a technical question cannot be answered from the sources in Principle I, STOP and
ask the operator. Do not guess, do not pick silently, do not reinvent.

- Browse existing code first, then present the options found with arguments for each,
  and let the operator decide.
- This applies to every technical decision: engine APIs, Workbench behaviour, an
  attribute that looks unused, anything that looks wrong in code that currently works.
- A thirty-second question is always cheaper than a debugging session.

### III. KISS — Keep It Simple

One state machine (`SLOTSELECTION → BRIEFING → GAME → DEBRIEFING`), one lobby manager
that owns the data, one player component that is the only client-server channel.

- Choose the simplest design that satisfies the feature. Complexity MUST be justified in
  the plan by a concrete need, never by "flexibility".
- No abstraction layers, base classes, registries or plugin systems with a single
  concrete use.
- Fewer features done well beat many features done poorly. Dropping a feature is a valid
  answer, and the correct one, whenever implementing it would harm network performance
  at 127 slots. Present the trade-off; the operator decides.

### IV. YAGNI — Build Only What Is Required

Nothing is implemented unless an accepted feature specification requires it.

- No speculative configuration knobs, "reserved for future" fields, optional code paths
  or generalisations for scenarios nobody has specified. A designer attribute exists
  because a mission maker needs it, and every attribute is read somewhere.
- No helper, utility or convenience API that has no caller in the current task.
- An agent that sees a "useful" addition outside the task MUST mention it and MUST NOT
  build it.

### V. Server Authority and Replication Discipline

The dedicated server is authoritative for every decision. Replication is the scarcest
resource in this project and every byte on the wire is a design decision.

- Clients request; the server validates every argument and permission and decides. The
  client never supplies a trusted slot, faction, player id, admin flag or stage.
  Server-only methods carry the `_S` suffix. Admin actions are gated on the server with
  `SCR_Global.IsAdmin`, never only in the UI.
- Follow the vanilla replication model: `[RplProp()]` for scalars only, RPCs
  (`RpcAsk_*` client to server, `RpcDo_*` server to clients) for deltas,
  `RplSave`/`RplLoad` for join-in-progress. Never `[RplProp()]` on a collection, and
  never a codec whose comparison always answers "changed".
- `Replication.BumpMe()` is called on the authority only, after changing a replicated
  scalar, and never in a loop.
- RPC methods take at most 8 arguments; split beyond that, and do not send fields that
  are guaranteed defaults.
- Per-entity replication multiplies by the entity count. Lobby state is centralised in
  the game-mode components; playable characters, vehicles, zones, markers and
  description entities carry zero replication of their own beyond what vanilla provides,
  unless a plan justifies it.
- Classify every payload by visibility before implementing it: public, the requesting
  player, faction-scoped, admin-only, local-only. Faction-scoped data is filtered on the
  server before it is sent; a client-side filter over a broadcast is a leak.
- Bulk state (a full slot list, a season table, a statistics view) is sent as one
  chunked, versioned transfer after connect or on publish, never as a continuously
  changing property.
- `RplSave` and `RplLoad` MUST mirror each other exactly; a mismatch corrupts silently.
  Arrival order is not deterministic; sort by a server-assigned key, never by arrival.
- Anything keyed by player id is initialised from `OnPlayerConnected`, not from component
  init, and reconnect identity is the identity GUID, never the transient player id.
- Instrument first, then change, then measure at the target player count on a dedicated
  server. One replication change per test.

### VI. Comments and Code Hygiene

Code explains itself; comments exist only for what code cannot show.

- Comments are written at the level of public members: a class, a public method, an
  editor attribute, a replicated property. Not on private helpers, not inside method
  bodies, not above self-explanatory lines.
- A comment states a constraint or a WHY that the reader cannot recover from the code:
  an ordering requirement, an engine quirk, a replication cost. One or two lines. Walls
  of text, narration of the next statement, section banners, labelled "WHY:" or "NOTE:"
  blocks, editorial judgements, progress notes and commented-out code are forbidden.
- Comments, identifiers, documents and specifications name only the official Arma
  Reforger scripts. No other addon, project, community, server, website or person is
  named, credited or described as a source anywhere in this repository, this file
  included. Write the actual reason instead of "as done elsewhere". Author credit lives
  in the scenario header and nowhere else.
- Every type, method, prefab, layout and config authored by this project carries the
  `LL_` prefix. Modded vanilla classes live in `scripts/game/Modded/` as
  `LL_M_<VanillaClass>.c` and keep vanilla parameter names in overrides exactly.
- No dead code, no dead attributes, no legacy systems kept "in case", no `_OLD` folders,
  no one-shot tools left behind after they ran.

### VII. Optional Integrations Stay Optional

The addon must run a mission for any community out of the box. Website-backed features
(registration gate, unit statistics, slotting import/export) are integrations, not the
product.

- Every integration is off until the server owner writes its `$profile:` configuration.
  Endpoints, secrets and page URLs are configuration, never attributes and never code.
- Unconfigured, an integration is inert: no kicks, no requests, no screens, no log spam.
  Its chat commands answer "not enabled".
- The protocol an integration speaks is documented in the code that speaks it, as a
  handful of endpoints and JSON shapes any site can implement.
- A test mock is a prefab checkbox that only works when the real configuration is
  absent, warns loudly in the log, and is never shipped ticked.

## Engine and Asset Constraints

Scripts own behaviour. Workbench owns prefabs, resource GUIDs and audio-graph nodes.

- `.et` prefabs are NEVER created or edited by an agent. The operator builds them in
  Workbench. An agent hands over the exact components, attributes and values to set.
- `.layout`, `.meta` and `.conf` files may be written by an agent. Every such file is
  listed in the handover for the operator to open and double-check in Workbench, and
  the operator is told to reload it if Workbench holds a stale buffer.
- The engine's override mechanism (Workbench "Create Override", same path, the base
  game's GUID) is the sanctioned way to add components to vanilla prefabs and entries to
  system configs. A standalone file with a fresh GUID at the same path builds fine and
  silently does nothing.
- Never override a global vanilla resource that changes behaviour for every mod on the
  server, the vanilla VoN audio graph above all. A feature whose only implementation
  path is such an override is not implementable here; propose dropping it.
- Never hand-author nodes in `.acp` audio graphs; hand-written nodes render but are dead
  at runtime. Attribute and wire edits on Workbench-born nodes, and verbatim copies of
  Workbench-born blocks, are allowed.
- Workbench owns resource GUIDs. Scripts and configs reference the GUID that is actually
  in the `.meta`; when Workbench reassigns one on import, re-read the `.meta` and re-sync
  every reference rather than fighting the tool.
- Every object id in hand-authored `.layout` and `.conf` content is freshly generated
  and unique, never copied from another addon. Ids are copied only when an include must
  address an existing vanilla object.
- Modded `BaseContainerProps` script objects repeat the decorator and base class, or the
  prefab's component list silently truncates.
- `Visible 0` in a `.layout` silently drops the node's children; hide from script.
  `FrameWidgetSlot` right and bottom offsets are subtracted from the anchor.

## User Interface

Every screen this addon ships is used by players who already know Arma Reforger. It
looks and behaves like the game.

- Screens are built from vanilla widgets and vanilla `SCR_*` UI components (button
  bases, edit boxes, combo boxes, scroll layouts) so they inherit focus, controller
  navigation, sounds and scrolling. Extend `SCR_ButtonBaseComponent`, never the
  deprecated `SCR_ButtonComponent`.
- Before designing a screen, open the vanilla layout that does the closest job and
  follow its structure and interaction grammar.
- The same action sits in the same place on every screen; the same state looks the same
  everywhere; nothing that matters is conveyed by colour alone.
- A screen is reviewed in the running game, at the game's resolution and scale, before
  it is called done: alignment, clipping, truncated strings in both languages, keyboard
  focus, Escape behaviour, and behaviour with 127 slots on the list.

## Localization and Language

- English is the source language; Russian is the first target and ships with every
  string. Other languages show English by design.
- `Language/ll_localization.st` is the source of truth; the runtime tables are
  regenerated with `tools/generate-runtime-locales.ps1` after every edit and never edited
  by hand. Keys are `LL-<Area>_<Name>`, values single-line, each item with a fresh GUID.
- No user-visible string is hard-coded. Server-to-client messages carry the key; the
  client translates in its own language. Composed strings translate their parts with
  `WidgetManager.Translate` first. Prefer whole-sentence keys with parameters over glued
  words.
- Russian is written as native Russian, the way players speak, never as a rendering of
  the English sentence. Community terms are used as the community uses them: «фризтайм»
  for freeze time, «триггер» not «зона», «тимкилл» not «свой»; gaming loan words are
  transliterated (вайп, респавн, лут, вайтлист, кулдаун), never replaced with descriptive
  Russian. Dynamic names are wrapped in case-safe constructions, never declined.
- Not localized: widget names, resource paths, faction keys, chat command words,
  player-typed content, log lines, Workbench attribute descriptions.

## Documentation

- No document is written unless the operator asks for it. Direction goes into the next
  specification, setup into the existing README, decisions into the agents' knowledge
  folder outside this repository. Roadmaps, next-steps files, handoff notes and review
  files are not repository content.
- When documentation is requested, the first document is the mission-maker guide, and
  every document says who needs it and when, or it is not written.
- One source per fact. A rule, a term or a value lives in exactly one place; other
  places link to it.
- Every summary of work, whether a handover message, a commit message or a pull request
  description, uses this format and nothing more:

  ```text
  Changes:
  - Short change A
  - Short change B
  ```

  One line per change. No preamble, no rationale essay, no headings, no walls of text.
  A description that needs more than the list is a sign the change should be split.

## Open Source Standards

This repository is public. Everything committed is read by strangers who never saw the
conversation that produced it.

- Nothing private leaks: no credentials, tokens, server addresses, player data,
  identity GUIDs, machine-local paths or account names, and no reference material the
  project does not own.
- No AI slop. Every file, paragraph and comment earns its place by telling a reader
  something they need. Forbidden: boilerplate and filler, placeholder sections, hedging,
  inflated adjectives, emoji, decorative headings, badges, generated changelogs and text
  written to look thorough rather than to be read.
- The repository stays clean: no legacy artefacts, no superseded files, no scratch
  notes, no drafts next to the thing they became. When something is replaced, its
  predecessor is deleted in the same change; git history is the archive.
- Commits are authored by the operator's identity and carry no tool attribution.

## Testability and the Demo World

There is no automated way to test game functionality. The demo scenario
(`Missions/LobbyDemo.conf` on `worlds/Arland/LobbyDemo.ent`) is the acceptance
environment for everything, and it must keep working for a stranger with no
configuration.

- Every feature MUST be reproducible in the demo world; the plan names the entities,
  attributes and admin commands it needs there. A feature only observable on a live
  event server with real players is not testable and is not done.
- Replication, voice and reconnect behaviour are verified on a dedicated server, never
  only in Workbench or a listen host: local hosting masks the bugs that matter.
- Scale-sensitive changes are measured at 127 slots before they are called done.
- Admin chat commands and Game Master actions are the test controls; they follow the
  same authority rules as player requests (Principle V).

## Development Workflow

- The project is Spec Kit driven. Every feature moves through the phases in order:
  specify, clarify, plan, tasks, implement, with analyze and checklist wherever the
  templates call for them. No code is written for a feature without an accepted
  specification and plan, and no task is implemented that is not in the task list.
- Every specification starts by evaluating the current state of the addon: what already
  exists, what the engine has since proven or disproven, and whether the feature still
  makes sense as asked. Ambiguities are collected as questions for the operator, not
  resolved by assumption (Principle II).
- Every plan carries a constitution check that classifies every new replicated value by
  visibility and mechanism (Principle V), names the vanilla pattern it follows
  (Principle I), and lists the Workbench steps and the asset files the operator must
  perform or double-check.
- Audits and reviews have done criteria stated before they start: the principles they
  check, the artefacts they cover, the finding types they report. They run once over
  that scope, report, and stop. No "one more pass", no widening mid-audit.
- The operator imports assets, builds prefabs, places entities, compiles and publishes.
  Agents hand over a precise list of those steps with every feature.
- Never commit, push, tag or otherwise change repository history without the operator's
  explicit permission for that specific change. Staging is fine; committing is not.

## Governance

This constitution supersedes every other practice in the repository. Feature
specifications, plans and task lists MUST be checked against it, and a plan that violates
a principle is rejected or amended before implementation starts.

Amendment procedure: propose the change with its rationale, update this file, record the
change in the Sync Impact Report at the top, and bump the version:

- MAJOR — a principle removed or redefined in a backward-incompatible way.
- MINOR — a principle or section added, or guidance materially expanded.
- PATCH — clarifications and wording that do not change meaning.

The operator ratifies every amendment.

**Version**: 1.0.0 | **Ratified**: 2026-09-08 | **Last Amended**: 2026-09-08
