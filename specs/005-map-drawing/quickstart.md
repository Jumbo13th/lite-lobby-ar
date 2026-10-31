# Validation: Shared Map Drawing

For the operator after implementation. Code integration, static checks and Workbench compilation are complete. In-game appearance, resource review and dedicated-server checks remain pending.

## Prerequisites

Accept the plan and resolve the sanctioned official source reference. Use the existing LobbyDemo mission. Configure a dedicated server and clients with two factions; include a same-faction late joiner. Use existing slot selection and hard-freeze controls.

## Source and Resource Review

1. Compare the ported core against the supplied implementation after normalizing prefixes and modded file placement. Geometry, document storage, spatial indexing, circle/anchor construction, rendering math, revision sequencing and history grouping must have no algorithm change.
2. Review the final changed .conf/.meta/.layout list in Workbench. No .et or resource-database edits are planned. Reload stale buffers; restart Workbench fully after input changes and compile scripts.
3. Verify generated language files match the source string table through the repository's locale generator.

## Map and Appearance

1. In English and Russian, exercise all tools, color presets and custom RGB on the demo map. Erase across every shape and inspect authors. Compare appearance with the existing insert-marker dialog.
2. Pan, zoom and reopen; geometry must keep its map position. Complete twenty briefing/game/spectator map cycles; check duplicate buttons, leftover windows and listeners, cursor visibility and stuck drags.
3. Check layouts at 1080p and 1440p with default and increased UI scale. Tool order, selected outline, disabled state, readable history and text fit must match the UI contract.
4. When drawing is off, verify the existing marker dialog and Shift marker-age behavior. With drawing enabled, verify digits, C, temporary modifiers and release restoration; marker dialogs must stay closed. Typing suppresses shortcuts.
5. Scroll history and descriptions without map zoom; wheel over the map still zooms.

## Authority and Synchronization

1. Draw and erase with two same-faction clients; compare results with a late joiner and reconnecting client. Switch factions and verify the previous drawing/history disappears.
2. Confirm opposing clients receive no private drawing or history data, using network inspection or scoped server instrumentation; a hidden enemy drawing is not sufficient evidence.
3. Test no faction, destroyed slot, spectator map, respawn and hard freeze. Check client refusal and server rejection of edit requests; drawing must remain unchanged.
4. Exercise history tool changes, ten-second grouping, multiple erased authors, five-minute expiry and frozen snapshots.
5. Exercise point-limit rejection, erasing to free capacity and snapshot loading during edits. Use existing geometry/replication validation where applicable.
6. Repeat shared editing, late join and faction isolation in a dedicated session with 127 occupied slots. Record actual conditions and failures; do not claim this check from local hosting.

Report source checks, compilation, visual review, multiplayer checks and scale checks separately. Any operator-only check not run remains pending.

## Implementation Checks Completed

- Twelve core modules match after mechanical prefix normalization: anchor tool/style/geometry, canvas, circle geometry, document, erasing geometry, renderer, replica/protocol, spatial reference, stroke renderer and tool state.
- Record, erased-target collection, entry insertion, pruning and snapshot creation retain their exact history method bodies after naming normalization.
- All eleven drawing RPC signatures retain their argument lists. Server edit rejection acknowledges the preview token.
- Forty-four drawing types resolve; edited resource/script braces and duplicate method checks pass; the input override has no duplicate actions or object IDs.
- Twenty-four drawing localization keys resolve in all thirteen regenerated runtime tables; English and Russian values are present in the source string table.
- Git whitespace checks pass. These checks do not compile Enforce Script or demonstrate runtime appearance.

## Exact Resource Review List

Open and double-check these resources in Workbench; reload them if an old buffer is open:

- `Lite Lobby/Configs/System/chimeraInputCommon.conf`
- `Lite Lobby/Language/ll_localization.st`
- `Lite Lobby/Language/ll_localization.cs_cz.conf`
- `Lite Lobby/Language/ll_localization.de_de.conf`
- `Lite Lobby/Language/ll_localization.en_us.conf`
- `Lite Lobby/Language/ll_localization.es_es.conf`
- `Lite Lobby/Language/ll_localization.fr_fr.conf`
- `Lite Lobby/Language/ll_localization.it_it.conf`
- `Lite Lobby/Language/ll_localization.ja_jp.conf`
- `Lite Lobby/Language/ll_localization.ko_kr.conf`
- `Lite Lobby/Language/ll_localization.pl_pl.conf`
- `Lite Lobby/Language/ll_localization.pt_br.conf`
- `Lite Lobby/Language/ll_localization.ru_ru.conf`
- `Lite Lobby/Language/ll_localization.uk_ua.conf`
- `Lite Lobby/Language/ll_localization.zh_cn.conf`

Fully restart Workbench for the merged input context, then compile scripts. Run Resource Manager → Plugins → Lite Lobby → Validate Map Paint (`LL_MapPaintTests`). Run LobbyDemo and the visual/multiplayer scenarios above. No new prefab component or entity placement is required by the implementation.

## Compilation Result

On 2026-10-31, the installed diagnostic Workbench 1.8.0.13 compiled the addon with `-wbSilent -noThrow` and returned exit code 0. The Game and WorkbenchGame modules loaded successfully with no script errors. The tooltip lookup helper was moved from the event handler to the panel class that calls it. Existing obsolete-API warnings remain. This check does not establish runtime behavior or compatibility with a different engine version.
