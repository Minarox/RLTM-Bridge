# Commands

The server can control the game by sending a JSON text message:

```json
{ "command": "<name>", "payload": { ... }, "id": "<optional>" }
```

| Field | Type | Description |
|---|---|---|
| `command` | string | Name of the command |
| `payload` | object | Parameters of the command, can be omitted when there are none |
| `id` | string \| number | Optional, sent back in the [`response`](messages.md#response) to match it with the command |

Each command is answered on the [`response`](messages.md#response) topic:

```json
{ "topic": "response", "payload": { "command": "ping", "id": "1", "success": true } }
{ "topic": "response", "payload": { "command": "focus_player", "id": "2", "success": false, "error": "Player not found or camera not available" } }
```

## Safety

A wrong command can't crash the plugin or the game:

- the messages larger than **16 KB** or deeper than **32 levels** are rejected before being parsed, and the JSON is parsed without exceptions,
- only the commands below can be executed, **never console commands**,
- the commands are queued (**64** at most) and executed on the game thread (**8** per frame at most), never from the network thread,
- every parameter is validated, and every error is sent back in the response.

The user of the plugin can disable every command, or only some groups of commands, in the [settings](../configuration.md#server-control). A disabled command is answered with an error: show it, so the operator knows what to enable.

## Errors

| `error` | Cause |
|---|---|
| `Message too large` / `JSON too deep` / `Invalid JSON object` | The message isn't a valid command |
| `"command" must be a string` | `command` is missing or isn't a string |
| `Unknown command "<name>"` | The command doesn't exist |
| `Server control is disabled` | Every command is disabled in the plugin |
| `Command "<name>" is disabled in the plugin settings` | The group of the command is disabled in the plugin |
| `Too many pending commands` | More than 64 commands are waiting to be executed |
| `"<parameter>" is required` / `must be a string` / `must be an integer` / `must be between <min> and <max>` | Invalid parameter |
| Other messages | The command couldn't be executed, e.g. `Not in a match` |

## Commands

| Command | Group | Description |
|---|---|---|
| [`ping`](#ping) | Always allowed | Check that the plugin answers |
| [`resync`](#resync) | Always allowed | Send every state again |
| [`spectate`](#spectate-ready-hide_hud) | Player actions | Join the spectators |
| [`ready`](#spectate-ready-hide_hud) | Player actions | Mark as ready |
| [`hide_hud`](#spectate-ready-hide_hud) | Player actions | Hide the spectator HUD |
| [`focus_player`](#focus_player) | Spectator camera | Focus the camera on a player |
| [`set_replay_name`](#set_replay_name) | Replay names | Name the replay of the match |
| [`clear_replay_name`](#set_replay_name) | Replay names | Keep the default name of the replay |
| [`set_setting`](#set_setting) | Game settings | Change a setting of the plugin |

### `ping`

Check that the plugin answers, e.g. to show the connection state of the game in the server. No parameter.

### `resync`

Send every state again (`match`, `scoreboard`, `stats`, `spectator`, `players`, `entities`, `result`), e.g. after a restart of a part of the server. No parameter.

### `spectate`, `ready`, `hide_hud`

Join the spectators, mark as ready, or hide the spectator HUD (with the elements kept in the settings), even when the automatic action is disabled in the settings. No parameter. Rejected with `Not in a match` (or `Not spectating a match` for `hide_hud`) when it can't be done.

### `focus_player`

Focus the spectator camera on a player.

| Parameter | Type | Description |
|---|---|---|
| `uid` | string | `uid` of the player |
| `name` | string | Name of the player |

At least one of them is required, give both to choose a bot (the bots share the same `uid`).

```json
{ "command": "focus_player", "payload": { "name": "Player1" }, "id": "camera-1" }
```

### `set_replay_name`

Name the replay of the current match, saved at the end of the match. Sent after the end of a match, the name is used for the **next** match. The name is only used for **one** match: send it for each game of a series.

=== "With the teams"

    ```json
    { "command": "set_replay_name", "payload": { "blue": "Team 1", "orange": "Team 2", "game": 3 } }
    ```

    | Parameter | Type | Description |
    |---|---|---|
    | `blue` | string | Name of the blue team (team `0`) |
    | `orange` | string | Name of the orange team (team `1`) |
    | `game` | integer | Number of the game in the series, 1 to 99 |

    The name is built with the `rltm_replay_name_format` setting of the plugin, `{blue} vs {orange} - Match {game}` by default: `Team 1 vs Team 2 - Match 3`.

=== "With a whole name"

    ```json
    { "command": "set_replay_name", "payload": { "name": "Grand final - Game 3" } }
    ```

    | Parameter | Type | Description |
    |---|---|---|
    | `name` | string | Name of the replay |

The name is sanitized (control characters and surrounding spaces removed) and limited to 128 bytes. The response contains the final name:

```json
{ "topic": "response", "payload": { "command": "set_replay_name", "id": null, "success": true, "result": { "name": "Team 1 vs Team 2 - Match 3", "applied": true } } }
```

`applied` is `false` when there is no match in progress or the match is over: the name will be given to the replay of the next match.

`clear_replay_name` forgets the name: the replay keeps the default name of the game. No parameter.

!!! note
    The replay is only saved when *Save the replay* (`rltm_auto_save_replays`) is enabled in the plugin.

### `set_setting`

Change a setting of the plugin.

| Parameter | Type | Description |
|---|---|---|
| `name` | string | Name of the cvar |
| `value` | string \| number \| boolean | New value (`true` / `false` for the checkboxes) |

Only the settings of the *Data*, *Spectator* and *End of the match* groups can be changed: `rltm_send_entities`, `rltm_entities_interval`, `rltm_send_stats`, `rltm_send_spectator`, `rltm_auto_spectate`, `rltm_hide_hud`, `rltm_hud_names`, `rltm_hud_match_info`, `rltm_hide_stat_graphs`, `rltm_auto_ready`, `rltm_auto_save_replays`, `rltm_replay_name_format`. The connection and the server control settings can't be changed by the server.

```json
{ "command": "set_setting", "payload": { "name": "rltm_entities_interval", "value": 50 } }
```
