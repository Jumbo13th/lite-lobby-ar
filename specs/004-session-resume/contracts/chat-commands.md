# Contract: admin chat commands

All four are typed in the in-game chat, registered through the engine's chat command
invoker like the existing commands, and re-validated on the server with
`SCR_Global.IsAdmin`. A non-admin gets the existing refusal message. With session
saves off for the mission, every command answers "not enabled" and does nothing.

| Command | Argument | Effect | Reply |
|---------|----------|--------|-------|
| `/save` | none | requests a manual save point now; refused outside GAME, during the hold, or while a save is in progress | the time of the save point once it completes, or the refusal reason |
| `/resume` | none | releases the resume hold; refused when no hold is active | broadcast to everyone: the game is on |
| `/discard` | `confirm` | disallows further saves and deletes the saved session; without the argument it only explains what it would do | confirmation, or the explanation |
| `/shutdown` | none | takes a final blocking save point and stops the server process when it completes; refused while a save is in progress | broadcast to everyone before the stop |

`/help` lists the four with one-line descriptions, admin-only like the existing
admin entries.

Broadcasts and replies are localization keys (`LL-Resume_*`), EN and RU, translated
on each client.
