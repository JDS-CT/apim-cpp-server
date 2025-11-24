# apim-cpp-server

A cross-platform HTTP server written in modern C++ with a SQLite-backed checklist runtime. It
implements the minimal update contract from `docs/design/apim_checklist_specification.md`, ships with
PowerShell + MCP clients for agents, and includes a browser console that follows the CAPTCHA and
indicator plans.

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
- `APIM_CPP_DB` – SQLite runtime store path (defaults to `.apim/checklists.db`)
- `APIM_CPP_SEED_DEMO` – set to `0`/`false` to skip seeding demo slugs

The server exposes the checklist runtime API:

| Method | Path                            | Description                                                 |
| ------ | ------------------------------- | ----------------------------------------------------------- |
| GET    | `/api/commands`                 | Lists every API endpoint                                    |
| GET    | `/api/health`                   | Readiness, uptime, and version metadata                     |
| GET    | `/api/hello`                    | Greeting (optional `name` query parameter)                  |
| POST   | `/api/echo`                     | Echoes the provided JSON payload                            |
| GET    | `/api/checklists`               | Lists every checklist in the runtime store                  |
| GET    | `/api/checklist/<checklist>`    | Returns slugs for the given checklist                       |
| GET    | `/api/slug/<address_id>`      | Returns a single slug by Address ID                       |
| GET    | `/api/relationships/<id>`       | Incoming/outgoing relationships for the slug                |
| PATCH  | `/api/update`                   | Minimal update contract (result/status/comment/timestamp)   |
| PATCH  | `/api/update_bulk`              | Minimal update contract applied to many slugs               |
| GET    | `/api/export/json`              | Export all slugs as a JSON array                            |
| GET    | `/api/export/jsonl`             | Export all slugs as JSON Lines                              |
| GET    | `/api/export/markdown/<checklist>` | Export a checklist as canonical Markdown for authors     |
| POST   | `/api/import/markdown?checklist=<name>` | Import Markdown for a checklist and replace its runtime state |

## PowerShell test client

```
pwsh -File .\APIM-CPP-CLIENT\Invoke-DemoRequests.ps1 -ServerHost 127.0.0.1 -Port 8080 -HelloName "Codex"
```

The script enumerates every endpoint, sends the corresponding HTTP request (including the minimal
update contract when seeded data is available), and prints the prettified JSON response (or raw
body) so you can verify the server end-to-end.

### Automation scripts

To run and monitor the server without leaving terminals blocked, use the helper scripts:

```powershell
pwsh .\tools\automation\start_apim_server.ps1   # launches in the background, waits for /api/health
pwsh .\tools\automation\stop_apim_server.ps1    # stops the tracked PID and cleans up
```

Logs are written to `.apim/logs`, and the PID lives at `.apim/server.pid`. When you need to rebuild
while a server binary is running, either stop it via the script above or point CMake at a new build
tree (e.g., `cmake -S . -B build-new -G Ninja`) so you can compile and test without interrupting
stakeholders who are connected to the existing process.

## Web test console (man page)

The HTML dashboard at `APIM-CPP-CLIENT/web/index.html` is the canonical "man page" for stakeholders.
It opens with the CAPTCHA landing (human + AI paths) and links to `portal.html`, which hosts the
human portal UI (checklist table) and the Testing Hub (copy-only commands for the smoke/full suites).

Serve it locally so browsers can reach `http://127.0.0.1:8080` without CORS issues:

```powershell
pwsh -File .\APIM-CPP-CLIENT\web\Start-WebDemo.ps1 -Port 8081
```

Then open `http://127.0.0.1:8081` to view the handbook and trigger HTTP requests directly.

## MCP bridge

An MCP-compatible bridge is built as part of the repo (`apim-mcp-bridge`). It exposes the checklist
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

4. The host will gain tools such as `apim.list_commands`, `apim.health`, `apim.get_slug`,
   `apim.update_slug`, and `apim.export_json`. Schemas and coverage live in `docs/mcp_tools.md`.
   You can run the automated MCP smoke test via `ctest --output-on-failure` after building.

## Testing

- Unified runner: `scripts/run_tests.ps1 -Label smoke` (fast) or `scripts/run_tests.ps1 -Label all`
  (full suite).
- CTest labels:
  - `smoke`: MCP bridge test + schema normalization integration test.
  - `all`: runs every registered test (currently same as `smoke` until more tests are added).

## Third-party notice

This repo vendors the single-header [`cpp-httplib`](https://github.com/yhirose/cpp-httplib),
[`sqlite`](https://sqlite.org), [`xxHash`](https://github.com/Cyan4973/xxHash), and
[`nlohmann/json`](https://github.com/nlohmann/json) implementations under `third_party/` to keep the
demo HTTP adapter and runtime store self-contained.

