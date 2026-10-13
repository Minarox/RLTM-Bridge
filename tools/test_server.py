"""WebSocket server to test the WSC plugin without RLTM (see docs/TEST-CHECKLIST.md).

Shows the messages of the plugin and sends commands typed in the terminal.

    pip install websockets
    python tools/test_server.py [--port 3000] [--token SECRET] [--all]

By default the entities and stats messages (several per second) are summarized every 5 seconds,
use --all to show every message. Type "help" for the commands.
"""

import argparse
import asyncio
import json
import sys
import time

import websockets

clients = set()
args = None
summary = {"entities": [0, None], "stats": [0, None]}  # topic -> [count since the last summary, last payload]

HELP = """Commands:
  ping | resync | spectate | ready | hide_hud | clear_replay
  focus <player name>                 focus_player on a player
  replay <blue> <orange> <game>       set_replay_name (use _ for spaces in the team names)
  replayname <whole name>             set_replay_name with a whole name
  set <cvar> <value>                  set_setting
  bad                                 invalid JSON
  unknown                             unknown command
  deep                                JSON too deep
  big                                 message too large
  flood                               200 commands at once
  raw <json>                          sends the JSON as is
  quit"""


def short(value, size=300):
    text = json.dumps(value, ensure_ascii=False)
    return text if len(text) <= size else text[:size] + "..."


def show(text):
    print(f"[{time.strftime('%H:%M:%S')}] {text}", flush=True)


async def handler(websocket):
    request = getattr(websocket, "request", None)
    headers = request.headers if request is not None else websocket.request_headers
    client = headers.get("X-WSC-Client", "?")
    authorization = headers.get("Authorization", "")

    if args.token and authorization != f"Bearer {args.token}":
        show(f"Rejected client {client}: wrong token ({authorization or 'no Authorization header'})")
        await websocket.close(code=4001, reason="Invalid token")
        return

    show(f"Client connected: {client} (Authorization: {'yes' if authorization else 'no'}, subprotocol: {websocket.subprotocol})")
    clients.add(websocket)

    try:
        async for message in websocket:
            try:
                data = json.loads(message)
            except ValueError:
                show(f"Invalid JSON from the plugin: {message[:200]}")
                continue

            topic, payload = data.get("topic"), data.get("payload")
            if topic in summary and not args.all:
                summary[topic][0] += 1
                summary[topic][1] = payload
            else:
                show(f"<- {topic}: {short(payload, 2000 if topic in ('result', 'response', 'event') else 300)}")
    except websockets.ConnectionClosed:
        pass
    finally:
        clients.discard(websocket)
        show(f"Client disconnected: {client}")


async def print_summary():
    while True:
        await asyncio.sleep(5)
        for topic, (count, payload) in summary.items():
            if count:
                show(f"<- {topic}: {count} messages in 5 s, last: {short(payload)}")
                summary[topic][0] = 0


def command(name, payload=None, id=None):
    message = {"command": name, "payload": payload or {}}
    message["id"] = id or f"{name}-{int(time.time() * 1000) % 100000}"
    return json.dumps(message, ensure_ascii=False)


def parse(line):
    parts = line.split()
    if not parts:
        return []

    name, rest = parts[0], parts[1:]
    simple = {"ping": "ping", "resync": "resync", "spectate": "spectate", "ready": "ready", "hide_hud": "hide_hud", "clear_replay": "clear_replay_name"}

    if name in simple:
        return [command(simple[name])]
    if name == "focus" and rest:
        return [command("focus_player", {"name": " ".join(rest)})]
    if name == "replay" and len(rest) == 3:
        return [command("set_replay_name", {"blue": rest[0].replace("_", " "), "orange": rest[1].replace("_", " "), "game": int(rest[2])})]
    if name == "replayname" and rest:
        return [command("set_replay_name", {"name": " ".join(rest)})]
    if name == "set" and len(rest) == 2:
        return [command("set_setting", {"name": rest[0], "value": rest[1]})]
    if name == "bad":
        return ['{"command": "ping", ']
    if name == "unknown":
        return [command("format_c_drive")]
    if name == "deep":
        return ['{"command":"ping","payload":' + "[" * 100 + "]" * 100 + "}"]
    if name == "big":
        return [command("ping", {"data": "x" * 20000})]
    if name == "flood":
        return [command("ping", id=f"flood-{i}") for i in range(200)]
    if name == "raw" and rest:
        return [line[4:]]

    print(HELP)
    return []


async def read_commands():
    loop = asyncio.get_running_loop()
    while True:
        line = await loop.run_in_executor(None, sys.stdin.readline)
        if not line or line.strip() == "quit":  # End of the input or quit
            return

        line = line.strip()
        if line == "help" or not line:
            print(HELP)
            continue

        messages = parse(line)
        if messages and not clients:
            show("No plugin connected")
        for websocket in list(clients):
            for message in messages:
                await websocket.send(message)
        if messages and clients:
            show(f"-> {len(messages)} message(s) sent: {messages[0][:200]}")


async def main():
    async with websockets.serve(handler, "0.0.0.0", args.port, subprotocols=["json"], max_size=None):
        show(f"Listening on ws://localhost:{args.port}" + (f" (token required: {args.token})" if args.token else ""))
        print(HELP)
        tasks = [asyncio.create_task(print_summary()), asyncio.create_task(read_commands())]
        await asyncio.wait(tasks, return_when=asyncio.FIRST_COMPLETED)
        for task in tasks:
            task.cancel()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="WebSocket server to test the WSC plugin")
    parser.add_argument("--port", type=int, default=3000)
    parser.add_argument("--token", help="Reject the clients without this token")
    parser.add_argument("--all", action="store_true", help="Show every entities and stats message")
    args = parser.parse_args()

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
