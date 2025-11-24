# Python Portal Migration Plan

Status: Draft (2025-11-23)

## Goal

Reuse the visual patterns from `APIM-CPP-CLIENT/web/pythonPortal/*.html` while wiring them to the current C++ backend/API/MCP/test contracts. Strip legacy Flask calls and “pile-of-files” data handling; treat the pythonPortal files as UI inspiration/templates only.

## Scope

- Leave `pythonPortal` pages as reference snapshots; do not wire them directly.
- Build a new portal shell that mirrors the desired layout/interactions but calls the current HTTP API and MCP tools.
- Ensure every UI action has a matching CLI/API/MCP path and test hook.

## Workstreams

1) UI harvesting
- Identify the visual/layout elements to keep (portal navigation, cards, tables, modals).
- Create fresh HTML/CSS/JS (outside `pythonPortal/`) that reproduces the layout without legacy JS.
- Remove/ignore all old Flask/API bindings in the reference files; annotate buttons as “TODO: wire to API” until connected.

2) API/CLI/test alignment
- For each UI action, map to:
  - HTTP endpoint (existing or to add),
  - MCP tool (existing or to add),
  - CLI/test invocation (e.g., via `scripts/run_tests.ps1`, ctest labels, or a targeted script).
- Keep one source of truth per action; UI should call the same contract used by CLI/tests.

3) Data model alignment
- Use the normalized schema (address_id, FK dimensions) and current API payload shapes.
- No direct file-store/pile-of-files patterns; all data flows through the HTTP API.

4) Testing
- UI: smoke endpoints only; avoid long-running tests in the browser.
- CLI/CI: use `ctest` labels (`smoke`, future `full`) and scripts in `scripts/`.
- Add references from UI buttons to their corresponding test entry points (comments/ids).

## Deliverables

- New portal shell (separate from `pythonPortal/`) with the harvested layout and no legacy JS calls.
- Mapping table of UI actions → HTTP API → MCP tool → CLI/test hook.
- Removal plan for `pythonPortal/` once the new shell is complete.

## Open items / questions

- Must-keep vs. optional:
  - Keep: checklist portal.
  - Optional repurpose: overview portal as a Testing/Diagnostics hub skeleton (keep portal links top-left, subcategory links top-right; drop legacy tests and inject CPP/CLI/MCP/UI test affordances).
- If endpoint/tool gaps appear, leave labeled placeholders/dead buttons and align labels with the current spec (`Ind`). Wire in future revisions as needed.
