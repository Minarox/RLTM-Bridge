# Configuration

Every setting is available in the settings window of the plugin (++f2++ → *Plugins* → *RLTM Bridge*) and as a BakkesMod cvar, usable in the console (++f6++), for example `rltm_hide_hud 0`.

The settings are saved in `bakkesmod/cfg/rltm_bridge.cfg`, and can be reset to their default values from the *Reset* section of the window.

## Settings window

| Section | Content |
|---|---|
| **Connection** | Server URL, client name, token, connection on load, *Connect* / *Disconnect* button, connection state and last error |
| **Status** | Messages and kilobytes sent, *Resend the state* button, commands received and the last one, name of the next replay |
| **Data sent to the server** | Balls and cars data and their update interval, advanced statistics, player followed by the camera |
| **Spectator** | Join the spectators, hide the HUD (and the elements kept), hide the statistics graphs |
| **End of the match** | Mark as ready, save the replay, replay name format with a preview |
| **Server control** | Commands allowed to the server, by group |
| **Advanced WebSocket settings** | Reconnection, compression, timeouts, debug mode |
| **Reset** | Reset every setting to its default value |

## Connection

| Cvar | Default | Description |
|---|---|---|
| `rltm_url` | `ws://localhost:3000` | URL of the WebSocket server, `ws://` or `wss://` (TLS, with the root certificates of Windows). The connection restarts when it changes. |
| `rltm_client_name` | *(empty)* | Name of this client for the server, the name of the computer if empty. Sent in the `X-RLTM-Client` header and the [`plugin`](protocol/messages.md#plugin) message. |
| `rltm_token` | *(empty)* | Optional token sent in the `Authorization: Bearer <token>` header, so the server can reject an unknown client. Never sent in the messages. |
| `rltm_auto_connect` | `0` | Connect to the server when the plugin is loaded |
| `rltm_auto_reconnect` | `1` | Reconnect automatically when the connection is lost |
| `rltm_handshake_timeout` | `3` | Handshake timeout (seconds) |
| `rltm_ping_interval` | `1` | Ping interval (seconds) |
| `rltm_min_wait_between_retries` | `500` | Minimum wait between two reconnection retries (milliseconds) |
| `rltm_max_wait_between_retries` | `500` | Maximum wait between two reconnection retries (milliseconds) |
| `rltm_per_message_deflate` | `1` | Compress the messages (per message deflate), applied on the next connection |
| `rltm_debug` | `0` | Log the messages sent to and received from the server in the console, except the entities and the advanced statistics |

## Data sent to the server

The [match](protocol/messages.md#match), [scoreboard](protocol/messages.md#scoreboard), [players](protocol/messages.md#players), [events](protocol/messages.md#event) and [result](protocol/messages.md#result) are always sent. A disabled data is cleared on the server (its topic is sent with a `null` payload).

| Cvar | Default | Description |
|---|---|---|
| `rltm_send_entities` | `1` | Send the balls and cars data ([`entities`](protocol/messages.md#entities)) |
| `rltm_entities_interval` | `100` | Interval between two entities updates, 16 to 1000 milliseconds |
| `rltm_send_stats` | `1` | Send the advanced statistics every second ([`stats`](protocol/messages.md#stats)), always included in the result |
| `rltm_send_spectator` | `1` | Send the player followed by the spectator camera ([`spectator`](protocol/messages.md#spectator)) |

## Spectator

| Cvar | Default | Description |
|---|---|---|
| `rltm_auto_spectate` | `1` | Join the spectators when joining a match |
| `rltm_hide_hud` | `1` | Hide the HUD of the game when spectating, to show the overlays instead. The HUD comes back as soon as the setting is disabled. |
| `rltm_hud_names` | `1` | Keep the names of the players above the cars when the HUD is hidden |
| `rltm_hud_match_info` | `1` | Keep the match info (score, clock) when the HUD is hidden |
| `rltm_hide_stat_graphs` | `1` | Hide the engine statistics graphs (++f10++) |

## End of the match

| Cvar | Default | Description |
|---|---|---|
| `rltm_auto_ready` | `1` | Mark as ready at the end of the match |
| `rltm_auto_save_replays` | `1` | Save the replay of every match. The previous value of the BakkesMod setting is restored when the plugin is unloaded. |
| `rltm_replay_name_format` | `{blue} vs {orange} - Match {game}` | Name of the replay when the server sends the teams of the match with [`set_replay_name`](protocol/commands.md#set_replay_name): `{blue}`, `{orange}` and `{game}` are replaced by the blue team, the orange team and the game number of the series |

## Server control

The server can send [commands](protocol/commands.md) to the game. Only the commands of the plugin can be executed, never console commands.

| Cvar | Default | Description |
|---|---|---|
| `rltm_server_control` | `1` | Allow the server to send commands. Disabled, every command is rejected. |
| `rltm_server_allow_camera` | `1` | Allow [`focus_player`](protocol/commands.md#focus_player) |
| `rltm_server_allow_actions` | `1` | Allow [`spectate`, `ready` and `hide_hud`](protocol/commands.md#spectate-ready-hide_hud) |
| `rltm_server_allow_replay_name` | `1` | Allow [`set_replay_name` and `clear_replay_name`](protocol/commands.md#set_replay_name) |
| `rltm_server_allow_settings` | `1` | Allow [`set_setting`](protocol/commands.md#set_setting) |

`ping` and `resync` are always allowed while the server control is enabled.

## Console commands

| Command | Description |
|---|---|
| `rltm_connect` | Connect to the server |
| `rltm_disconnect` | Disconnect from the server |
| `rltm_version` | Show the version of the plugin |
