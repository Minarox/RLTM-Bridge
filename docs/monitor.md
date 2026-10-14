---
hide:
  - toc
---

# Monitor

Shows everything the plugin sends and sends it commands, from this page, without a server: to check the data of a match, or to try the commands.

1. In Rocket League: ++f2++ → *Plugins* → *RLTM Bridge* → *Monitor* → check **Enable the monitor**. The plugin opens a WebSocket server only reachable from this computer (`ws://127.0.0.1:8765`). The *Open the monitor page* button opens this page with the right port.
2. Click **Connect** below. The current state of the game is received immediately, then every message as it is sent.

The monitor receives the same messages as the server, even when the plugin isn't connected to a server, and the commands follow the same rules and [permissions](configuration.md#server-control).

!!! note
    Only this site and the local pages (`localhost`, `127.0.0.1`, files) can connect to the monitor of the plugin. The browser may ask to allow this page to access the devices of the local network: accept it.

<div id="rltm-monitor" class="rltm-monitor" data-lang="en" markdown>
*The monitor needs JavaScript.*
</div>
