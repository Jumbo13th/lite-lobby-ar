# Contract: the `/replay` chat command

| Who | What | Network |
|-----|------|---------|
| an admin | types `/replay` | one request to the server, one reply to that admin |
| anyone else | types `/replay` | the client says the command is for admins; the server would ignore the request |

The request carries nothing. The server checks the sender with `SCR_Global.IsAdmin` and
answers only an admin, with six values: state, session, segment, recorded seconds,
kilobytes written, and seconds since the last write. The client turns them into one
translated line.

## Replies

| State | English | Russian |
|-------|---------|---------|
| not enabled | Replay recording is not enabled on this server. | Запись реплеев на этом сервере не включена. |
| ready | Replay recording is on and starts with the game. | Запись реплеев включена и начнётся вместе с игрой. |
| recording | Recording %1, segment %2: %3 min, %4 MB, last write %5 s ago. | Идёт запись %1, сегмент %2: %3 мин, %4 МБ, последняя запись %5 с назад. |
| ended | Recording %1 ended at debriefing. | Запись %1 завершена на дебрифинге. |
| failed | Replay recording stopped: the server cannot write to its profile. | Запись реплея остановлена: сервер не может писать в профиль. |

Keys: `LL-Replay_NotEnabled`, `LL-Replay_Ready`, `LL-Replay_Recording`,
`LL-Replay_Ended`, `LL-Replay_Failed`, and `LL-Command_HelpReplay` for the `/help`
line ("/replay — replay recording status (admin)" / «/replay — статус записи реплея
(админ)»). The Russian text is written by the localization pass in the community's
terms; the lines above give the meaning.
