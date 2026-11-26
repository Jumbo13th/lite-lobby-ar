# Implementation Plan: Game Master Only Player Notices

**Branch**: `006-gm-only-notifications` | **Date**: 2026-11-07 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/006-gm-only-notifications/spec.md`

## Summary

Route the game's join, leave, kick and ban notices to Game Masters only in Lite Lobby
missions. The game's notification sender component is the only sender of those
notices; a modded override of its two player handlers uses the game's own Game Master
only delivery instead of the broadcast and leaves every other notice, including the
Game Master status notices, as it is. Nothing new is replicated, and each disconnect
costs one message per Game Master instead of one per player.

## Technical Context

**Language/Version**: Enforce Script, Arma Reforger 1.9.0.37

**Primary Dependencies**: vanilla `SCR_NotificationSenderComponent.OnPlayerRegistered` and `OnPlayerDisconnected`, `SCR_NotificationsComponent.SendLocalUnlimitedEditor` and `SendToUnlimitedEditorPlayers` (research R1, R2), `LL_GameModeCoop.GetInstance()`

**Storage**: N/A

**Testing**: manual, demo world on a dedicated server ([quickstart.md](quickstart.md))

**Target Platform**: dedicated server for leave, kick and ban; each client for the join notice

**Project Type**: game addon, one modded class

**Performance Goals**: fewer messages than the unmodified game; no added per-frame work

**Constraints**: no replication, no setting, no change outside lobby game modes

**Scale/Scope**: one new script file, two changelog pages

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Result |
|-----------|-------|--------|
| I. Engine Fidelity | Both delivery paths are the game's own, used by the same component for other notices (research R2). The disconnect body is copied from the 1.9.0.37 method with only the three recipients changed. The lobby gate follows `LL_M_SCR_ReconnectComponent`. | PASS |
| II. Ask, Never Assume | The three open choices were put to the operator with recommendations; the spec records them under Assumptions for review in the pull request. The static-helper alternative was dropped because no official script shows it working (research R3). | PASS |
| III. KISS | One modded class, two overrides, no new component or state. | PASS |
| IV. YAGNI | No setting (FR-008). | PASS |
| V. Replication | Replicated values added: none. RPCs added: none. `RplSave`/`RplLoad`: unchanged. Visibility: the left, kick and ban notices become Game Master only and are filtered on the server before sending; the join notice is local on each client and never on the wire. | PASS |
| VI. Comments and Hygiene | One file-level WHY: why the notices are Game Master only and why the disconnect body is a copy. New file `scripts/game/Modded/LL_M_SCR_NotificationSenderComponent.c`; vanilla parameter names `playerId`, `cause`, `timeout` kept. | PASS |
| VII. Optional Integrations | Not touched. | PASS |
| Engine and Asset Constraints | No `.et`, `.layout`, `.conf`, `.acp` or `.meta` change. | PASS |
| User Interface | No screen change; the game's own Game Master notice marking is used. | PASS |
| Localization | No new string; the notices keep the game's text. | PASS |
| Documentation | Changelog entry in English and Russian under 1.1.2, the next Workshop version (1.1.1 is published). | PASS |
| Testability | Reproducible in the demo world with four clients on a dedicated server; admin login and the admin kick action are the controls. | PASS |

Workbench steps for the operator: compile. Asset files to double-check: none.

## Project Structure

### Documentation (this feature)

```text
specs/006-gm-only-notifications/
├── plan.md              # This file
├── research.md          # Phase 0: senders, Game Master delivery, hook choice
├── quickstart.md        # Phase 1: validation in the demo world
└── tasks.md             # Phase 2
```

No `data-model.md` and no `contracts/`: the feature has no data and no interface.

### Source Code (repository root)

```text
Lite Lobby/scripts/game/Modded/
└── LL_M_SCR_NotificationSenderComponent.c   # new: OnPlayerRegistered, OnPlayerDisconnected
docs/src/content/docs/
├── changelog.mdx                            # 1.1.2 entry
└── ru/changelog.mdx                         # 1.1.2 entry
```

## Design

1. `OnPlayerRegistered(int playerId)`: without `LL_GameModeCoop.GetInstance()`, call
   `super` and return. Otherwise
   `SCR_NotificationsComponent.SendLocalUnlimitedEditor(ENotification.PLAYER_JOINED, playerId)`,
   which shows the join only when the local player's editor is unlimited (FR-001). On
   the dedicated server there is no local editor and nothing is shown, as before.
2. `OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)`: without a
   lobby game mode, call `super` and return. Otherwise run the game's body unchanged
   (master check, Game Master left to everyone, kick-reason preset lookup, data
   collector ban timeout, kick versus ban versus left) and send `PLAYER_KICKED`,
   `PLAYER_BANNED_NO_DURATION`, `PLAYER_BANNED` and `PLAYER_LEFT` with
   `SendToUnlimitedEditorPlayers` and the same parameters (FR-002, FR-003, FR-004).
3. Rights are read when the event happens, by the game's own helpers, so a player who
   gains or loses Game Master rights is included or excluded from the next notice on
   (US2 scenarios 2 and 3).
4. Stages are not consulted: the handlers fire the same way in every stage (FR-005).
5. Every other handler of the component (kill feed, faction join, weather, editor
   status) is not overridden.

## Constitution Check (post-design)

Unchanged from the gate above. PASS.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|--------------------------------------|
| Copied vanilla method body | The recipients sit inside the game's disconnect handler and `super` would broadcast first | A receiving-side filter keeps the broadcast (FR-007); a static helper override is not shown working in the official scripts |
