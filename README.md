# apim-cpp-server

A minimal cross-platform HTTP server written in modern C++ that advertises a handful of API
endpoints for demos and smoke testing. The repository also contains a PowerShell client and a very
simple browser UI so you can exercise the endpoints from multiple contexts.

## Prerequisites

- CMake 3.28+ and Ninja 1.10+ (already available in the provided environment)
- GCC 13+/Clang 16+ (the Windows preset targets MinGW-w64 GCC from MSYS2 UCRT64)
- PowerShell 7+ for the scripted client (Windows PowerShell 5.1 also works)

## Building the server

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

Or pick one of the presets:

```powershell
cmake --preset windows-gcc-ninja
cmake --build --preset windows-gcc-ninja
```

The resulting executable lives at `build/apim-cpp-server` (or the preset-specific binary folder).

## Running the server

```powershell
.\build\apim-cpp-server.exe
```

Configuration knobs:

- `APIM_CPP_HOST` – interface to bind (defaults to `127.0.0.1`)
- `APIM_CPP_PORT` – port to bind (defaults to `8080`)
- `APIM_CPP_LOG_LEVEL` – `error`, `warn`, `info`, or `debug`

The server exposes four demo endpoints:

| Method | Path           | Description                                               |
| ------ | -------------- | --------------------------------------------------------- |
| GET    | `/api/commands`| Lists every sample endpoint                               |
| GET    | `/api/health`  | Reports uptime and version                                |
| GET    | `/api/hello`   | Returns a greeting (optional `name` query parameter)      |
| POST   | `/api/echo`    | Echoes the provided JSON payload                          |

## PowerShell test client

```
pwsh -File .\APIM-CPP-CLIENT\Invoke-DemoRequests.ps1 -Host 127.0.0.1 -Port 8080 -HelloName "Codex"
```

The script enumerates every endpoint, sends the corresponding HTTP request, and prints the prettified
JSON response (or raw body) so you can verify the server end-to-end.

## Browser demo client

Open `APIM-CPP-CLIENT/web/index.html` in any modern browser. Provide the host/port (defaults to
`127.0.0.1:8080`), edit the request payloads if desired, and click a button to send each command.
Responses are shown immediately without needing any runtime dependencies.

## MCP bridge

An MCP-compatible bridge is built as part of the repo (`apim-mcp-bridge`). It exposes the four demo
HTTP endpoints as MCP tools so agents (Cursor, Claude Desktop, etc.) can call them over stdio.

1. Build the bridge target (once per environment):

   ```powershell
   cmake --build build --target apim-mcp-bridge
   ```

2. Ensure `apim-cpp-server` is running locally (or set `APIM_MCP_BASE_URL`).
3. Register the bridge with your MCP host:

   ```powershell
   .\build\apim-mcp-bridge.exe
   ```

4. The host will gain four tools: `apim.list_commands`, `apim.health`, `apim.hello`, and `apim.echo`.
   Their schemas and test coverage live in `docs/mcp_tools.md`. You can run the automated MCP smoke
   test via `ctest --output-on-failure` after building.

## Third-party notice

This repo vendors the single-header [`cpp-httplib`](https://github.com/yhirose/cpp-httplib)
implementation under `third_party/cpp-httplib` (MIT license) to keep the demo HTTP adapter
self-contained.
