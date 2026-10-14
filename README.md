<div align="center">
  <img src="https://avatars.githubusercontent.com/u/71065703?v=4" alt="Logo" width="112px">

  <h1>RLTM Bridge</h1>

  BakkesMod plugin connecting Rocket League to RLTM (Rocket League Tournament Manager), or any WebSocket server. <br />
  <a href="https://minarox.github.io/RLTM-Bridge"><b>Documentation »</b></a>

  <sub>
    If you like this project, please star it & <a href="https://github.com/Minarox">follow me</a> to see what other cool projects I'm working on! ❤️
  </sub>
</div>

## ⭐️ Features

- 📃 **Data stream** - Send match, statistics and entities datas from the game in real time
- 🔍 **Ready for stream** - Hide some parts of the HUD and Engine debug graph in spectator
- ✅ **Shortcuts** - Automatically mark as ready and save replay file at the end of the match
- ⚙️ **Configurable** - Change target server, events, automatic actions and more

## 🛠️ Tech Stack

- [BakkesModSDK](https://github.com/bakkesmodorg/BakkesModSDK)
- [IXWebSocket](https://github.com/machinezone/IXWebSocket)
- [nlohmann/json](https://github.com/nlohmann/json)

## 🚗 Roadmap

- [x] WebSocket connection to server
- [x] Automatic reconnection on interruption
- [x] Fetch match data (time, score, state, etc.)
- [x] Fetch statistics data (scoreboard, statistic event, etc.)
- [x] Fetch entities data (boost, position, etc.)
- [x] Fetch players data (uid, name, car, etc.)
- [x] Fetch current spectator target
- [x] Send datas to server
- [x] Hide HUD when spectating
- [x] Hide engine debug statistics when spectating (F10)
- [x] Mark as ready when match end
- [x] Save match replay when match end
- [x] Join spectator team on new game
- [x] Customize replay name
- [x] Plugin settings
- [x] Control the game from the server
- [x] Documentation

## ⚙️ Configuration

Every setting is available in the plugin settings window (`F2` > `Plugins` > `RLTM Bridge`) or from the BakkesMod console, and saved in `bakkesmod/cfg/rltm_bridge.cfg`.

The settings window is organized by feature: connection to the server, status (messages sent, last command received, next replay name), data sent to the server, spectator, end of the match, server control and advanced WebSocket settings. Every setting can be reset to its default value from the window.

| Cvar | Default | Description |
| --- | --- | --- |
| **Connection** | | |
| `rltm_url` | `ws://localhost:3000` | URL of the WebSocket server, `ws://` or `wss://` (TLS, with the root certificates of Windows) |
| `rltm_client_name` | | Name of this client for the server (the name of the computer if empty), sent in the `X-RLTM-Client` header and the `plugin` message |
| `rltm_token` | | Optional token to authenticate this client, sent in the `Authorization: Bearer <token>` header |
| `rltm_auto_connect` | `0` | Connect to the server when the plugin is loaded |
| `rltm_auto_reconnect` | `1` | Reconnect automatically to the server |
| `rltm_handshake_timeout` | `3` | Handshake timeout (seconds) |
| `rltm_ping_interval` | `1` | Ping interval (seconds) |
| `rltm_min_wait_between_retries` | `500` | Minimum wait between reconnection retries (milliseconds) |
| `rltm_max_wait_between_retries` | `500` | Maximum wait between reconnection retries (milliseconds) |
| `rltm_per_message_deflate` | `1` | Compress the messages |
| `rltm_debug` | `0` | Log the messages sent to and received from the server in the console (except the entities) |
| **Monitor** | | |
| `rltm_monitor` | `0` | Local WebSocket server (127.0.0.1 only) for the [monitor page](https://minarox.github.io/RLTM-Bridge/monitor/) of the documentation |
| `rltm_monitor_port` | `8765` | Port of the monitor |
| **Data** | | |
| `rltm_send_entities` | `1` | Send the balls and cars data (`entities`) |
| `rltm_entities_interval` | `100` | Interval between two entities updates (16 to 1000 milliseconds) |
| `rltm_send_spectator` | `1` | Send the player followed by the spectator camera (`spectator`) |
| `rltm_send_stats` | `1` | Send the advanced statistics every second (`stats`, always in the `result`) |
| **Spectator** | | |
| `rltm_auto_spectate` | `1` | Join the spectators when joining a match |
| `rltm_hide_hud` | `1` | Hide the HUD when spectating |
| `rltm_hud_names` | `1` | Keep the players names when the HUD is hidden |
| `rltm_hud_match_info` | `1` | Keep the match info (score, clock) when the HUD is hidden |
| `rltm_hide_stat_graphs` | `1` | Hide the engine statistics graphs (F10) when spectating |
| **End of the match** | | |
| `rltm_auto_ready` | `1` | Mark as ready at the end of the match |
| `rltm_auto_save_replays` | `1` | Save the replay at the end of the match |
| `rltm_replay_name_format` | `{blue} vs {orange} - Match {game}` | Name of the replays when the server sends the teams (see `set_replay_name`) |
| **Server control** (see [Commands](#-commands)) | | |
| `rltm_server_control` | `1` | Allow the server to send commands |
| `rltm_server_allow_camera` | `1` | Allow `focus_player` |
| `rltm_server_allow_actions` | `1` | Allow `spectate`, `ready` and `hide_hud` |
| `rltm_server_allow_replay_name` | `1` | Allow `set_replay_name` and `clear_replay_name` |
| `rltm_server_allow_settings` | `1` | Allow `set_setting` |

| Command | Description |
| --- | --- |
| `rltm_connect` | Connect to the WebSocket server |
| `rltm_disconnect` | Disconnect from the WebSocket server |
| `rltm_version` | Show the plugin version |

## 📡 Messages

Every message sent to the server is a JSON object `{ "topic": "<topic>", "payload": <data> }`:

- `plugin` - Sent first on each connection: plugin `version`, `protocol` version of the messages format (currently `2`) and `client` name
- `match` - State of the match (unique `guid`, arena, score, clock, flags, and the custom `teams` names and colors)
- `scoreboard` - Statistics of each player (score, goals, saves, etc. and the `events` counters received for the player, a missing event means 0)
- `spectator` - Player followed by the spectator camera (`target`, `null` with the free camera) and camera state (`camera`)
- `players` - Players of the match (uid, name, team, car)
- `entities` - Balls and cars data (speed, location, boost, etc.), sent every `rltm_entities_interval` ms while the match is playing. Its `time` field is a monotonic time in milliseconds: use the difference between two updates to interpolate the positions
- `stats` - Advanced statistics of each player, every second while the match is playing: boost used and collected, time supersonic, on the ground, on the wall and in the air, average speed
- `statistic` - Statistic event of a player (goal, save, demolition, etc.)
- `event` - Events of the match: `kickoff`, `goal` (team, scorer, assister, own goal, ball speed), `overtime`, `replayStart`, `replayEnd`, `matchEnded`, `podium`
- `result` - Result of the last match, sent once at the end of the match (winner, MVP, score, teams, scoreboard, advanced statistics, replay name): kept and sent again on each connection until the next match ends, deduplicate it with its `guid`
- `response` - Result of a command sent by the server (see below)

The `match`, `scoreboard`, `stats`, `spectator`, `players`, `entities` and `result` topics are only sent when their payload changes, and are sent again on each (re)connection. A `null` payload means that there is no match anymore.

## 🎮 Commands

The server can control the game by sending `{ "command": "<name>", "payload": { ... }, "id": "<optional>" }`.
Each command is answered on the `response` topic with `{ "command": "<name>", "id": "<id>", "success": true }` (and a `"result"` for some commands), or `"success": false` and an `"error"` message.

| Command | Payload | Description |
| --- | --- | --- |
| `ping` | | Check that the plugin answers |
| `resync` | | Send every state again (`match`, `scoreboard`, `stats`, `spectator`, `players`, `entities`, `result`) |
| `spectate` | | Join the spectators |
| `ready` | | Mark as ready |
| `hide_hud` | | Hide the spectator HUD |
| `focus_player` | `{ "uid": "<uid>", "name": "<name>" }` | Focus the spectator camera on a player (`uid` and / or `name`) |
| `set_setting` | `{ "name": "<cvar>", "value": <value> }` | Change a setting of the **Data**, **Spectator** or **End of the match** groups (never the connection nor the server control) |
| `set_replay_name` | `{ "blue": "<team>", "orange": "<team>", "game": <number> }` or `{ "name": "<name>" }` | Name the replay of the current match (or of the next one if the current match is ended), with `rltm_replay_name_format` or the given name. Result: `{ "name": "<name>", "applied": <bool> }` |
| `clear_replay_name` | | Keep the default name for the replay |

Only these commands can be executed: an invalid message or command is answered with an error and never reaches the game. `ping` and `resync` are always allowed, the other commands can be disabled by group in the settings.

## 🧪 Tests

- Unit tests of the messages format (no BakkesMod SDK needed): `sh tests/run.sh` (requires [nlohmann/json](https://github.com/nlohmann/json)), run by the CI on each push.
- [Monitor page](https://minarox.github.io/RLTM-Bridge/monitor/) in the browser: enable the monitor in the plugin settings, it shows the messages of the plugin and sends commands without a server.
- Test server to check the plugin without RLTM: `pip install websockets` then `python tools/test_server.py`, it shows the messages of the plugin and sends commands typed in the terminal.
- In game: [docs/TEST-CHECKLIST.md](docs/TEST-CHECKLIST.md).

## 📖 Documentation

The documentation, in English and in French, is published on [minarox.github.io/RLTM-Bridge](https://minarox.github.io/RLTM-Bridge): installation, configuration, complete reference of the messages and the commands, server integration. Its sources are in `docs/` ([MkDocs Material](https://squidfunk.github.io/mkdocs-material/)): `pip install -r docs/requirements.txt` then `mkdocs serve`.

## 🔨 Build

Open `RLTMBridge.sln` with Visual Studio 2022 and build the `Release | x64` configuration: the dependencies are installed by [vcpkg](https://vcpkg.io) (manifest mode) and the BakkesMod SDK is found from the BakkesMod installation.

Each push is built by GitHub Actions (the DLL is available in the artifacts of the run), and a release is created with the DLL for each `v*` tag. The build number of the version comes from the CI, local builds keep the one of `version.h`.

## 🗂️ Project structure

```
RLTMBridge/
├── RLTMBridge.h / .cpp    Plugin class, load / unload and game hooks
├── Settings/              Cvars, console commands and settings window
├── WebSocket/             Connection to the server, messages and server commands
├── Protocol/              Messages format, without the BakkesMod SDK (unit tested)
└── Game/                  Game data (match, statistics, events, result) and actions (HUD, spectator, replays)
```

## 💼 License

Apache 2.0 © [Mathis Serrieres Maniecki](https://github.com/Minarox)
