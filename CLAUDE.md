# CLAUDE.md

Instructions for AI coding agents working in this repository.

## What this is

Lite Lobby, an Arma Reforger addon: slot selection, briefing map, freeze time, voice rooms,
spectator and mission triggers for cooperative scenarios with up to 127 players. The addon lives
in `Lite Lobby/` (`addon.gproj`, scripts under `scripts/game/`, one folder per system). Scripts
are Enforce Script (`.c`), prefixed `LL_`; modded vanilla classes are `LL_M_<VanillaClass>.c`.

## This project is spec-driven

Development follows Spec Kit. The skills are in `.claude/skills/`, the specifications in
`specs/`, and the constitution in [.specify/memory/constitution.md](.specify/memory/constitution.md).

**When the author asks for a feature or a behaviour change, do not write code. Propose
`/speckit-specify` first**, then run the phases in order: specify, clarify, plan, tasks,
implement. Only the author can waive the process, and only for that one request. The size of
the change is never a waiver.

## Read the constitution before anything else

It is the authority on how this addon is built: engine constraints, replication rules, what
agents may and may not touch, comments, localization, and what the author does in Workbench.
Nothing here overrides it.
