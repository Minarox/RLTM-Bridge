<div align="center">
  <img src="https://avatars.githubusercontent.com/u/71065703?v=4" alt="Logo" width="112px">

  <h1>WebSocket Controller Plugin</h1>

  Bi-directionnal communication between WebSocket server and Rocket League. <br />
  <a href="https://minarox.github.io/WSC-Plugin"><b>Documentation »</b></a>

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
- [ ] Fetch current spectator target
- [x] Send datas to server
- [x] Hide HUD when spectating
- [x] Hide engine debug statistics when spectating (F10)
- [x] Mark as ready when match end
- [x] Save match replay when match end
- [x] Join spectator team on new game
- [ ] Customize replay name
- [x] Plugin settings
- [ ] Control the game from the server
- [ ] Documentation

## ⚙️ Configuration

Every setting is available in the plugin settings window (`F2` > `Plugins` > `WebSocket Controller`) or from the BakkesMod console, and saved in `bakkesmod/cfg/wsc.cfg`.

| Cvar | Default | Description |
| --- | --- | --- |
| `wsc_url` | `ws://localhost:3000` | URL of the WebSocket server |
| `wsc_auto_connect` | `0` | Connect to the server when the plugin is loaded |
| `wsc_auto_reconnect` | `1` | Reconnect automatically to the server |
| `wsc_handshake_timeout` | `3` | Handshake timeout (seconds) |
| `wsc_ping_interval` | `1` | Ping interval (seconds) |
| `wsc_min_wait_between_retries` | `500` | Minimum wait between reconnection retries (milliseconds) |
| `wsc_max_wait_between_retries` | `500` | Maximum wait between reconnection retries (milliseconds) |
| `wsc_per_message_deflate` | `1` | Compress the messages |
| `wsc_entities_interval` | `100` | Interval between two entities updates (milliseconds) |
| `wsc_auto_spectate` | `1` | Join the spectators when joining a match |
| `wsc_hide_hud` | `1` | Hide the HUD when spectating |
| `wsc_hide_stat_graphs` | `1` | Hide the engine statistics graphs (F10) when spectating |
| `wsc_auto_ready` | `1` | Mark as ready at the end of the match |
| `wsc_auto_save_replays` | `1` | Save the replay at the end of the match |

| Command | Description |
| --- | --- |
| `wsc_connect` | Connect to the WebSocket server |
| `wsc_disconnect` | Disconnect from the WebSocket server |
| `wsc_version` | Show the plugin version |

## 📡 Messages

Every message sent to the server is a JSON object `{ "topic": "<topic>", "payload": <data> }`:

- `match` - State of the match (arena, score, clock, flags and statistics of each player)
- `players` - Players of the match (uid, name, team, car)
- `entities` - Balls and cars data (speed, location, boost, etc.), sent every `wsc_entities_interval` ms while the match is playing
- `statistic` - Statistic event of a player (goal, save, demolition, etc.)

The `match`, `players` and `entities` topics are only sent when their payload changes, and are sent again on each (re)connection. A `null` payload means that there is no match anymore.

## 🗂️ Project structure

```
WSC/
├── WSC.h / WSC.cpp        Plugin class, load / unload and game hooks
├── Settings/              Cvars, console commands and settings window
├── WebSocket/             Connection to the server and messages
└── Game/                  Game data (match, statistics, entities) and actions (HUD, spectator, replays)
```

## 💼 License

Apache 2.0 © [Mathis Serrieres Maniecki](https://github.com/Minarox)
