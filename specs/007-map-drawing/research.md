# Research: Map Line Drawing

Every decision names the official script or Lite Lobby code it follows. Paths under
`scripts/` and `GameData/` are in the 1.9.0.37 script dump; `Lite Lobby/...` paths are in this
repository.

## R1. Drawing a thick, semi-transparent line

- **Decision**: one `CanvasWidget` per map screen and two `LineDrawCommand`s per line: the band
  with all its points at 55% alpha, then its outline as one closed loop along both edges (each
  vertex offset by half the width along its mitre, capped at sharp turns), 2 px wide, in the
  colour at 53% brightness and 80% alpha (the glyph outline is the fill at 53%: 231→123,
  236→122). An outline drawn under the whole band was tried first: the two alphas stacked to
  about 94% and hid the map. The start point is
  not drawn on the canvas: it is an `ImageWidget` per shown line, loaded with
  `LoadImageFromSet(0, imageset, quad)` from the Dot entry of the placed-marker config, tinted with
  `SetColor` and sized from that entry's `GetIconSize()` (or the marker layout's 56 px when 0),
  which is exactly how `LL_MapMarkerEntryPlaced.InitClientSettings` and
  `SCR_MapMarkerWidgetComponent.SetImage`/`SetColor` draw a placed marker; flags as vanilla's
  script-made images (`BLEND | VISIBLE | STRETCH`, plus `IGNORE_CURSOR | NOFOCUS`). The canvas is
  the existing `UI/Map/ZoneCanvas.layout`, created in the map frame when the map opens and given
  Z order -450; start points sit at -445, labels at -440, markers at 0.
- **Why an edge loop**: `LineDrawCommand` has outline fields, but no official script sets them to
  anything but 0, so how the engine draws them is unknown; plain lines are the known path.
- **Rationale**: `LineDrawCommand` (`scripts/Core/proto/EnWidgets.c`) draws a whole polyline with
  a width in one command, so 150 lines are 300 commands, not thousands of widgets. Polyzones
  already draw on that canvas layout in the map frame, projected with `SCR_MapEntity.WorldToScreen`
  (`Lite Lobby/scripts/game/Zones/LL_MapZoneComponent.c`). `GameData/UI/layouts/Map/Map.layout`
  puts its drawing layers below the default Z order of marker widgets (`DrawingWidget` -500, the
  pencil's `DrawingContainer` -400); -450 keeps lines above the map and below markers.
- **Alternatives considered**: one rotated image widget per segment, as the game's pencil does
  (`scripts/Game/Map/ComponentsUI/SCR_MapDrawingUI.c`): up to 9,600 widgets at the limits.
  Drawing into the map's own `DrawingWidget` canvas: shared with the game, whose commands would
  replace ours.

## R2. Width fixed on the ground, chosen from the zoom

- **Decision**: each click of a line in progress sets the whole line's ground width to
  `0.02 × screen height (px) / SCR_MapEntity.GetCurrentZoom()`, capped at 50 m; between clicks
  it is drawn like a finished line, and Enter sends the latest value. A stored line is
  drawn `width × GetCurrentZoom()` pixels wide, never below 1 px. The start dot's radius is 0.75 ×
  the width.
- **Rationale**: `GetCurrentZoom()` is pixels per metre (`scripts/Game/Map/SCR_MapEntity.c`), so
  a ground width times the current zoom is exactly "scales with the map like a road".
- **Projection cost**: `WorldToScreen(x, y, withPan = true)` is
  `(x - offsetX) × ppu + pan` and `(sizeY - y + offsetY) × ppu + pan`. The UI projects the world
  origin once per frame and computes every point as `origin + (x × ppu, -y × ppu)`, so the frame
  costs one engine call, not one per point. Vertices are rebuilt only when the zoom, the pan or
  the set of lines changed, and a line whose cached bounding box lies outside
  `GetMapVisibleFrame` is skipped.

## R3. Starting, finishing and cancelling a line

- **Decision**:
  - Start: the map's `MapSelect` action (a click: `InputFilterClick`, 200 ms) while a new action
    `LL_MapLineStart` (Left or Right Ctrl, polled with `GetActionValue`) is held.
  - While a line is in progress, the UI activates a new context `LL_MapLineContext` every frame
    (Priority 60, Flags `0xc`, as `MapMarkerEditContext`). It defines `LL_MapLineFinish` (Enter,
    numpad Enter) and `LL_MapLineCancel` (Escape), and references only the map's navigation
    actions (`MapSelect`, `MapWheelUp`, `MapWheelDown`, `MapZoomIn`, `MapZoomOut`, `MapPanDrag`,
    `MapPanH`, `MapPanV`, `MapCursorX`, `MapCursorY`, `MapDrag`), `LL_MapMarkerInfo` and `VONDirect`.
- **Rationale**: the vanilla marker editor and the lobby's insert-marker dialog take the keyboard
  the same way: `ActivateContext("MapMarkerEditContext")` each frame, a Priority 60 context that
  suppresses the menu's own map context (`LL_MapMarkersUI.Update`). Without `ChatToggle` and
  `MenuBack` in the active context, Enter cannot open the chat and Escape cannot close the map
  (`LL_BriefingMapMenu` and `LL_GameMapMenu` listen to both). `VONContext` has Priority 110 and
  is not suppressed; `VONDirect` is referenced because the lobby maps carry it in
  `LL_BriefingMapContext`. `MapSelect` is a click filter, so a drag that pans adds no point.
  Holding a key and polling it is how the marker-info key already works (`LL_MapMarkerInfo`).
- **Risk**: what the `Flags` bits mean is not documented in the sources; the value is copied
  from the context known to work. Quickstart step 3 verifies it.
- **Alternatives considered**: guarding the map screens' Escape and chat listeners while a line
  is in progress: three screens to patch, and every future listener on the map context would
  fire mid-line.

## R4. Transport: who sends what

- **Decision**: all requests and deliveries go through `LL_LobbyPlayerComponent`, the only
  client-server channel. The server stores lines in a plain class owned by `LL_LobbyManager` on
  the authority. A confirmed line goes to each player of its faction with an owner RPC; nothing
  is broadcast.
- **Rationale**: lines are faction intel. `ARCHITECTURE.md` makes the exception explicit: when
  filtering protects information, gate on the server. The marker gate already sends a faction's
  markers one owner RPC per entitled player (`LL_M_SCR_MapMarkerManagerComponent.OnAskAddStaticMarker`
  → `LL_LobbyPlayerComponent.SendStaticMarker_S`). A plain class needs no prefab change.
- **Arrays in RPCs**: `array<int>` and `array<float>` are RPC parameters in the official scripts
  (`SCR_VoiceoverSystem.RpcDo_PlaySequenceFor`, `SCR_DataCollectorCommunicationComponent.Rpc_DoSendData`).
- **Alternatives considered**: `RplSave`/`RplLoad` for join-in-progress: one snapshot for every
  client, so faction-scoped data would leak (Principle V). A new game-mode component: needs the
  operator to edit the game-mode prefab for no gain.

## R5. Point encoding and validation

- **Decision**: points travel as `array<int>` of whole decimetres, x then y, two numbers per
  point. The server refuses a request with fewer than 2 or more than 64 points, any point outside
  the world's bounding box, a colour index outside the placed-marker colour list, or a width that
  is not between 0.1 m and 50 m.
- **Rationale**: decimetres need no map-dependent packing that both sides would have to agree on.
  `world.GetBoundBox` is the vanilla way to bound positions
  (`scripts/Game/Camera/Components/SCR_LimitBoxManualCameraComponent.c`). The colour list is the
  placed-marker entry every machine builds identically (`LL_M_SCR_MapMarkerManagerComponent.OnPostInit`).
  A width comparison written as `!(w >= min && w <= max)` also rejects NaN.
- **Size**: a full line delivery is at most about 560 bytes (64 points × 8 bytes plus id, colour,
  width, age, author name and own flag). A faction at 150 lines is about 75 KB for one player.
- **Alternatives considered**: two 16-bit values per point packed into one int relative to the
  map: half the size, but it depends on the map offset, which `SCR_MapEntity` does not expose.

## R6. Delivering a faction's lines on slot take

- **Decision**: `LL_LobbyManager.SetPlayerEngineFaction_S` (the one place the server changes a
  player's engine faction) starts a line sync after `SetFaction_S`: an owner RPC that clears the
  client's lines, then the faction's lines in server order. Syncs are paced on one 250 ms server
  tick: at most 10 lines per player and 100 in total per tick. Moving to no faction sends only
  the clear. A newer sync for the same player replaces the pending one.
- **Rationale**: markers are re-sent from the same place (`SyncFactionMarkers_S`). Reliable owner
  RPCs from one component arrive in order, so a clear followed by lines needs no version number.
  A line can arrive twice (live and in the sync); the client keys lines by id. Pacing bounds the
  worst case (everyone retaking slots after a session resume with full factions) to about
  120 KB/s of line traffic, well clear of the outbound overloads this server has seen; one player
  receives a full faction in under 4 seconds (SC-003).
- **Alternatives considered**: every line in one frame, as markers are: up to 75 KB per player
  at once, times every player retaking a slot after a resume.

## R7. Identity, ownership and admins

- **Decision**: the server stores the author's identity id (`SCR_PlayerIdentityUtils.GetPlayerIdentityId`)
  and name. Clients never receive the identity; each delivery carries an `own` flag computed for
  that recipient. Delete is allowed to the author (identity match) or to `SCR_Global.IsAdmin`,
  when the line's faction is the requester's faction and the requester passes the marker edit
  rule. Limits (20 per player) and the 2-second rate count by identity, so a reconnect neither
  resets nor breaks them.
- **Rationale**: Principle V: reconnect identity is the identity id, never the player id. The
  marker edit rule (faction, living slot character, no hard freeze) is reused, not copied:
  `LL_M_SCR_MapMarkerSyncComponent.LL_CanEditSharedMarkers` on the server and
  `LL_MapMarkersUI.LL_CanEditMarkers` on the client, both made callable from the line code.

## R8. Refusals

- **Decision**: the client checks the same limits before starting a line (it knows the faction's
  lines and which are its own) and says why. The server re-checks and answers a refused request
  with an owner RPC carrying a reason code; the client translates it and shows it in the chat
  panel with `SCR_ChatPanelManager.ShowHelpMessage`.
- **Rationale**: `RpcDo_SlotImportResult` and `RpcDo_VerifyCommandResult` report results to one
  player this way.

## R9. Who drew it (marker-info key)

- **Decision**: while `LL_MapMarkerInfo` is held, a text label "Name (12 minutes ago)" sits to
  the right of each visible start point, in the marker labels' font, size and colour, at the
  marker label's offset: half the icon size times the Dot entry's label ink, plus the label gap
  (`LL_M_SCR_MapMarkerWidgetComponent.LL_PlaceLabels`). Each delivery
  carries the line's age in seconds; the client stores `GetServerTimestamp()` minus that age and
  formats it with the markers' age wording.
- **Rationale**: markers show exactly this while the key is held
  (`LL_MapMarkersUI.LL_UpdateMarkerAges`, `LL_M_SCR_MapMarkerWidgetComponent.LL_UpdateLabels`).
  A server-relative age avoids sending a `WorldTimestamp`; session resume stores markers' age the
  same way (`LL_MarkerData`). Labels exist only while the key is held.

## R10. Deleting with the marker delete key

- **Decision**: on `MapMarkerDelete`, if one of the player's removable markers is under the
  cursor, the marker is deleted and lines are left alone. Otherwise the topmost line the player
  may delete whose start dot or any segment is within its half width (at least 6 px) of the
  cursor is deleted.
- **Rationale**: one key, one deletion, markers first, matching how the stock marker delete reads
  widgets under the cursor (`SCR_MapMarkersUI.OnInputMarkerDelete`).

## R11. The colour-only dialog

- **Decision**: `LL_InsertMarkerDialog` gets a line mode that hides `PreviewFrame`,
  `EditBoxRoot` and `TypeCombo` from script and reports only the colour. A double click while
  drawing opens it (the 400 ms / 12 px rule of `LL_MapMarkersUI.RegisterClickAndCheckDouble`;
  the point the first click of the pair added is taken back). The line UI drives it the way
  `LL_MapMarkersUI` does: `MapMarkerEditContext` each frame after a one-frame gap,
  `MENU_ACTION_SELECT` and `MENU_ACTION_BACK` listeners, `HandleDialog`. OK recolours the line in
  progress and stores the colour; drawing continues. Lines and markers share the last confirmed
  colour, so Enter sends without a dialog.
- **Rationale**: the spec asks for the marker dialog itself (FR-004), so players meet one
  dialog for both. Hiding from script follows the layout rule that `Visible 0` drops children.

## R12. Session resume

- **Decision**: a `LL_MapLineData` persistent state with its `ScriptedStateSerializer`, registered
  in `Configs/Systems/Persistence/LL_Persistence.conf` beside `LL_MarkerData`. A record holds
  id, faction key, author identity and name, colour, width, points and age. On restore the store
  takes the records once the system is active, continues ids above the highest restored one, and
  starts a sync for every player who already has a faction.
- **Rationale**: identical to how markers are saved and restored (`Persistence/LL_MarkerData.c`).
  The replay recorder is not touched (spec FR-015).

## R13. Teaching the keys

- **Decision**: a "Map lines" heading and one paragraph in the generated Communications entry
  (`LL_MissionDescriptionUI.BuildCommsGuideMarkup`), with the keys in `<b>`, as the voice and
  radio paragraphs do. The three line actions get rows in `keyBindingMenu.conf`, so they can be
  rebound like `LL_MapMarkerInfo`.

## R14. Files from the branch's first commit

Files from the branch's first commit that this design does not use are deleted, and the files
that commit modified return to their `main` content before this feature's changes are applied.
The list is in [plan.md](plan.md).
