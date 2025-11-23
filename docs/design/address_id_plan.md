# Address ID Rename Plan

Status: Implemented (2025-11-23T07:15:06-05:00)

## Context

`checklist_id` currently names the deterministic address for a slug (checklist + section + procedure + action + spec). The term collides with “checklist” as a whole and reads as checklist-level scope. We want to rename it to **Address ID** (`address_id` / `addressId`) across code, storage, APIs, docs, and clients.

## Goals

- Use “Address ID” consistently to mean the slug address hash.
- Keep the hashing inputs and output stable (same 16-char Base32 value), only the field and symbol names change.
- Avoid long-term dual-field “legacy” support; prefer a clean, early rename with a one-time migration path.

## Scope (rename surfaces)

- Core C++:
  - Functions: `ComputeChecklistId` → `ComputeAddressId`.
  - Struct fields: `checklist_id` members in `ChecklistSlug`, update types and accessors.
  - Storage: DB column names, indexes, and foreign keys in `slugs`, `relationships`, `history` tables.
  - HTTP handlers/payloads: request/response JSON keys; error messages; logs.
- API routes:
  - Path params: `/api/slug/<address_id>`, `/api/relationships/<address_id>`.
  - Bodies/queries: PATCH `/api/update` expects `address_id`.
  - Markdown import/export surfaces that show the ID label.
- MCP bridge:
  - Tool schemas, descriptions, parameter names, and responses.
- Clients:
  - Web console labels, state keys, fetch calls, selectors, and display of IDs.
  - PowerShell demo scripts and any example payloads.
- Docs and samples:
  - Checklist specification sections naming “Checklist ID”.
  - README API table, docs/mcp_tools.md, any prose or code snippets.
- Tests:
  - ctests, MCP bridge tests, sample JSON comparisons.

## Non-goals

- Do not change the hash algorithm or inputs.
- Do not yet normalize schema (separate checklist/section/procedure/action/spec tables) — tracked separately.

## Migration approach (storage and API)

- Storage migration (SQLite):
  - Implemented: rename legacy tables, recreate with `address_id` columns and updated FKs/indexes, copy data (`checklist_id` → `address_id`), drop legacy tables inside one transaction, then recreate indexes.
  - Fallback (unused): add `address_id` columns, backfill from `checklist_id`, then rebuild tables to drop legacy columns.
- API break:
  - Make a clean cut to `address_id` keys and paths; update clients/tests in the same change.
- Do not accept `checklist_id` keys; this is a clean break.

## Execution steps

1) Storage and core rename
   - Rename hashing function and struct fields to `address_id`.
   - Update all SQL statements, bindings, column names, and indexes.
   - Wire HTTP handlers, serializers, and log messages to the new names.
2) API and client surfaces
   - Update routes, payload keys, and OpenAPI-style descriptions in code.
   - Patch MCP tool schemas and tests.
   - Update web console and PowerShell demo to the new key and labels.
3) Docs and samples
   - Rename “Checklist ID” to “Address ID” across spec/README/mcp docs and Markdown export/import labels.
4) Migration utilities
   - Provide a migration note or script to rebuild/copy the SQLite store (or rely on re-import from Markdown for early adopters).
5) Validation
   - Run full test suite; add/refresh tests expecting `address_id` keys and API paths.

## Risks and mitigations

- Client breakage if any surface still expects `checklist_id`: mitigate with exhaustive search and tests; optional short-lived dual-acceptance if needed.
- Migration failures: keep a reversible backup of the DB and/or support re-import from Markdown as a fallback.

## Open questions

- Whether to keep a short compatibility window accepting `checklist_id` inputs (default answer: no, unless a blocker emerges).
- Whether to align SQLite schema migration with the future normalization effort to avoid two migrations.
- Markdown: imports require `**Address ID:**`; exports emit `**Address ID:**`.
