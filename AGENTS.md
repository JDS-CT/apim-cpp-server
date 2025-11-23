# Codex Operating Guide (C++ Edition)

1. PROJECT INITIALIZATION
   1. Ensure AGENTS.md, TODO.md, and PROBLEMS.md exist. No other initialization is required.
   1. Use TODO.md and PROBLEMS.md as active inputs to the iteration cycle. They do not need special formatting beyond having top-level headers.
   1. Verify .gitignore uses an allowlist pattern and properly ignores build/, compiler artifacts, and OS temp files.
   1. PowerShell sessions: set the console to UTF-8 to avoid mojibake when viewing/editing files: `chcp 65001; $OutputEncoding = [Console]::OutputEncoding = [Text.Encoding]::UTF8`. Note: this is safe for PowerShell/.NET usage; if you need to run legacy cmd/batch tools that dislike code page 65001, run them in a separate shell or switch back with `chcp 437`.

1. ITERATION CYCLES
   1. Before selecting tasks, integrate feedback by:
      1. scanning TODO.md and PROBLEMS.md for comments (e.g., "> jds: comments")
      1. updating priorities accordingly
      1. lowering ambiguous or incomplete requests to p3/p4 until clarified
   1. When you draft or revise a planning document (any file matching docs/**/[A-Za-z0-9_]*_plan.md), immediately:
      1. Add a top-level TODO entry referencing the plan
      1. Link from CHANGELOG.md
      1. Update the plan status inside TODO.md
   1. Expand TODO.md into actionable steps using priorities [p1–p4]. Prefer concise, self-documenting tasks.
   1. Implement TODO items in priority order. Write clean, self-contained C++ code. Add tests only for final or stable behaviors to avoid early test bloat.
   1. Write code that is clang-format/clang-tidy clean on first output. Avoid generating code that requires large follow-up fix passes.
   1. After each change, run the validation suite. If anything fails, fix it immediately and record the issue in PROBLEMS.md.
   1. When a task is complete, move it from TODO.md to CHANGELOG.md with an ISO8601 timestamp. Do not leave checked-off entries inside TODO.md.
   1. Keep #include directives at the top of the file. If a required dependency is missing, log an ERROR explaining what is missing and let the process fail fast (do not silently ignore missing includes).
   1. Log boundary events only (API entry/exit, file IO, IPC, network calls) using INFO/WARN/ERROR. Use DEBUG-level logs in core logic only when helpful for troubleshooting, and always guard them with log-level checks so they activate only when verbosity is increased. Do not remove DEBUG logs—keep them dormant by default.
   1. When creating or moving files, update .gitignore immediately and confirm tracking via `git status -sb`.

1. PLATFORM & BUILD MATRIX
   1. Target matrix:
      1. Windows 10+ with GCC via MinGW-w64 (MSYS2 UCRT64 toolchain, invoked from PowerShell/VS Code)
      1. Linux (Ubuntu-class) with GCC as the primary compiler (Clang optional)
   1. Build tools:
      1. CMake is the only supported meta-build system.
      1. Ninja is the default build backend on all platforms (`-G Ninja`).
      1. Avoid MSVC-specific project files or Visual Studio–only settings; any future MSVC support must be wired through CMake presets, not .sln/.vcxproj.
   1. Keep platform-specific code in one place:
      1. Place portable logic in `src/core/**` (ISO C++ + standard library only; no OS headers, no compiler intrinsics).
      1. Place OS/compiler-specific code in `src/platform/**` (e.g., `platform_win32.cpp`, `platform_posix.cpp`).
      1. Public headers used by the rest of the codebase should expose clean interfaces (e.g., `platform_fs.hpp`, `platform_net.hpp`) without leaking OS headers.
   1. Restrict OS and compiler conditionals:
      1. `#if defined(_WIN32)`, `#if defined(__unix__)`, `__GNUC__`, and similar macros are allowed only inside `src/platform/**` and a small `platform_config.hpp` (if needed).
      1. Core business logic must not contain raw `#ifdef _WIN32` / `#ifdef __linux__` blocks.
   1. Restrict platform headers:
      1. `#include <windows.h>`, `<winsock2.h>`, `<ws2tcpip.h>`, `<unistd.h>`, `<sys/socket.h>`, `<sys/stat.h>`, and similar OS headers are allowed only inside platform implementation files.
      1. If a new OS header is required, add it to a platform file and expose the needed behavior via a small, testable function in a portable header.
   1. CMake responsibilities:
      1. Use a single top-level target (e.g., `apim-cpp-server`) with platform sources selected via `CMAKE_SYSTEM_NAME` and `CMAKE_CXX_COMPILER_ID`.
      1. Keep compiler flags centralized in CMake (warnings, sanitizers, defines). Do not hard-code flags in IDE-specific configs.
      1. Prefer CMake presets (e.g., `CMakePresets.json`) for at least:
         1. `windows-gcc-ninja` (MinGW-w64 via MSYS2 UCRT64)
         1. `linux-gcc-ninja`
         1. `linux-clang-ninja` (optional, for additional CI coverage)
   1. Adding new dependencies:
      1. Prefer portable libraries with CMake support.
      1. If a dependency is OS-specific, isolate it behind a platform adapter in `src/platform/**` and document the limitation in PROBLEMS.md or the relevant plan.
      1. Do not introduce dependencies that only work with a single compiler unless a clear fallback exists for the others.

1. MCP INTEGRATION
   1. Scope and goals:
      1. Provide an MCP server that exposes `apim-cpp-server` capabilities as tools for agents and voice clients.
      1. Keep the MCP layer thin: it should delegate to the HTTP API (or other stable backends), not reimplement core logic.
   1. Layout:
      1. Place MCP adapter code under `mcp/apim/**` (e.g., `mcp/apim/mcp_apim_server.py` or `mcp/apim/mcp_apim_server.ts`).
      1. Keep MCP client configuration files (e.g., `mcp.config.json`, `mcp_agent.config.yaml`) in `mcp/` or the repo root, not scattered.
      1. Document exposed tools and their schemas in `docs/mcp_tools.md`.
   1. MCP server behavior:
      1. Each MCP tool should wrap a single HTTP endpoint or a clearly bounded sequence (e.g., `get_health`, `list_commands`, `hello`, `echo_json`).
      1. The MCP server must respect `APIM_CPP_HOST` and `APIM_CPP_PORT` (default `127.0.0.1:8080`) when calling the C++ HTTP API.
      1. Tool names and JSON Schemas (inputs/outputs) must remain stable; breaking changes require a documented version bump and CHANGELOG entry.
      1. Avoid hidden side effects: tools should be explicit about mutations and external I/O.
   1. Testing and automation:
      1. Add a smoke-test script (e.g., `scripts/mcp_smoke_test.ps1` or `scripts/mcp_smoke_test.py`) that:
         1. starts or assumes a running `apim-cpp-server`
         1. calls each MCP tool at least once
         1. asserts basic shape/fields of the responses
      1. When MCP code changes, update or add tests so that CI can exercise both:
         1. direct HTTP endpoints (e.g., via PowerShell client)
         1. the MCP tools that wrap them
   1. Agent and voice control:
      1. New automation features intended for agents or voice clients must be reachable via MCP tools; avoid “special” endpoints used only by one client.
      1. Prefer small, composable tools over giant multi-purpose ones so agents can build their own higher-level workflows.
      1. Keep MCP error messages structured and machine-parseable so agents can recover and retry.
   1. Constraints:
      1. The MCP layer must not access databases or the filesystem directly unless absolutely required; prefer going through the C++ HTTP API or a clearly defined backend service.
      1. Do not embed API keys or secrets in MCP configs; rely on environment variables or the host platform’s secret store.
      1. Any new MCP tool must be accompanied by:
         1. an entry in `docs/mcp_tools.md`
         1. at least one automated test
         1. a short CHANGELOG note if it changes external behavior.

1. HANDLING USER FEEDBACK
   1. Integrate user comments found in TODO.md, PROBLEMS.md, or other markdown files (prefixed with "> jds:" or similar). Treat them as new feedback.
   1. Reprioritize tasks: clear requests become p1/p2; ambiguous requests become p3/p4 until clarified.
   1. If ambiguity is detected, continue working on existing p1/p2 tasks and place unclear items in "Further Feedback / Planning Needed".

1. RELEASE CHECK
   1. Ensure all p1/p2 items are complete.
   1. Ensure no failing tests, no compiler errors, and no clang-tidy warnings.
   1. Ensure CHANGELOG.md documents all changes since last release.
   1. Ensure docs, comments, and schemas match the current code.
   1. Ensure the platform layer compiles on all supported presets (at minimum: `windows-gcc-ninja` and `linux-gcc-ninja`) and that core code builds cleanly regardless of platform.

1. CODE QUALITY CONTRACT
   1. Follow the Stability-Driven Test Model (SDTM):
      1. Prototype code (p3/p4) requires only smoke tests.
      1. Stabilizing code (p2) receives tests once behavior settles.
      1. Stable, core code (p1) gets full behavioral tests to prevent regressions.
   1. Coverage applies only to stable modules. The goal is regression protection, not a numeric target.
   1. All generated code must pass clang-format, clang-tidy, and compile with `-Wall -Wextra -Wpedantic` on first output.
   1. Prefer cohesive modules with clear responsibility boundaries; warn if a module becomes overly large or mixes concerns.
   1. Keep public APIs minimal and avoid god-objects.

1. AUTO-VERIFY
   1. After completing a batch of related changes, run the relevant checks for the areas you touched:
      1. C++: clang-format, clang-tidy, `cmake -S . -B build -G Ninja`, `cmake --build build`, and ctest (targeted paths when possible)
      1. Node/React: pnpm test, pnpm lint, pnpm typecheck (only when frontend code is affected)
   1. For changes touching platform code, ensure at least:
      1. a Linux/GCC configure+build succeeds (e.g., `linux-gcc-ninja` preset)
      1. a Windows/MinGW GCC configure+build is defined and remains valid (e.g., `windows-gcc-ninja` preset), even if not executed in the current environment
   1. Full-suite verification (all C++ and Node checks) is required only before releases, major PRs, or substantial cross-cutting changes.
   1. If any command fails, stop and fix the issue immediately, and record the problem in PROBLEMS.md.
   1. Codex must generate code that already passes clang-format and clang-tidy on first output; verification should confirm correctness, not clean up styling.

1. CONSTRAINTS
   1. Codex PRs must not add or modify binary files that are larger than 50mB. Allowed actions: delete, rename, and move.
   1. When a feature requires binary assets, Codex should produce placeholders, manifests, or instructions for human upload rather than pushing binaries directly.
   1. Keep .gitattributes updated to mark binary formats, and avoid storing large binary payloads in the repository.
   1. *.glb files have given issues when uploaded to the repo via Codex.
