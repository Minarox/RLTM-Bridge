# Development

## Project structure

```
RLTMBridge.sln
RLTMBridge/
├── RLTMBridge.h / .cpp    Plugin class, load / unload and game hooks
├── Settings/              Cvars, console commands and settings window
├── WebSocket/             Connection to the server, messages, server commands and local monitor
├── Protocol/              Messages format, without the BakkesMod SDK (unit tested)
├── Game/                  Game data, statistics, events, result and actions (HUD, spectator, replays)
├── IMGUI/, fmt/           Third-party libraries
└── version.h              Version of the plugin
tests/                     Unit tests of the messages format
tools/test_server.py       Test server
docs/                      This documentation (MkDocs)
```

The plugin is one class, `RLTMBridge`, declared in `RLTMBridge.h` and implemented in several source files by category. The game is only accessed from the game thread: the network thread of the WebSocket library only queues the commands of the server, executed on the next frame.

## Build

Requirements: Windows, Visual Studio 2022 with the C++ workload, [vcpkg](https://vcpkg.io) integrated with Visual Studio, and BakkesMod installed.

1. Open `RLTMBridge.sln`.
2. Build the `Release | x64` configuration.

vcpkg installs the dependencies (IXWebSocket with TLS, nlohmann/json) from `vcpkg.json`, and the BakkesMod SDK is found from the BakkesMod installation (`BakkesMod.props`). The DLL is built in `plugins/` and patched for BakkesMod by the post-build step.

## Tests

The messages format (`RLTMBridge/Protocol`) doesn't depend on the BakkesMod SDK and is unit tested on any platform:

```bash
sh tests/run.sh   # requires nlohmann/json, e.g. apt install nlohmann-json3-dev
```

The behavior in game is tested manually with the checklist of the repository (`docs/TEST-CHECKLIST.md`, in French) and the test server (`tools/test_server.py`).

## Continuous integration

The **Build** workflow runs on each push and pull request:

- `test`: unit tests on Linux,
- `build`: MSVC build with vcpkg and the BakkesMod SDK, the DLL is uploaded as the `RLTMBridge` artifact,
- `release`: on a `v*` tag, creates a GitHub release with the DLL.

The build number of the version (`VERSION_BUILD` in `version.h`) is the number of the CI run. Local builds keep the one of `version.h`.

The **Docs** workflow builds this documentation on each push and pull request, and publishes it on GitHub Pages from the `main` branch.

## Documentation

The documentation is written in Markdown in `docs/` and built with [MkDocs Material](https://squidfunk.github.io/mkdocs-material/). Each page exists in English (`page.md`) and in French (`page.fr.md`).

```bash
pip install -r docs/requirements.txt
mkdocs serve   # http://127.0.0.1:8000
```

## Protocol changes

When changing the messages:

- add fields and topics without breaking the existing ones when possible,
- otherwise increase `protocol_version` in `RLTMBridge.h`,
- update the [protocol documentation](protocol/index.md), the [changelog](changelog.md) and the unit tests.
