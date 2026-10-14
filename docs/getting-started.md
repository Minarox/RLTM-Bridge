# Getting started

## Requirements

- Rocket League on Windows, with [BakkesMod](https://bakkesmod.com) installed and running.
- A WebSocket server receiving the data: RLTM, or the test server of the repository to try the plugin (see [Try it without a server](#try-it-without-a-server)).

## Installation

1. Download `RLTMBridge.dll`:
    - from the [releases](https://github.com/Minarox/RLTM-Bridge/releases) of the repository,
    - or from the artifacts of the last [Build](https://github.com/Minarox/RLTM-Bridge/actions/workflows/build.yml) run (artifact `RLTMBridge`).
2. Copy it in the `plugins` folder of BakkesMod: BakkesMod window → *File* → *Open BakkesMod folder* → `plugins/`.
3. In Rocket League, open the BakkesMod console (++f6++) and type:

    ```
    plugin load rltmbridge
    ```

    The console shows `RLTM Bridge v1.x.x.x loaded`.

4. To load it automatically on each launch, enable it in the plugin manager: ++f2++ → *Plugins* → *Plugin Manager*.

## Connect to the server

Open the settings of the plugin: ++f2++ → *Plugins* → *RLTM Bridge*.

1. **Server URL**: address of the WebSocket server, `ws://` or `wss://` (TLS).

    === "RLTM"

        ```
        ws://<RLTM address>:<port>/?token=<tournament token>
        ```

        The token of the tournament identifies the tournament the match belongs to.

    === "Local server"

        ```
        ws://localhost:3000
        ```

2. **Client name** (optional): name of this computer for the server, to tell the matches apart when several computers are connected. The name of the computer is used if empty.
3. **Token** (optional): token given by the server, sent in the `Authorization` header of the connection.
4. Click **Connect**. The state becomes **Connected** (green).
5. Check **Connect when the plugin is loaded** to connect automatically on each launch.

The plugin reconnects automatically when the connection is lost, and sends the current state of the match again on each connection.

## Spectate a match

The plugin works in **private matches** and offline matches (exhibition with bots). With the default settings, when you join a private match:

- you join the spectators,
- the HUD of the game is hidden (the names of the players and the match info are kept), as well as the statistics graphs (++f10++),
- at the end of the match, you are marked as ready and the replay is saved.

Each of these actions can be disabled in the [settings](configuration.md).

## Try it without a server

The repository contains a small test server showing everything the plugin sends, and sending the commands typed in the terminal:

```bash
pip install websockets
python tools/test_server.py
```

Connect the plugin to `ws://localhost:3000`, join a match, and type `help` in the terminal for the commands (`ping`, `focus <player>`, `replay <blue> <orange> <game>`...).

Or, without installing anything, use the [monitor](monitor.md) in the browser.

!!! tip "Debug mode"
    Enable *Advanced WebSocket settings* → *Debug mode* (`rltm_debug 1`) to see every message sent to and received from the server in the BakkesMod console (++f6++).
