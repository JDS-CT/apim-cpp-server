# MCP Tools

This file tracks every Model Context Protocol tool exposed by the repository. The native
`apim-mcp-bridge` binary proxies requests to the local `apim-cpp-server` HTTP API so agent hosts can
interact with the backend using the MCP transport.

## Tool catalog

| Tool name            | Method/Path     | Description                                                     | Arguments                        |
| -------------------- | --------------- | --------------------------------------------------------------- | -------------------------------- |
| `apim.list_commands` | `GET /api/commands` | Returns the canonical list of demo HTTP commands.                | _none_                           |
| `apim.health`        | `GET /api/health`   | Reports readiness, uptime, and server version metadata.          | _none_                           |
| `apim.hello`         | `GET /api/hello`    | Sends a greeting to the provided `name` (defaults to `world`).   | `name` _(string, optional)_      |
| `apim.echo`          | `POST /api/echo`    | Echoes the JSON payload that agents supply for smoke testing.    | `payload` _(string, required)_   |

## Usage

1. Build and launch the MCP bridge:

   ```powershell
   cmake --build build --target apim-mcp-bridge
   .\build\apim-mcp-bridge.exe
   ```

2. Ensure the C++ server is running locally (default `127.0.0.1:8080`). The MCP bridge reads the
   same `APIM_CPP_HOST` and `APIM_CPP_PORT` environment variables; you can also override everything
   with `APIM_MCP_BASE_URL`.
3. Point your MCP host (Cursor, Claude Desktop, etc.) at the running bridge. The host can now call
   the four tools and receive structured responses.

## Testing

`mcp-bridge-test` (invoked via `ctest`) spins up a lightweight HTTP stub and verifies the MCP bridge
implements `tools/list`, `apim.hello`, and `apim.echo` end-to-end.

```powershell
ctest --output-on-failure
```
