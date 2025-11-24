# Schema Normalization Plan (Addressing FKs)

Status: Draft (2025-11-23)

## Current state

- `slugs` stores full addressing strings (`checklist`, `section`, `procedure`, `action`, `spec`) plus state, instructions, and `address_id`.
- `relationships` references slugs by `address_id`.
- `history` snapshots mutable fields keyed by `(address_id, timestamp)`.
- Strings are duplicated per row; lookups on addressing fields use text comparisons/indexes.

## Goals

- Reduce string duplication for addressing fields via normalized tables with integer surrogate keys.
- Keep `address_id` stable and derived from the same addressing strings (no algorithm change).
- Enable faster joins/filtering on compact FK columns and improve referential integrity.
- Provide a one-way migration path from the current schema; no legacy dual support.

## Proposed normalized schema

1) Address dimension tables
- `checklists` (`id` INTEGER PRIMARY KEY, `name` TEXT UNIQUE NOT NULL)
- `sections` (`id` INTEGER PRIMARY KEY, `name` TEXT NOT NULL, `checklist_id` INTEGER NOT NULL REFERENCES checklists(id))
- `procedures` (`id` INTEGER PRIMARY KEY, `name` TEXT NOT NULL, `section_id` INTEGER NOT NULL REFERENCES sections(id))
- `actions` (`id` INTEGER PRIMARY KEY, `name` TEXT NOT NULL, `procedure_id` INTEGER NOT NULL REFERENCES procedures(id))
- `specs` (`id` INTEGER PRIMARY KEY, `text` TEXT NOT NULL, `action_id` INTEGER NOT NULL REFERENCES actions(id))

Notes:
- Names remain TEXT to preserve author intent; uniqueness is scoped by parent (e.g., sections unique per checklist, procedures per section, etc.).
- Cascading deletes should be avoided; rely on explicit checklist replacement logic.

2) Slugs
- `slugs`:
  - `address_id` TEXT PRIMARY KEY
  - `checklist_id` INTEGER NOT NULL REFERENCES checklists(id)
  - `section_id`   INTEGER NOT NULL REFERENCES sections(id)
  - `procedure_id` INTEGER NOT NULL REFERENCES procedures(id)
  - `action_id`    INTEGER NOT NULL REFERENCES actions(id)
  - `spec_id`      INTEGER NOT NULL REFERENCES specs(id)
  - `result` TEXT
  - `status` TEXT CHECK (status IN ('Pass','Fail','NA','Other','Unknown'))
  - `comment` TEXT
  - `timestamp` TEXT
  - `instructions` TEXT

3) Relationships
- `relationships`:
  - `subject_id` TEXT NOT NULL REFERENCES slugs(address_id)
  - `predicate` TEXT NOT NULL
  - `target_id` TEXT NOT NULL REFERENCES slugs(address_id)
  - (Optionally) add an index on `(predicate)` if needed for predicate-filtered queries.

4) History
- `history`:
  - `address_id` TEXT NOT NULL REFERENCES slugs(address_id)
  - `timestamp` TEXT NOT NULL
  - `result` TEXT
  - `status` TEXT
  - `comment` TEXT
  - PRIMARY KEY (`address_id`, `timestamp`)

## Address ID computation

- Remains derived from the addressing strings (`checklist`, `section`, `procedure`, `action`, `spec`), not the integer IDs. On ingestion, compute the same canonical string and hash; store the strings in their dimension tables.
- When exporting/serving, return the original strings to clients; the normalization is an internal storage concern.

## Migration strategy

1. Create new dimension tables and populate them by distinct selection from existing `slugs` addressing fields, preserving parent-child relationships.
2. Create a new `slugs` table with FK columns and backfill rows by joining to dimension tables on the addressing strings; preserve `address_id`, state, instructions, timestamp.
3. Rebuild `relationships` and `history` tables unchanged (they remain keyed by `address_id`).
4. Replace old `slugs` with the normalized version inside a transaction; add indexes:
   - `idx_slugs_checklist_id` on `slugs(checklist_id)`
   - `idx_slugs_section_id` on `slugs(section_id)`
   - `idx_slugs_procedure_id` on `slugs(procedure_id)`
   - `idx_slugs_action_id` on `slugs(action_id)`
   - `idx_slugs_spec_id` on `slugs(spec_id)`
5. Update code to:
   - Resolve or insert addressing strings into dimension tables on ingest/import.
   - Join to dimension tables when exporting or answering `/api/checklists` (distinct on `checklists.name`).
   - Keep `address_id` computation based on strings, not IDs.

## Impacts

- Lookup speed improves for addressing filters; string duplication is reduced.
- Complexity increases: ingest/update paths must manage dimension lookups/inserts.
- Schema migration required; backups recommended.

## Design decisions

- Uniqueness and immutability:
  - Identity layer uses pooled text tables (`*_texts`) with `UNIQUE(text)` and immutable rows (only `INSERT`/`DELETE`, no `UPDATE`) for `checklist`, `section`, `procedure`, `action`, `spec`.
  - Structural layer prevents duplicate slugs with the same full `(checklist, section, procedure, action, spec)` tuple.
  - Ingestion normalizes inputs (trim/canonical rules if adopted), looks up pooled text by canonical string, inserts if absent. Any identity change produces a new `address_id` and new slug; old rows remain until pruned.

- Performance stance:
  - Start with normalized tables + immutable string pools + indexes on FK columns (`checklist_id`, `section_id`, `procedure_id`, `action_id`, `spec_id`).
  - Expose read-only SQL views (logical, not materialized) to reconstruct API/MCP shapes by joining slugs to pooled text tables.
  - Add denormalized exports or materialized caches only if profiling shows joins are a bottleneck, and keep any cache explicitly refreshed and non-canonical.

- Canonical string handling:
  - Do not cache the canonical addressing string in `slugs`; reconstruct via joins to immutable string pools.
  - Keep `address_id` hashing in application code using the concatenated strings.
  - `slugs` holds `address_id`, identity FKs, and mutable fields (`result`, `status`, `comment`, `timestamp`, `instructions`); relationships stay keyed by `address_id`.
  - Identity changes create new slugs with new `address_id`s; old slugs remain unchanged to avoid silent global renames.
