# Server integration

This page shows how to receive the data of the plugin in a WebSocket server. The examples use [Bun](https://bun.sh) and [Elysia](https://elysiajs.com), like RLTM, but any WebSocket server works: the plugin only needs a WebSocket endpoint accepting JSON text messages.

## Minimal server

```ts title="server.ts"
import { Elysia } from "elysia";

// Last value of each state topic, sent to the overlays
const state: Record<string, unknown> = {};
const results = new Map<string, unknown>();

new Elysia()
    .ws("/", {
        // Compression proposed by the plugin
        perMessageDeflate: true,

        // Optional: reject the unknown clients during the handshake
        beforeHandle({ request, set }) {
            const token = request.headers.get("authorization");
            if (process.env.TOKEN && token !== `Bearer ${process.env.TOKEN}`) {
                set.status = 401;
                return "Unauthorized";
            }
        },

        open(ws) {
            console.log("Plugin connected:", ws.data.headers["x-rltm-client"]);
        },

        message(ws, message: any) {
            const { topic, payload } = message;

            switch (topic) {
                case "plugin":
                    if (payload.protocol !== 2) console.warn("Unsupported protocol", payload.protocol);
                    break;

                case "result": {
                    // Sent again on each connection: deduplicate it
                    const key = payload?.guid ?? `${payload?.client}-${payload?.endedAt}`;
                    if (payload && !results.has(key)) results.set(key, payload);
                    state.result = payload;
                    break;
                }

                case "statistic":
                case "event":
                    // One-shot messages: trigger the animations of the overlays
                    console.log(topic, payload);
                    break;

                case "response":
                    if (!payload.success) console.warn(`Command ${payload.command} failed: ${payload.error}`);
                    break;

                default:
                    // State topics: the payload replaces the previous one, null clears it
                    state[topic] = payload;
            }
        },

        close() {
            console.log("Plugin disconnected");
        }
    })
    .listen(3000);
```

Connect the plugin to `ws://localhost:3000`.

## Sending commands

```ts
// Focus the camera on a player
ws.send(JSON.stringify({ command: "focus_player", payload: { name: "Player1" }, id: "camera-1" }));

// Name the replay of the current game of the series
ws.send(JSON.stringify({ command: "set_replay_name", payload: { blue: "Team 1", orange: "Team 2", game: 3 } }));
```

Match the answers with the `id` in the [`response`](protocol/messages.md#response) topic.

## Integration checklist

- [ ] Check `protocol` in the [`plugin`](protocol/messages.md#plugin) message.
- [ ] Keep the last value of each **state** topic, and clear it on a `null` payload.
- [ ] Don't expect a state to be sent again if it didn't change: keep it.
- [ ] **Deduplicate the [`result`](protocol/messages.md#result)** with its `guid`: it is sent again on each connection.
- [ ] Read the missing `events` counters of the [`scoreboard`](protocol/messages.md#scoreboard) as `0`.
- [ ] Interpolate the positions of the [`entities`](protocol/messages.md#entities) with their `time`, don't assume a fixed interval.
- [ ] Use the `X-RLTM-Client` header (or `plugin.client`) to tell the computers apart when several matches are played at the same time.
- [ ] Handle the `success: false` [responses](protocol/messages.md#response): a command can be disabled by the user of the plugin.
- [ ] Ignore the unknown topics and fields: new ones can be added without changing the protocol version.
- [ ] Don't send anything else than [commands](protocol/commands.md) to the plugin: any other message is answered with an error.

## Testing

The repository contains a test server, in Python, showing every message of the plugin and sending the commands typed in the terminal (valid ones, and invalid ones to check the robustness of the plugin):

```bash
pip install websockets
python tools/test_server.py --port 3000 --token SECRET
```
