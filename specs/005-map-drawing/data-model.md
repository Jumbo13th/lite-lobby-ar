# Data Model: Shared Map Drawing

Preserve existing fields and algorithms; change identifiers mechanically to the repository prefix.

| Entity | Existing fields and relationships | Validation / lifecycle |
| --- | --- | --- |
| Stroke | Map-space points, width, anchor flag, packed color, owner id, spatial id and draw order | Fragments retain author/color/order; point budget remains 8192 per faction document. |
| Drawing document | Ordered strokes, point count and spatial index | Authority applies valid operations; erasing updates both geometry and index. Lifetime is one mission. |
| Operation | Server revision, server-resolved author, client correlation token, kind, size, color, points | Preserve operation enums and validation; at most 32 transmitted points per operation. Client tokens are not player identity. |
| Replica | Current document, pending snapshot, buffered operations and contiguous revision | Keep snapshot assembly and ordered replay. No editing before readiness. |
| Faction state | Faction key and authoritative replica/history | Server derives faction from owned player controller; previous faction data is discarded locally on a switch. |
| History record/group | Author id/name, tool, monotonic creation time, erased author; per-actor current group | Ten-second grouping, five-minute expiry; preserve independent erased-author identity. |
| History snapshot | Request id, faction key, records, ready/available state | Complete owner-directed response only; frozen after completion, dropped on close/faction switch. |
| Tool state | Selected mode, enabled flag, color, active gesture, pending anchor and temporary modifier | Local-only; map close clears gestures and disables drawing. Permission loss ends the gesture. |

No persistence schema, migration or new replicated property is introduced. User-visible history formatting changes to localization without changing the history record model.
