# Messages

Every message sent by the plugin: `{ "topic": "<topic>", "payload": <data> }`. See the [overview](index.md) for the state and one-shot topics, the identity of the players and the units.

| Topic | Kind | Sent |
|---|---|---|
| [`plugin`](#plugin) | One-shot | First, on each connection |
| [`match`](#match) | State | On each change of the match (every second while the clock runs) |
| [`scoreboard`](#scoreboard) | State | When the statistics of a player change |
| [`stats`](#stats) | State | Every second while the match is played |
| [`spectator`](#spectator) | State | When the camera target changes |
| [`players`](#players) | State | When a player joins, leaves or changes team or car |
| [`entities`](#entities) | State | Every 100 ms (by default) while the match is played |
| [`statistic`](#statistic) | One-shot | On each stat event of a player (goal, save, demolition...) |
| [`event`](#event) | One-shot | On each event of the match (kickoff, goal, overtime...) |
| [`result`](#result) | State | Once, at the end of the match |
| [`response`](#response) | One-shot | Answer to a [command](commands.md) |

When there is no match anymore, `match`, `scoreboard`, `stats`, `spectator`, `players` and `entities` are sent with a `null` payload.

---

## `plugin`

Sent first on each connection.

| Field | Type | Description |
|---|---|---|
| `version` | string | Version of the plugin |
| `protocol` | number | Version of the protocol, currently `2` |
| `client` | string | Name of the client (the client name of the settings, or the name of the computer) |

```json
{ "topic": "plugin", "payload": { "version": "1.1.0.42", "protocol": 2, "client": "CASTER-1" } }
```

---

## `match`

State of the match.

| Field | Type | Description |
|---|---|---|
| `guid` | string \| null | Unique ID of the match given by the game, `null` when it doesn't give one (offline matches) |
| `arenaCode` | string | Code of the arena, e.g. `Stadium_P` |
| `score` | number[] | `[blue, orange]` |
| `teams` | object[] | Teams, see below |
| `clock` | number | Seconds remaining, frozen when the match ends in overtime |
| `duration` | number | Duration of the match in the settings (seconds) |
| `isUnlimited` | boolean | Unlimited time |
| `isStarted` | boolean | `true` from the first kickoff |
| `isPaused` | boolean | The match is paused |
| `isOvertime` | boolean | Overtime |
| `isEnded` | boolean | The match is over |
| `isReplay` | boolean | A goal replay is playing |

A team:

| Field | Type | Description |
|---|---|---|
| `index` | number | `0` blue, `1` orange |
| `name` | string \| null | Custom name of the team set in the private match, `null` if not set |
| `primaryColor` | string | Primary color, `#RRGGBB` |
| `secondaryColor` | string | Secondary color, `#RRGGBB` |

```json
{
  "topic": "match",
  "payload": {
    "guid": "D3A6F8C24B1E4E0F9A2B6C7D8E9F0A1B",
    "arenaCode": "Stadium_P",
    "score": [1, 2],
    "teams": [
      { "index": 0, "name": null, "primaryColor": "#1873FF", "secondaryColor": "#0E4AA8" },
      { "index": 1, "name": null, "primaryColor": "#FF8A15", "secondaryColor": "#B35A00" }
    ],
    "clock": 183,
    "duration": 300,
    "isUnlimited": false,
    "isStarted": true,
    "isPaused": false,
    "isOvertime": false,
    "isEnded": false,
    "isReplay": false
  }
}
```

---

## `scoreboard`

Statistics of each player, indexed by the [key of the player](index.md#players). Empty object until the match starts.

| Field | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identity of the player](index.md#players) |
| `mvp` | boolean | MVP of the match |
| `score` | number | Score of the scoreboard |
| `goals`, `shots`, `assists`, `saves` | number | Counters of the scoreboard |
| `ballTouches`, `carTouches` | number | Touches of the ball and of other cars |
| `events` | object | Number of each stat event received for the player since the start of the match (`Goal`, `Shot`, `Save`, `EpicSave`, `Demolish`, `AerialGoal`...). **A missing event means 0.** |

```json
{
  "topic": "scoreboard",
  "payload": {
    "Steam|76561198000000000|0|Player1": {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "mvp": false, "score": 320, "goals": 1, "shots": 2, "assists": 0, "saves": 1,
      "ballTouches": 24, "carTouches": 3,
      "events": { "Shot": 2, "Goal": 1, "Save": 1, "Demolish": 2 }
    }
  }
}
```

---

## `stats`

Advanced statistics computed by the plugin on each frame, while the ball is in play (from the first touch after a kickoff to the next goal, without the goal replays). Indexed by the [key of the player](index.md#players), sent every second while the match is played (only when a value changes). Can be disabled with `rltm_send_stats`, always included in the [`result`](#result).

| Field | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identity of the player](index.md#players) |
| `boostPickups` | number | Boost pads collected |
| `boostUsed` | number | Boost used, `100` per full tank |
| `timeInPlay` | number | Seconds with a car on the field while the ball is in play |
| `timeSupersonic` | number | Seconds at supersonic speed |
| `timeOnGround`, `timeOnWall`, `timeInAir` | number | Seconds on the ground, on a wall, in the air |
| `averageSpeed` | number | Average speed (km/h) |

```json
{
  "topic": "stats",
  "payload": {
    "Steam|76561198000000000|0|Player1": {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "boostPickups": 12, "boostUsed": 410, "timeInPlay": 245.3, "timeSupersonic": 31.2,
      "timeOnGround": 180.5, "timeOnWall": 12.1, "timeInAir": 52.7, "averageSpeed": 64
    }
  }
}
```

---

## `spectator`

Player followed by the spectator camera. Can be disabled with `rltm_send_spectator`.

| Field | Type | Description |
|---|---|---|
| `target` | object \| null | [Identity of the player](index.md#players) followed by the camera, **`null` with the free camera** (or the ball camera) |
| `camera` | string | Raw name of the camera state given by the game |

```json
{ "topic": "spectator", "payload": { "camera": "…", "target": { "uid": "Steam|…", "name": "Player1", "bot": false, "teamIndex": 0 } } }
```

!!! tip "Overlay of the watched player"
    Show the statistics of `target` (from the [`scoreboard`](#scoreboard) and [`stats`](#stats), with the key of the player), and hide the overlay when `target` is `null`. Wait ~150 ms before hiding it, to avoid a flicker if the camera goes through `null` when switching players.

---

## `players`

Every player of the match, spectators included (`teamIndex` 255).

| Field | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identity of the player](index.md#players) |
| `carId` | number | ID of the car body, `0` without car |

```json
{ "topic": "players", "payload": [
  { "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0, "carId": 23 }
] }
```

---

## `entities`

Balls and cars, sent every `rltm_entities_interval` ms (100 by default) while the match is played: started, not paused, not over. Can be disabled with `rltm_send_entities`.

| Field | Type | Description |
|---|---|---|
| `time` | number | Monotonic time of the sample (ms): use the difference between two updates to interpolate the positions |
| `balls` | object[] | `speed` (km/h) and `location` (`[X, Y, Z]`) of each ball |
| `cars` | object[] | Cars, see below |

A car:

| Field | Type | Description |
|---|---|---|
| `uid`, `name`, `bot`, `teamIndex` | | [Identity of the player](index.md#players) |
| `speed` | number | km/h |
| `location` | number[] | `[X, Y, Z]` |
| `boost` | number | `0` to `100` |
| `isSuperSonic`, `isOnGround`, `isOnWall`, `isInGoal`, `isDodging`, `hasFlip` | boolean | State of the car |

```json
{
  "topic": "entities",
  "payload": {
    "time": 123456789,
    "balls": [ { "speed": 87, "location": [120, -2300, 93] } ],
    "cars": [ {
      "uid": "Steam|76561198000000000|0", "name": "Player1", "bot": false, "teamIndex": 0,
      "speed": 154, "location": [-1500, 800, 17], "boost": 33,
      "isSuperSonic": false, "isOnGround": true, "isOnWall": false,
      "isInGoal": false, "isDodging": false, "hasFlip": true
    } ]
  }
}
```

!!! note
    When the server doesn't read the messages fast enough, the plugin skips the entities updates (they are outdated on the next update anyway) instead of slowing down the game: don't assume a fixed interval, use `time`.

---

## `statistic`

Stat event of a player, as shown by the stat ticker of the game. Followed by an updated [`scoreboard`](#scoreboard) once the match has started.

| Field | Type | Description |
|---|---|---|
| `uid`, `name`, `bot` | | [Identity of the player](index.md#players) |
| `eventName` | string | Name of the event: `Goal`, `Assist`, `Save`, `EpicSave`, `Shot`, `Demolish`, `Demolition`, `Center`, `Clear`, `AerialGoal`, `OwnGoal`... |

```json
{ "topic": "statistic", "payload": { "uid": "Steam|…", "name": "Player1", "bot": false, "eventName": "Demolish" } }
```

---

## `event`

Events of the match, to trigger the animations of the overlays. The state stays in the other topics.

Every event has a `type` and the `clock` (seconds remaining) of the match:

| `type` | Data | Sent |
|---|---|---|
| `kickoff` | | Beginning of each kickoff countdown |
| `goal` | `teamIndex`, `scorer`, `assister`, `isOwnGoal`, `ballSpeed` | ~0.5 s after each goal, see below |
| `overtime` | | Beginning of the overtime |
| `replayStart` / `replayEnd` | | Beginning and end of a goal replay |
| `matchEnded` | `winner` (`0`, `1` or `null`), `score` | End of the match, followed by the [`result`](#result) |
| `podium` | | Beginning of the podium screen |

A goal:

| Field | Type | Description |
|---|---|---|
| `teamIndex` | number | Team scoring the goal |
| `scorer` | object \| null | [Identity of the player](index.md#players) scoring the goal, `null` if not received |
| `assister` | object \| null | Player of the assist, `null` without assist |
| `isOwnGoal` | boolean | Own goal: `scorer` is the player who scored against their team |
| `ballSpeed` | number | Speed of the ball (km/h) |

```json
{ "topic": "event", "payload": {
  "type": "goal", "clock": 120, "teamIndex": 0, "isOwnGoal": false, "ballSpeed": 112,
  "scorer": { "uid": "Steam|…", "name": "Player1", "bot": false, "teamIndex": 0 },
  "assister": null
} }
```

---

## `result`

Complete result of the match, sent **once** at the end of the match, after its last goal. Use it to record the results of a tournament.

| Field | Type | Description |
|---|---|---|
| `guid` | string \| null | Unique ID of the match (see [`match`](#match)) |
| `client` | string | Name of the client |
| `endedAt` | number | End of the match (ms since 1970) |
| `arenaCode` | string | Code of the arena |
| `duration` | number | Duration of the match in the settings (seconds) |
| `timePlayed` | number | Time really played, overtime included (seconds) |
| `isOvertime` | boolean | The match went to overtime |
| `score` | number[] | `[blue, orange]` |
| `winner` | number \| null | Winning team, `null` if the game didn't set it |
| `mvp` | object \| null | [Identity of the player](index.md#players) MVP |
| `teams` | object[] | Teams (see [`match`](#match)) |
| `scoreboard` | object | Final [`scoreboard`](#scoreboard) |
| `stats` | object | Final advanced [`stats`](#stats) |
| `replayName` | string \| null | Name given to the replay with [`set_replay_name`](commands.md#set_replay_name) |

!!! important "Deduplicate the results"
    The `result` is kept by the plugin and **sent again on each connection until the next match ends**, so a server restarted after a match still receives it. Deduplicate it with its `guid` (or `client` + `endedAt` when there is no `guid`).

---

## `response`

Answer to a [command](commands.md) of the server.

| Field | Type | Description |
|---|---|---|
| `command` | string | Name of the command, empty when the message was invalid |
| `id` | string \| number \| null | `id` of the command |
| `success` | boolean | The command was executed |
| `error` | string | Only when `success` is `false`: why the command was rejected |
| `result` | object | Only for some commands, see [Commands](commands.md) |

```json
{ "topic": "response", "payload": { "command": "focus_player", "id": "42", "success": false, "error": "Player not found or camera not available" } }
```
