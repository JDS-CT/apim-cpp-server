# CHANGELOG

- 2025-11-23T07:15:06-05:00 (p1) Renamed checklist_id to address_id across storage, API routes/payloads, MCP tools, web/PowerShell clients, docs, and tests; added SQLite migration and legacy Markdown label compatibility.
- 2025-11-23T07:42:12-05:00 (p1) Removed legacy Checklist ID compatibility; Markdown imports now require Address ID and legacy schema migration paths were dropped.
- 2025-11-23T07:04:41-05:00 (p1) Drafted docs/design/address_id_plan.md to rename checklist_id to address_id across schema, API, MCP, clients, and docs; logged TODO for execution.
- 2025-11-23T05:49:28-05:00 (p1) Documented action-vs-procedure principles in docs/design/principles.md to justify the action-first portal layout for field users.
- 2025-11-22T12:51:14-05:00 (p1) Implemented the SQLite-backed checklist runtime (deterministic IDs via xxHash/Base32), relationships, bulk updates, and exports aligned with the checklist specification.
- 2025-11-22T12:51:14-05:00 (p1) Rebuilt the web console with the CAPTCHA landing, indicator-driven checklist board, and inline minimal-update workflow for humans and agents.
- 2025-11-22T12:51:14-05:00 (p2) Expanded MCP and PowerShell clients, docs, and build/test coverage to surface the new API endpoints and runtime store configuration.
- 2025-11-22T12:28:01-05:00 (p3) Registered active plans: docs/design/captcha_plan.md and docs/design/indicator_plan.md for the web landing + indicator work.
- 2025-11-16T13:29:44-05:00 (p1) Bootstrapped the hello-world HTTP server with CMake scaffolding and the cross-platform HTTP adapter.
- 2025-11-16T13:29:44-05:00 (p1) Added PowerShell and HTML demo clients for exercising the API endpoints.
- 2025-11-16T13:29:44-05:00 (p2) Documented build, configuration, and demo client usage in README.md.
- 2025-11-16T13:54:00-05:00 (p1) Added a Python-based MCP bridge plus automated tests to expose the HTTP API as MCP tools.
- 2025-11-16T13:54:00-05:00 (p2) Documented the MCP tool catalog and updated README with bridge usage instructions.
- 2025-11-16T18:31:26-05:00 (p1) Replaced the Python MCP bridge/tests with native C++ targets, plus a reusable HTTP client and ctest coverage.
- 2025-11-16T18:31:26-05:00 (p2) Updated README and docs/mcp_tools.md to describe the non-Python workflow and recorded the resolved MSYS2 toolchain issue.
