# Changelog

## 1.1.0 (protocol 2)

The plugin is renamed **RLTM Bridge** (formerly WSC, WebSocket Controller).

### Breaking changes

- The cvars and console commands are prefixed with `rltm_` instead of `wsc_`, and saved in `rltm_bridge.cfg`: the previous settings aren't loaded.
- The statistics of the players moved from `match.statistics` to the new [`scoreboard`](protocol/messages.md#scoreboard) topic, only sent when they change.
- The stat events counters (`Demolish`, `EpicSave`...) are grouped in an `events` object and only contain the events received for the player.
- `entities.cars[].asFlip` is renamed `hasFlip`.

### New

- [`plugin`](protocol/messages.md#plugin) message with the versions of the plugin and of the protocol, and the client name.
- [`stats`](protocol/messages.md#stats): advanced statistics (boost, supersonic, air time, average speed).
- [`event`](protocol/messages.md#event): kickoff, goal (scorer, assist, ball speed), overtime, replays, end of the match, podium.
- [`result`](protocol/messages.md#result): complete result of each match.
- [`spectator`](protocol/messages.md#spectator): player followed by the spectator camera.
- `match.guid`, `match.teams` (custom names and colors), `entities.time`.
- [Commands](protocol/commands.md) from the server, with [`response`](protocol/messages.md#response) messages: camera, replay names, player actions, settings.
- Client identification: `X-RLTM-Client` and `Authorization` headers.
- Settings window organized by feature, debug mode, settings for every automatic action.
- [Replays](protocol/index.md#replays): the replay of each match is sent to the server over HTTP once the game has saved it, with retries.
- [Monitor](monitor.md): local WebSocket server of the plugin and web page showing its messages and sending commands, without a server.

### Fixes

- Random crashes: the entities were sent from a separate thread sharing the data of the game thread.
- A message of the server containing `{` could crash the plugin.
- The socket settings were only applied when changed, and the automatic connection didn't work.
- Two identical stat events in a row (e.g. two demolitions) were counted once.
- The game could slow down when the server didn't read the messages fast enough.
- The automatic replay saving of BakkesMod was disabled when unloading the plugin, it is now restored.

## 1.0.0 (protocol 1)

First version: match, statistics, players and entities data, HUD and spectator actions.
