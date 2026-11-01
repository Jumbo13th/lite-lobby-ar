# Contract: recording format `ll-replay/1`

What the server writes under its profile, and how a reader reconstructs a game from it.
A website builds its viewer from this document alone.

## Files

All files live in `$profile:LL_Replays/`.

| File | What it is | Public |
|------|------------|--------|
| `<session>-<n>.llr` | segment `n` of the recording of session `<session>`, `n` from 1 | yes, once the game is approved |
| `<session>-identities.txt` | the recording's players and their identity GUIDs | never |
| `status.txt` | one line describing what the server is doing now | not sent |

`<session>` is the server session identity the statistics files also carry, for example
`20261031-201500-eden` (local start date and time, then the world's slug). A game that
is never resumed after a crash is one segment file. A crash-resume opens the next
segment.

## Lines and fields

- UTF-8, one record per line, lines end with LF. Readers accept CR LF.
- A record is a type token followed by fields, each separated by one space.
- Field kinds:
  - **int**: decimal, optionally negative.
  - **tok**: one or more characters, none of them a space or a double quote.
  - **str**: a JSON string literal: double quotes, `\"`, `\\`, `\n`, `\r`, `\t` escaped,
    every other character as UTF-8.
  - `-` stands for an absent value where a field allows it.
- A reader skips a line whose type it does not know, skips a line it cannot parse, and
  ignores a last line that does not end with LF (a file cut off by a crash).

## Coordinates, time and identifiers

- Positions are integer metres in world space: `x` east, `z` north, origin at the
  world's south-west corner; `alt` is metres above sea level.
- Headings are integer degrees 0 to 359 as the engine reports an entity's yaw.
- Mission time `t` is the mission clock the statistics files use, in tenths of a second
  (an integer). It runs during the game phase and stands still during a hard freeze. A
  statistics event at `t` seconds and a record at `10 × t` describe the same moment.
- Identifiers are local to one recording. `eid` entities, `pref` players, `wid` weapons,
  `aid` map areas and `zid` trigger zones are stable across its segments. `gid` squads and
  groups, `slot` slots and `mid` markers are numbered within a segment: the segment's first
  snapshot restates every one of them, and a squad keeps its callsign across segments.
- `side` is a faction key from the header. `sides` is `*` (everyone) or faction keys
  joined by commas.
- Colours are hex `rrggbb`, or `rrggbbaa` where transparency matters.

## Header

Every segment file begins with its header:

```text
LLR 1
H session 20261031-201500-eden
H segment 1
H mission "Operation Example"
H author "[ABC]Author"
H world "{853E92315D1D9EFE}worlds/Eden/Eden.ent"
H bounds 0 0 12800 12800
H date 1989 6 21 5 30
H started 1793563200
H side US "United States" 2266b3
H side USSR "Soviet Union" b32a2a
H end
```

| Record | Fields |
|--------|--------|
| `LLR` | int format version, `1`; raised whenever the meaning of any record changes, so a reader keys any workaround for a faulty release on it |
| `H session` | tok session identity |
| `H segment` | int segment number |
| `H mission` | str mission name as the game reports it (may be a localization key) |
| `H author` | str mission author |
| `H world` | str world resource |
| `H bounds` | int min x, min z, max x, max z of the terrain |
| `H date` | int in-game year, month, day, hour, minute at the start of the segment |
| `H started` | int UTC Unix seconds at the start of the segment |
| `H resumed` | int mission time `t` the segment resumed from; only in segments 2 and later |
| `H side` | tok faction key, str name, colour |
| `H end` | end of the header |

## Time

| Record | Fields | Meaning |
|--------|--------|---------|
| `T` | int `t` | every following record happened at mission time `t`, until the next `T` |
| `U` | int Unix seconds, int `t` | wall-clock anchor; written at the start of a segment, at least every 10 seconds, and while the mission clock stands still before the first record of every new wall-clock second, so the real time of any record is known within a second |

## Dictionaries

| Record | Fields | Meaning |
|--------|--------|---------|
| `W` | int `wid`, tok weapon type, str name | a weapon; type is `rifle`, `sniperrifle`, `machinegun`, `handgun`, `grenadelauncher`, `rocketlauncher`, `fraggrenade`, `smokegrenade`, `autocannon` or `other` |
| `GROUP` | int `gid`, side or `-`, str callsign | a squad or AI group |
| `N` | int `pref`, str name | a player and the name the game displayed; repeated when the name changes |

## Roster

| Record | Fields | Meaning |
|--------|--------|---------|
| `S` | int `slot`, int `gid`, side, str role | a slot of the lobby; `slot` is the slot's stable sort key |
| `O` | int `slot`, `pref` or `-` | the slot's occupant changed |
| `D` | int `pref`, int `1` or `0` | the player connected or disconnected |
| `C` | int `pref`, `eid` or `-` | the entity the player controls changed |

## Entities

| Record | Fields | Meaning |
|--------|--------|---------|
| `E` | int `eid`, tok kind, side or `-` (for a tracker: `sides`), `gid` or `-`, tok prefab file name, str display name | an entity appeared |
| `P` | int `eid`, `x`, `z`, `alt`, heading | the entity is here |
| `Q` | int `eid` | the entity was removed from the world (deleted, not killed) |

Kinds: `man`, `car`, `truck`, `apc`, `tracked`, `heli`, `plane`, `static`, `mortar`,
`vehicle` (any other vehicle, boats included) and `tracker` (a mission-maker tracker,
whose display name is its label).

A `P` record is written when the entity moved at least 1 metre east or north, 2 metres in
height, or turned at least 5 degrees since its previous record; characters are checked every second,
vehicles and trackers every half second. Between records a reader interpolates.

## Combat

| Record | Fields | Meaning |
|--------|--------|---------|
| `L` | int `eid`, `u`, attacker `eid` or `-`, tok cause or `-`, `wid` or `-`, int metres or `-` | the character was knocked out |
| `L` | int `eid`, `a` | the character is conscious again |
| `K` | int victim `eid`, killer `eid` or `-`, tok relation, tok cause or `-`, `wid` or `-`, int metres or `-` | a character died or a vehicle was destroyed |
| `V` | int `eid`, vehicle `eid`, tok role, str seat name | the character took a seat; role is `driver`, `gunner` or `cargo` |
| `V` | int `eid`, `-` | the character left its seat |
| `F` | int `eid`, int shots, int heading, `wid` or `-` | the character fired this many shots in the second that ends at this record, first towards this heading |
| `B` | int `x`, `z`, `alt`, tok kind, shooter `eid` or `-`, `wid` or `-` | something detonated here; kind is `grenade`, `rocket` or `shell` |

Relations: `enemy`, `friendly`, `suicide`, `editor` (killed by a Game Master), `none`
(no killer is known), `vehicle` (the victim is a vehicle). Causes are the game's damage
type names in lower case, for example `kinetic`, `explosive`, `fragmentation`,
`incendiary`, `fire`, `collision`. Individual hits that neither knock out nor kill are
not recorded.

## Map

| Record | Fields | Meaning |
|--------|--------|---------|
| `M+` | tok `mid`, tok type, int `x`, `z`, rotation, `pref` or `-`, `sides`, str icon, colour or `-`, str text | a marker was placed; type `manual` is a mission-maker marker, the others are the game's marker types in lower case |
| `M-` | tok `mid` | the marker was removed |
| `A` | int `aid`, `sides`, fill colour or `-`, outline colour or `-`, int outline width, int `1` for an open line or `0`, str label, then int `x` `z` pairs | a map area the mission maker drew |
| `AV` | int `aid`, int `1` shown or `0` hidden | the area appeared or disappeared |
| `Z` | int `zid`, attacker side or `-`, defender side or `-`, str label, then int `x` `z` pairs | a trigger zone |
| `ZH` | int `zid`, holder side or `-` | the zone's holder changed (`-` is nobody) |
| `ZC` | int `zid`, side | the zone was captured by this side |
| `TR` | str trigger key, str label | a trigger fired |

Marker text is recorded as the author typed it. The game filters profanity on clients
only; a public viewer should filter it as well.

## Game flow

| Record | Fields | Meaning |
|--------|--------|---------|
| `FLOW game` | | the game phase began |
| `FLOW freeze` | int seconds | freeze time started or was changed; seconds remaining |
| `FLOW freeze-end` | | freeze time ended (it can end twice if an admin revives it) |
| `FLOW hardfreeze` | int seconds | a hard freeze started or was changed; seconds remaining, or 0 while it holds until an admin releases it |
| `FLOW hardfreeze-end` | | the hard freeze ended |

## Snapshots

`SNAP` and `SNAP-END` enclose a full statement of the current state: every `W`,
`GROUP`, `N`, `S`, `O`, `D`, `C`, every live entity's `E` and `P`, `L u` for every
unconscious character, `V` for every occupied seat, `M+` for every marker, `A` and `AV`
for every area, `Z` and `ZH` for every zone, and `FLOW freeze` or `FLOW hardfreeze` while
one is running. No other record appears between `SNAP` and `SNAP-END`. A snapshot
follows the header of every segment and repeats at least every five minutes of mission
time. A reader can start at any snapshot with nothing but
that file's header. A snapshot without its `SNAP-END` (a file cut off by a crash) is
ignored.

## Ends

| Record | Meaning |
|--------|---------|
| `STOP shutdown` | the server stopped cleanly; the game may still be resumed into a next segment |

A segment without a stop record was cut off by a crash. A segment `n` ≥ 2 names in
`H resumed` the mission time it continues from; records in earlier segments later than
that time were rolled back by the crash and are not part of the game.

## Identity file

```text
LLI 1
H session 20261031-201500-eden
I 1 6f1a2b3c-4d5e-6f70-8192-a3b4c5d6e7f8
I 2 0b9c8d7e-6f5a-4b3c-2d1e-0f9a8b7c6d5e
```

`I` pairs a player reference with the player's identity GUID. A line is appended once the
player's identity is known, which can be seconds after the player first appears; a player
whose identity becomes known only after a reference was given keeps that reference, unless
the identity already has an earlier one, which the player uses from then on. A player whose
identity never becomes known has no line. This file is for matching statistics on the
receiving website and is never shown to viewers.

## Status file

One line, rewritten at server start and with every write:

```text
LLS 1 rec 20261031-201500-eden 1 1793566800
```

Fields: format version, state, session or `-`, segment or `-`, UTC Unix seconds.
States: `idle` (no recording; the session is the one this server start minted), `resume`
(the server is resuming a game; nothing is over yet), `rec` (recording this session and
segment). A reader of a
partially written status file retries.

## Example

```text
T 1240
P 17 4512 6120 41 87
P 18 4519 6118 41 92
F 17 3 88 4
L 31 u 17 kinetic 4 142
T 1250
U 1793563325 1250
K 31 17 enemy kinetic 4 142
M+ g57 custom 4630 6211 0 3 US "danger" ff3030 "MG here"
```
