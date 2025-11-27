# APIM Checklist Specification

- Version: 0.0.2-draft
- Date: 2025-12-18T20-33-00Z
- Organization: cvmewt
- Project: APIM
- Repository: https://github.com/JDS-CT/APIM.git
- License: MIT (for reference implementation code)
- Git-Tracked: yes

This document defines the canonical specification for:

- Checklist slugs
- Address IDs
- The canonical data model (addressing, state, instructions, relationships)
- The Markdown authoring format
- The SQLite runtime storage model
- Reference API and JSON/JSONL encodings

The specification is technology-agnostic at the conceptual level, but provides a concrete reference implementation using:

- Markdown for human authoring
- SQLite for runtime state storage
- HTTP+JSON/JSONL for API transport

Other views (HTML forms, static PDFs, dashboards, etc.) are derived from the canonical model and the SQLite runtime store.

---

## 1. Scope and Purpose

APIM checklists are intended to be **active SOPs**: procedures that are authored, executed, updated, and reviewed continuously, rather than static documents that drift out of sync with real work.

This specification defines:

1. **The canonical data model** of a checklist slug:  
   - addressing fields  
   - state fields  
   - instructions  
   - relationships  
   - deterministic Address ID  
   - timestamp (ISO 8601 UTC, last update)  
   - entity_id (16-character Base32 identifier for the last updating entity)

2. **Canonical representations:**
   - **Markdown** as the canonical human authoring format (client-side)
   - **SQLite** as the canonical runtime store (server-side)

3. **Reference encodings and interfaces:**
   - A minimal API update contract: `Address ID + field(s) to update`
   - Example JSON/JSONL payloads for transport
   - A relationship model for dependency graphs between procedures

Operationally:

- Stakeholders author or revise procedures in Markdown using a standard template.
- A parser ingests Markdown into the canonical data model and persists it in SQLite.
- Runtime systems (CLI, UI, automations, MCP tools) interact with a server that exposes the state via an API backed by SQLite.
- The only required information to update a procedure state at runtime is a `address_id` plus one or more state fields; the runtime regenerates `timestamp` and `entity_id` from the current execution context.

The specification is written to allow:

- **Provisional rows** (new procedures added by front-line users) that are immediately executable, and can later be reviewed/edited without breaking IDs.
- **Deterministic updates**: given a stable set of addressing fields, the same procedure always has the same Address ID.

---

## 2. Terminology

For this specification, the following terms are used:

- **Checklist**  
  A collection of related procedures grouped for a specific context (e.g., PM visit, site survey, installation).

- **Checklist Slug**  
  A single procedure definition and its current state. Conceptually, one row in the checklist that can be addressed, updated, and related to others.

- **Addressing Fields**  
  The canonical administrative fields that locate a slug within a checklist:
  `checklist`, `section`, `procedure`, `action`, `spec`.

- **State Fields**  
  Mutable fields and runtime metadata that represent the current outcome of a procedure:
  `result`, `status`, `comment`, `timestamp`, `entity_id`.

- **Entity**  
  Any actor that mutates slug state (human, automation, background job, or agent). Entities are identified by an `entity_id` and may have additional metadata in a separate catalog.

- **Entity ID**  
  A 16-character Crockford Base32 identifier for the entity that last updated a slug's state. It is:
  - opaque (no embedded semantics),
  - stable per entity within a deployment,
  - set by the runtime from authentication or system context,
  - not authored in Markdown and not supplied by clients in minimal update calls.
  Clients obtain the active `entity_id` from the server (auth/session metadata or a discovery endpoint), and the runtime echoes it in responses.

- **Instructions**  
  Freeform text providing detailed guidance, SOP steps, or references associated with a slug.

- **Address ID**  
  A deterministic 16-character Base32 identifier derived from the addressing fields. Used for relationships, API updates, and storage. Recomputed from addressing fields; not directly edited by humans.

- **Semantic Relationship**  
  A directed triple of the form:
  `(subject_address_id, predicate, target_address_id)`.

- **Relationship Predicate**  
  A lowercase ASCII token (e.g., `depends_on`, `fulfills`, `satisfied_by`) describing how one slug relates to another.

- **Runtime Store**  
  The SQLite database that holds all slugs, relationships, and history for a given deployment.

- **Connector**  
  Any export/import mechanism that maps between the runtime store and external systems (JSON/JSONL exports, JSON-LD, RDF triples, etc.).

- **State Machine**  
  The logic that consumes current slug states and relationships to derive or validate updated states (for example, enforcing `depends_on` constraints or time-based rules).

---

## 3. Canonical Data Model

This section defines what a checklist slug **is**, independent of any specific format (Markdown, JSON, SQLite schema, etc.).

A **checklist slug** consists of:

1. **Addressing fields**  
2. **State fields**  
3. **Instructions**  
4. **Relationships**  
5. **Address ID**  
6. **Timestamp (ISO 8601 UTC, last update)**  
7. **Entity ID (opaque identifier of last updating entity)**

Each slug represents one actionable procedure step in a checklist. The timestamp and entity ID record the most recent mutation metadata; they are not high-frequency telemetry channels (see Section 6.4).

### 3.1 Addressing Fields

Addressing fields identify and locate a checklist slug within a checklist.  
They define the stable semantic address of a procedure and are the sole inputs used to compute the `address_id`.

#### `checklist`
A stable identifier for the checklist as a whole. Typically derived from the filename or a system-assigned slug.  
Groups related procedures under the same operational context (e.g., PM, Survey, Installation).

#### `section`
A logical grouping within a checklist (e.g., “Baked Goods”, “Ingredients”).  
Represents major subdivisions that help organize large checklists.

#### `procedure`
A short **noun phrase** naming the underlying work item (e.g., “Multi-grain bread”).  
Used for planning, analysis, conceptual grouping, and higher-level discussions.  
Soft limit: ~28 characters for dense UI rendering.

#### `action`
A short **verb phrase** describing the operator-facing task (e.g., “Make multi-grain bread”).  
Used by execution systems, runtime prompts, and operator workflows.  
Soft limit: ~28 characters.

**Design Rationale:**  
Although `procedure` and `action` may be linguistically similar, they serve distinct roles.  
`procedure` expresses the *conceptual* identity of the work item, while `action` expresses the *executable task*.  
Maintaining both fields supports planning interfaces, operator UIs, analytics, and LLM-based assistants.  
This dual representation creates a stable bridge between conceptual (planning/analysis) and operational (execution/action) workflows with negligible storage overhead.

#### `spec`
A concise description of the expected target or standard for the action (e.g., “18–22 °C”).  
Used for verification and automated status logic.  
Soft limit: ~28 characters.

These addressing fields collectively define the canonical identity of a slug.  
Any change to an addressing field produces a new `address_id`.


### 3.2 State Fields

State fields capture the mutable outcome of executing the procedure.  
They are evaluated against the addressing fields—especially `spec`—to determine the current status of a slug.

#### `result`
A concise description of the observed outcome, formatted for direct comparison with `spec`.  
Example: typing “20” when the spec is “18–22 °C” provides enough context for automated or manual evaluation.  
Soft limit: ~28 characters.

Provides the primary input to automated or manual pass/fail evaluation.
`result` should be a short, measurable, or directly comparable statement whenever possible.

#### `status`
An enumerated value representing the evaluation outcome:
- `Pass`
- `Fail`
- `NA`
- `Other`

`status` may be set manually or derived through simple logic comparing `result` to `spec`.

#### `comment`
Freeform text for additional context, deviations, or clarifications. Soft UI limit: ~80 characters (higher limits permissible in storage).

Comments often serve as:
- justification for `Fail` or `Other`,
- explanation of `NA` when the spec does not apply,
- feedback for checklist authors to improve procedure clarity.

#### `timestamp`
ISO 8601 UTC timestamp indicating when this slug was last updated (e.g., `2023-10-01T12:00:00Z`).

#### `entity_id`
A 16-character Crockford Base32 identifier for the entity (human, system, or agent) that last updated the slug's state.

- Format: see Section 11.4.1 (Entity ID Encoding)  
- Set by the runtime based on authentication or system identity  
- Not authored directly in Markdown or accepted from minimal update payloads  
- Discoverable by clients via authentication/session metadata or a server-provided endpoint; echoed by the runtime in read/update responses

State fields are mutable and may change over time; they do not affect the `address_id`. Timestamp and `entity_id` are runtime-managed metadata regenerated on each successful update.

### 3.2 State Fields

State fields capture the mutable outcome of executing the procedure:

- `result`  
  A concise, human-readable summary of the observed result (soft limit ~28 characters).
- `status`  
  Evaluation outcome (`Pass`, `Fail`, `NA`, or `Other`).
- `comment`  
  Freeform clarifying text.
- `timestamp`  
  Runtime-managed ISO 8601 UTC marker of the last update.
- `entity_id`  
  Runtime-managed 16-character Crockford Base32 identifier for the actor that performed the last update.


These fields may change over time; addressing fields must remain stable for the slug to retain the same `address_id`. Timestamp and `entity_id` are regenerated by the runtime on each successful write.

### 3.3 Instructions

`instructions` is a freeform text field providing detailed steps, clarifications, and operational context for completing the action.

- Intended for short SOP-style guidance associated with the slug.
- Recommended soft limit: ~500 characters for storage efficiency.
- Longer materials (images, diagrams, videos, or extensive SOPs) should be linked rather than embedded.

Instructions may include references to external documents, training modules, or videos when tacit knowledge or hands-on demonstration is required. These external links serve as durable pointers without inflating the size of the SQLite database.

### 3.4 Relationships

Each slug may reference other slugs using semantic relationships. Relationships are represented conceptually as directed triples:

- `subject_address_id` — the slug defining the relationship  
- `predicate` — the relationship type (e.g., `depends_on`, `fulfills`)  
- `target_address_id` — the slug being referenced

Although relationships are **authored as outgoing links** from the subject slug, the system must also expose **incoming links** when displaying or querying a checklist. This prevents stakeholders from unintentionally duplicating existing relationships simply because they originated in another checklist or section.

In the runtime store, relationships are queryable in both directions:

- **Outgoing**: all targets referenced by this slug  
- **Incoming**: all slugs that reference this slug as a target

This bidirectional visibility supports correct editing, review, cross-checklist coordination, and dependency analysis, even though the underlying data model stores each relationship only once.

The relationship graph supports:

- dependency constraints (`depends_on`)  
- hierarchical or roll-up structures (`fulfills`, `satisfied_by`)  
- additional predicates defined in the relationship model (see Section 8)

### 3.5 Address ID

Every slug has a **Address ID**:

- Computed deterministically from the addressing fields only.
- Represented as a 16-character Crockford Base32 string (uppercase).
- Used as the primary key for:
  - API updates
  - relationship edges
  - references in external systems

The hashing and encoding algorithm is defined in detail in Section 4. The important property is that **addressing fields uniquely determine the Address ID**, and the ID is never edited directly by users.

## 4. Address ID Specification

The `address_id` is the stable, deterministic identifier for a checklist slug.  
It is derived solely from the addressing fields (`checklist`, `section`, `procedure`, `action`, `spec`) and is unaffected by state fields or instructions.

### 4.1 Purpose

The Address ID serves as:

- the stable primary key in the runtime store (SQLite),
- the target and source of relationship triples,
- the addressing token for all API updates,
- a durable reference for external systems, exports, and reports.

Two slugs with identical addressing fields must always produce the same `address_id`.  
Any change to an addressing field produces a new ID.

### 4.2 Canonical Addressing String

The Address ID is computed from a canonical, delimiter-encoded string:

<checklist> || <section> || <procedure> || <action> || <spec>

Rules:

- `||` is the reserved delimiter.
- Whitespace is preserved exactly.
- None of the addressing fields may contain the literal sequence `||`.

This canonical string forms the input to the hashing step.

### 4.3 Hashing and Encoding

Address IDs use a compact 16-character Crockford Base32 string derived as follows:

1. Compute the **xxHash128** of the canonical addressing string.  
2. Extract the **lowest 80 bits (10 bytes)** of the hash output.  
3. Encode the 10-byte value in **Crockford Base32**, uppercase.

This yields a **16-character**, URL-safe, clipboard-friendly identifier.

Example:
F7W4X2J9T3B6P8KD

### 4.4 Properties

- Extremely low collision probability even at large scale  
  (~1 in 2.5×10¹⁵ at 100k procedures).
- Deterministic — identical addressing fields always yield the same ID.
- Human-friendly:
  - uppercase,
  - no ambiguous characters,
  - double-click selects entire token in common editors,
  - safe in URLs, filenames, and code.
- ID is **not reversible** mathematically, but is resolvable via database lookup.

### 4.5 Regeneration Rules

- IDs are **never edited** by authors or operators.  
- The ID is always **recomputed** during parsing from the addressing fields.  
- If authors revise addressing fields in Markdown, the resulting ID will change;  
  this is equivalent to creating a new slug with a new identity.
- State fields (`result`, `status`, `comment`, `timestamp`, `entity_id`) do **not** affect the ID.

Address IDs therefore provide a stable linking mechanism across Markdown, API payloads, exports, and the SQLite runtime store.

## 5. Markdown Authoring Specification (Canonical at Authoring Time)

Markdown is the canonical authoring format for APIM checklists.  
It serves as the human-editable source from which the canonical data model is parsed and persisted into the SQLite runtime store.

A checklist is represented as a Markdown file containing one or more procedures.  
Each procedure expands into a single checklist slug during parsing.

### 5.1 Structural Requirements

APIM Markdown checklists follow a consistent hierarchical pattern:

- **H1** — Section heading  
- **H2** — Procedure heading  
- Bullet fields for action, spec, result, status, comment  
- **H3** — Instructions section  
- Optional subheadings (H4–H6) for additional structure  
- **H3** — Relationships section (outgoing relationships)

Each **H2 procedure block** corresponds to exactly one slug.

Example Structure

```markdown
# <Section>

## <Procedure>

- **Action**: <text>
- **Spec**: <text>
- **Result**: <text>
- **Status**: <Pass | Fail | NA | Other>
- **Comment**: <text>

### Instructions
Freeform guidance (links permitted).

### Relationships
**Address ID:** <THIS_address_id>
- depends_on <OTHER_address_id>
- fulfills <OTHER_address_id>
```

The parser extracts the addressing fields, state fields, instructions, and outgoing relationships from this structure.

### 5.2 Section Headings (`section`)

Each H1 heading defines the `section` field for all procedures that follow until the next H1.  
Sections may contain an optional descriptive paragraph, which is ignored by the parser.

```markdown
# Vacuum System
(optional descriptive text)
```

### 5.3 Procedure Headings (`procedure`)

The H2 heading defines the `procedure` field.

```markdown
## Multi-grain bread
```

This is the noun form of the task and should be concise (~28 character soft limit).

### 5.4 Required Bullet Fields

Within each H2 block, the following bullets must appear exactly once:

- **Action**  
- **Spec**  
- **Result**  
- **Status**  
- **Comment**

Bullet formatting uses:

```markdown
- **FieldName**: value
```

Capitalization of field names is required as shown.

Runtime-managed metadata (`timestamp`, `entity_id`) are not part of the required bullets. Markdown tools may ignore or overwrite any manually authored values for these fields; the runtime store is canonical. Parsers may tolerate legacy `Timestamp` bullets for compatibility but must not treat them as authoritative.

### 5.5 Field Extraction Rules

- **Action** → `action` (verb phrase)  
- **Spec** → `spec` (expected target or standard)  
- **Result** → `result` (observed outcome, formatted for comparison with spec)  
- **Status** → `status` (must be Pass, Fail, NA, or Other)  
- **Comment** → `comment` (freeform clarifying text)

Whitespace after the colon is trimmed.  
Empty values are allowed.

### 5.6 Instructions Section

Each procedure must include one **Instructions** subsection:

```markdown
### Instructions
<freeform text>
```

Rules:

- Freeform text may include paragraphs, lists, links, or subheadings (H4–H6).  
- Recommended soft limit: ~500 characters.  
- External links are encouraged when tacit knowledge, videos, diagrams, or extended SOPs are required.  
- All text under this H3 until the next H3 or H2 belongs to `instructions`.

### 5.7 Relationships Section

Outgoing relationships are defined using a dedicated **Relationships** subsection:

```markdown
### Relationships
- depends_on <address_id>
- fulfills <address_id>
```

Rules:

- Each bullet defines a single outgoing relationship from this slug.  
- `<address_id>` must be a 16-character Base32 ID.  
- Invalid or missing IDs generate warnings but do not halt parsing.  
- Relationship subjects are always implicitly the current slug’s `address_id`.

The runtime store exposes both outgoing and incoming relationships; Markdown only describes outgoing edges.

### 5.8 Checklist Field (`checklist`)

The `checklist` field is derived from the filename of the Markdown file:

- `baked goods.md` → `baked goods`  

## 6. SQLite Storage Specification (Canonical at Runtime)

SQLite is the canonical runtime store for APIM checklists. All slug data, relationships, and state histories are persisted here. Markdown authoring is transformed into the canonical data model and stored in SQLite, and all API operations read/write exclusively from the runtime database.

The schema is optimized for:
- deterministic identity (`address_id`),
- compact storage,
- fast lookup by ID,
- efficient traversal of relationship graphs,
- and compatibility with future connectors and exports.

### 6.1 Overview of Tables

The runtime store contains three primary tables:

1. **slugs** — the current state of every checklist slug  
2. **relationships** — directed edges between slugs  
3. **history** — optional append-only log of state changes (informative)

Additional indexes support fast lookup and graph traversal.

### 6.2 Table: `slugs`

Stores the canonical representation of each checklist slug.

```sql
CREATE TABLE slugs (
    address_id    TEXT PRIMARY KEY,
    checklist       TEXT NOT NULL,
    section         TEXT NOT NULL,
    procedure       TEXT NOT NULL,
    action          TEXT NOT NULL,
    spec            TEXT NOT NULL,

    result          TEXT,
    status          TEXT CHECK (status IN ('Pass','Fail','NA','Other')),
    comment         TEXT,
    timestamp       TEXT,
    entity_id       TEXT,  -- 16-char Base32, FK to entities(entity_id) if catalog is enabled

    instructions    TEXT
);
```

#### Field rules enforced by the database

- `address_id` is unique and stable.  
- Addressing fields (`checklist`, `section`, `procedure`, `action`, `spec`) must match exactly what the parser extracted from Markdown.  
- Mutating state fields does **not** alter the addressing fields or the ID.  
- `timestamp` and `entity_id` are runtime-managed metadata regenerated on update; they do not affect addressing or Address ID.  
- `status` is constrained to the allowed tokens.

### 6.3 Table: `relationships`

Stores directed semantic relationships between slugs.

```sql
CREATE TABLE relationships (
    subject_id  TEXT NOT NULL,
    predicate   TEXT NOT NULL,
    target_id   TEXT NOT NULL,

    FOREIGN KEY(subject_id) REFERENCES slugs(address_id),
    FOREIGN KEY(target_id)  REFERENCES slugs(address_id)
);
```

#### Behavior

- Each row represents one directed triple: `(subject_id, predicate, target_id)`.  
- A relationship is stored **once**, but can be queried in either direction:
  - outgoing: `WHERE subject_id = ?`
  - incoming: `WHERE target_id = ?`
- The database does not enforce predicate semantics; rules are applied by the state machine (Section 9).

### 6.4 Table: `history` (Optional but Recommended)

The `history` table stores **snapshot records** of a slug’s mutable state at specific points in time.  
Each row represents the full recorded state for one `address_id` at one `timestamp`.

This table provides long-term auditability and supports analytics, reviews, compliance, and rollback inspection.  
It is **not** intended for high-frequency telemetry or continuous sensor logging; such timeseries storage is out of scope for APIM and should be handled by a separate subsystem.

```sql
CREATE TABLE history (
    address_id  TEXT NOT NULL,
    timestamp     TEXT NOT NULL,   -- ISO8601 UTC
    result        TEXT,
    status        TEXT,
    comment       TEXT,
    entity_id     TEXT,            -- 16-char Base32

    FOREIGN KEY(address_id) REFERENCES slugs(address_id),

    PRIMARY KEY (address_id, timestamp)
);
```

#### Notes

- The primary key is the natural composite key `(address_id, timestamp)`, since each timestamped snapshot must be unique.
- There is no artificial `id` column.
- Each snapshot records the regenerated `timestamp` and `entity_id`, preserving who or what applied the change.
- Storing the full mutable state at each timestamp avoids diff reconstruction and simplifies analytics.
- To view the “previous value,” clients simply query the most recent earlier timestamp.
- This table is typically updated on meaningful state changes (daily, monthly, or procedural events), not high-frequency intervals.

#### 6.4.1 Optional: Entities Catalog

A normalized catalog may be used to track entity metadata without changing runtime semantics:

```sql
CREATE TABLE entities (
    entity_id    TEXT PRIMARY KEY,  -- 16-char Base32, matches slugs/history
    kind         TEXT NOT NULL,     -- e.g., 'human' | 'system' | 'agent'
    display_name TEXT NOT NULL,     -- e.g., "JDS", "pm-worker-01"
    meta         TEXT               -- optional JSON blob for auth IDs, email, etc.
);
```

The `entities` catalog decouples opaque `entity_id` tokens from human identifiers.

- Core tables (`slugs`, `history`) store only `entity_id`.  
- UIs and reporting join on `entities.entity_id` to render `display_name` and `kind`.  
- Access to the `entities` table may be restricted independently from operational tables, allowing logs and exports to retain `entity_id` while redacting or omitting PII.

The `history` table may be omitted in minimal deployments but is strongly recommended for environments requiring audit trails and longitudinal analysis.

### 6.5 Indexes

Recommended indexes for performance:

```sql
CREATE INDEX idx_slugs_checklist          ON slugs(checklist);
CREATE INDEX idx_relationships_subject    ON relationships(subject_id);
CREATE INDEX idx_relationships_target     ON relationships(target_id);
CREATE INDEX idx_history_checklist        ON history(address_id);
```

### 6.6 Mapping Markdown → SQLite

During ingest:

1. Parse each Markdown procedure.  
2. Extract addressing fields exactly as written.  
3. Construct the canonical addressing string.  
4. Regenerate `address_id`.  
5. Populate runtime metadata (`timestamp`, `entity_id`) from the ingestion context (e.g., the ingest service's assigned 16-character `entity_id` plus current UTC).  
6. Insert or update the corresponding row in `slugs`.  
7. Insert all outgoing relationships into `relationships`.  
8. Preserve `instructions` as raw text.

### 6.7 Treatment of Updates

State updates via the API:

- modify `result`, `status`, `comment` from the client payload  
- regenerate `timestamp` and `entity_id` from the current runtime/auth context  
- append to `history`  
- **must not** modify the addressing fields  
- **must not** modify instructions  
- do not touch Markdown

Addressing-field changes require revising the Markdown source and re-ingesting, which produces a new slug with a new `address_id`.

### 6.8 Deletions and Deactivation

Slugs are rarely deleted. Recommended workflow:

- Soft-delete via a dedicated status or marker in the checklist source, or  
- Hard-delete rows only during administrative cleanup.

Relationships referencing a deleted slug should be removed or revalidated during ingestion.

### 6.9 Regeneration Properties

The runtime store is fully regenerable from Markdown:

- If all Markdown source files are available, the entire SQLite state (slugs + relationships) can be rebuilt deterministically.
- State fields (`result`, `status`, `comment`, `timestamp`, `entity_id`) and `history` are **not** regenerated from Markdown and must be preserved separately.

### 6.10 SQLite Configuration Notes

- UTF-8 text storage is recommended.  
- WAL mode (`PRAGMA journal_mode=WAL`) is recommended for concurrent read-heavy operation.  
- Foreign keys should be enabled (`PRAGMA foreign_keys=ON`).  
- Size scales efficiently; 3–10 million slugs is feasible within typical SQLite performance envelopes with proper indexing.

SQLite provides reliability and atomicity guarantees appropriate for APIM runtimes without requiring external infrastructure.

## 7. API and MCP Encodings (Reference Transport Layer)

The APIM transport layer enables interaction between clients and the runtime store (SQLite).  
Two complementary interfaces are provided:

1. **HTTP+JSON API** — a classical REST-style interface for broad compatibility.  
2. **MCP (Model Context Protocol)** — a structured, self-describing interface designed for LLMs, agents, and automated systems.

Both interfaces operate on the same canonical runtime store and use the same data model defined in Section 3.

Markdown remains the canonical authoring format for humans.  
SQLite remains the canonical runtime state store.  
API and MCP serve as transport and automation layers.

---

### 7.1 Design Principles for Transport Interfaces

#### Minimal update contract
State updates require only:
- `address_id`
- one or more mutable fields (`result`, `status`, `comment`)
- the server regenerates `timestamp` and `entity_id` for each successful update

Addressing fields are immutable in place.

#### Addressing changes create new slugs
If a client (human or agent) submits a complete slug with **changed addressing fields**, a **new slug** is created with a newly generated `address_id`.  
This preserves the immutability of identity while allowing programmatic creation of new procedures.

#### Markdown defines authored structure
Instructions, addressing fields, and authored relationships originate in Markdown.  
When the runtime store diverges from underlying Markdown sources, new Markdown files may be regenerated to realign the human authoring layer with the canonical runtime state.

#### Agents and LLMs are first-class clients
APIM is designed so that:
- UIs may use the API,
- humans may edit Markdown,
- automated agents (LLMs, voice interfaces, etc.) may issue API or MCP calls directly.

Agents may:
- create new procedures,
- draft Markdown files,
- update state fields,
- validate slugs,
- traverse relationships.

No restriction forces agents to manipulate Markdown directly; the runtime store and transport interfaces treat agents as equal participants in the workflow.

#### Runtime authority
All operational state is canonical in SQLite at runtime.  
Transport interfaces simply expose or update this canonical store.

#### Deterministic behavior
Transport operations:
- are side-effect free beyond state machine evaluation,
- return consistent results for identical input,
- never mutate addressing fields of existing slugs.

---

### 7.2 Slug Representation (Common to API and MCP)

Transport interfaces exchange slugs using a unified JSON object shape:

```json
{
  "address_id": "<BASE32_ID>",
  "checklist": "<string>",
  "section": "<string>",
  "procedure": "<string>",
  "action": "<string>",
  "spec": "<string>",

  "result": "<string>",
  "status": "Pass | Fail | NA | Other",
  "comment": "<string>",
  "timestamp": "<ISO8601>",
  "entity_id": "<ENTITY_ID>",   // 16-char Base32, see Section 11.4.1

  "instructions": "<string>",

  "relationships": [
    { "predicate": "<string>", "target": "<BASE32_ID>" }
  ]
}
```

This representation is identical across HTTP+JSON and MCP.

---

### 7.3 JSONL Encoding (Reference Only)

For bulk operations or developer workflows, slugs may be exported or imported as JSONL:

- one complete slug per line  
- stable field ordering is not required  
- useful for backups, comparison, migrations, analytics

JSONL is a transport format, not a canonical storage mechanism.

---

## 7.4 HTTP+JSON API

A classical REST-style transport interface.

### 7.4.1 GET /api/slug/<address_id>

Returns the full slug.

```json
{ ... slug object ... }
```

### 7.4.2 GET /api/checklist/<checklist>

Returns all slugs for a given checklist.

### 7.4.3 GET /api/relationships/<address_id>

Returns incoming and outgoing relationships:

```json
{
  "address_id": "<BASE32_ID>",
  "outgoing": [
    { "predicate": "<p>", "target": "<BASE32_ID>" }
  ],
  "incoming": [
    { "predicate": "<p>", "source": "<BASE32_ID>" }
  ]
}
```

### 7.4.4 PATCH /api/update

Minimal update contract:

```json
{
  "address_id": "<BASE32_ID>",
  "status": "Pass",
  "comment": "Adjusted",
  "result": "20 C"
}
```

### 7.4.5 PATCH /api/update_bulk

Accepts a JSON array of minimal updates.

### 7.4.6 GET /api/export/jsonl or /api/export/json

Reference-only bulk exports.

---

## 7.5 MCP Interface (Model Context Protocol)

MCP is a structured, schema-driven protocol designed for LLMs and agents.  
It allows APIM to expose its capabilities, schema, and available operations in a machine-discoverable form.

### 7.5.1 Rationale for MCP Integration

MCP allows:

- LLMs to **discover** available API methods without prior knowledge  
- agents to **query schema** and understand slug fields programmatically  
- safe execution via a **capability declaration model**  
- contextual reasoning (LLMs pull only the slugs needed in their working window)  
- onboarding of new agents without writing custom SDK code  

MCP effectively turns APIM into a self-describing, AI-friendly service.

### 7.5.2 Core MCP Tools

APIM exposes the following MCP “tools”:

#### `apim.get_slug`
Inputs:
```json
{ "address_id": "<BASE32_ID>" }
```
Output: slug object.

#### `apim.list_checklist`
Inputs:
```json
{ "checklist": "<string>" }
```
Output: list of slug objects.

#### `apim.update_slug`
Inputs:
```json
{
  "address_id": "<BASE32_ID>",
  "result": "<optional>",
  "status": "<optional>",
  "comment": "<optional>"
}
```
Output:
```json
{
  "ok": true,
  "updated_fields": ["status", "comment"],
  "timestamp": "<ISO8601>",
  "entity_id": "<ENTITY_ID>"   // 16-char Base32, see Section 11.4.1
}
```

#### `apim.create_slug`
Inputs:
```json
{
  "checklist": "...",
  "section": "...",
  "procedure": "...",
  "action": "...",
  "spec": "...",
  "instructions": "...",
  "relationships": [ ... ]
}
```
Output:
```
{ "address_id": "<NEW_BASE32_ID>" }
```

(This mirrors the addressing-field rules: new addressing → new ID.)

#### `apim.relationships`
Returns incoming + outgoing edges.

### 7.5.3 MCP Schema Exposure

APIM exposes schema definitions for:

- slug fields  
- allowed status values  
- relationship predicates  
- canonical ID format  
- update contract rules  

This allows LLMs to craft well-formed calls without guesswork.

---

### 7.6 Internal Consistency Rules

Transport interfaces must:

- reflect the canonical data stored in SQLite  
- never mutate addressing fields of existing slugs  
- evaluate state machine logic deterministically  
- preserve consistent slug shapes across API and MCP  

---

### 7.7 Error Handling (Common to API and MCP)

Errors use a structured JSON object:

```json
{
  "ok": false,
  "error": "invalid_field",
  "detail": "status must be Pass, Fail, NA, or Other"
}
```

---

### 7.8 Out-of-Scope Behaviors

- Timeseries data ingestion (sensor/telemetry)  
- Bulk structural modification of Markdown  
- Automated inference of addressing fields  
- Relationship creation via minimal update (requires full slug creation or Markdown authoring)  
- High-frequency logging  

These may be addressed in future modules but are explicitly outside the transport specification.

## 8. Relationship Model

The APIM relationship model defines logical connections between checklist slugs.  
Relationships enable dependency management, procedural roll-up, hierarchical reasoning, and cross-checklist referencing.  
Each relationship is a directed triple stored in the runtime database and used by the state machine (Section 9).

Relationships are authored in Markdown (outgoing edges only) and expanded at runtime into a bidirectional graph for querying, evaluation, and visualization.

### 8.1 Conceptual Structure

A relationship is defined as:

```
(subject_address_id, predicate, target_address_id)
```

- **subject** — the slug declaring the relationship  
- **predicate** — the type of relationship  
- **target** — the referenced slug

In Markdown, only outgoing edges are authored.  
In SQLite and transport interfaces, **both outgoing and incoming edges** are visible for clarity and to prevent duplicated relationships.

### 8.2 Predicates

APIM defines a small, controlled vocabulary of relationship predicates.  
All predicates are lowercase ASCII identifiers to ensure stability across formats.

#### 8.2.1 `depends_on`

Indicates that the subject slug cannot be considered fully “Pass” until the target slug is also Pass.

Semantics:

- If `subject` has status Pass and any `target` has Fail or Other → subject is considered constrained.  
- If target is NA → subject may still pass.  
- If target is missing or unresolved → subject evaluation is indeterminate (left to the state machine).

This expresses procedural dependencies, prerequisite tasks, or logical ordering.

#### 8.2.2 `fulfills`

Indicates that the subject contributes to satisfying a higher-level requirement represented by the target.

Typical use:

- subject = low-level check  
- target = high-level “roll-up” item

Evaluation is generally one-way: target may aggregate status from multiple fulfilling subjects.

#### 8.2.3 `satisfied_by`

Inverse of `fulfills`.

Equivalent to:

```
(A, satisfied_by, B)  ≡  (B, fulfills, A)
```

This predicate exists for author clarity; the runtime system normalizes both directions into the same graph semantics.

#### 8.2.4 Additional predicates

New predicates may be added in future revisions of the spec.  
They must:

- be lowercase ASCII  
- have clearly defined semantics  
- be deterministic  
- not imply hidden side effects  
- remain compatible with the state machine rules

### 8.3 Relationship Directionality

Markdown only stores outgoing relationships:

```
### Relationships
**Address ID:** <THIS_ID>
- depends_on <OTHER_ID>
- fulfills <OTHER_ID>
```

Runtime systems must expose relationships in both directions:

- **Outgoing:** `subject → target`
- **Incoming:** `other slugs → subject`

Incoming visibility prevents stakeholders from unknowingly duplicating or contradicting relationships authored elsewhere.

### 8.4 Graph Semantics

The complete set of slugs and relationships forms a directed multigraph.

Key properties:

- Multiple predicates may exist between the same two nodes.  
- A slug may have zero or many incoming or outgoing edges.  
- Slugs referencing non-existent targets should be treated as warnings, not hard errors.

### 8.5 Cycles

Cycles are permitted at the data level, but the **state machine must handle them explicitly**.

Rules:

- Cycles in `depends_on` produce an indeterminate evaluation; neither side can resolve.  
- The state machine may detect and warn on circular dependency chains.  
- Cycles involving `fulfills` / `satisfied_by` do not semantically break the graph but indicate contradictory modeling and should produce warnings.

The specification does not forbid cycles but encourages tools to highlight them.

### 8.6 Evaluation Rules

The state machine (Section 9) evaluates relationships as follows:

- For `depends_on`:
  - subject.Status = Pass only if all targets are Pass or NA
  - conflicts produce warnings or constraints

- For `fulfills`:
  - subject contributes to the target’s aggregated status  
  - aggregation strategy is defined in Section 9

- For `satisfied_by`:
  - treated identically to the inverse of `fulfills`

### 8.7 Authoring vs Runtime Responsibilities

**Markdown authoring responsibilities:**

- Declare outgoing edges intentionally  
- Avoid duplicating relationships that already exist inbound from other checklists  
- Keep predicates concise and meaningful  
- Provide consistent formatting for parsing

**Runtime responsibilities:**

- Normalize relationships into structured triples  
- Expose incoming + outgoing edges via API and MCP  
- Detect missing target IDs  
- Warn about cycles or unresolved chains  
- Supply stable graph traversal for agents and tools

### 8.8 MCP Integration

The MCP interface exposes relationships through:

- `apim.relationships` (incoming + outgoing)  
- schema declarations of allowed predicates  
- safety validation for malformed relationships  
- auto-completion assistance for agents generating new relationships  

Agents may:

- inspect dependencies before executing tasks  
- visualize procedural roll-up  
- adjust workflows dynamically  
- suggest new relationships (requiring human approval or Markdown updates)

### 8.9 Extensibility

To maintain forward compatibility:

- New predicates may be introduced in future versions of this specification.  
- Predicate semantics must be defined in a normative appendix.  
- Runtime systems must not infer behavior for unknown predicates; they are treated as opaque metadata unless defined in the specification.

## 9. State Machine and Automation

The APIM state machine defines how slug states are evaluated, validated, and propagated.  
It provides deterministic behavior for interpreting relationships, enforcing constraints, and generating consistent results across clients, agents, and automated tools.

The state machine operates entirely on the canonical runtime store (SQLite).  
API and MCP calls interact with the state machine implicitly.

### 9.1 Goals

- Normalize handling of `depends_on`, `fulfills`, and `satisfied_by`  
- Ensure deterministic evaluation of `status`  
- Provide predictable propagation rules without hidden side effects  
- Allow time-based logic where appropriate  
- Surface errors and conflicts without silently modifying data  
- Support agent-driven or automated execution workflows  
- Preserve immutability of addressing fields  
- Avoid implicit inference of new relationships or hidden rule creation

### 9.2 Inputs and Outputs

#### Inputs
- A complete graph of slugs and relationships from the runtime store  
- The current values of state fields (`result`, `status`, `comment`, `timestamp`, `entity_id`)  
- Any updates received via API or MCP  
- Time-based rules configured by the deployment (optional)

#### Outputs
- Validated `status` for each slug  
- Warnings or constraint flags (not written to the database unless configured)  
- Aggregated roll-up values for slugs participating in `fulfills` chains  
- Evaluation traces or logs (implementation-defined)

The state machine **does not** inject new relationships or modify addressing fields.

### 9.3 Evaluation Order

Evaluation proceeds in the following order:

1. **Load all slugs and relationships**  
2. **Evaluate dependency constraints (`depends_on`)**  
3. **Evaluate roll-up chains (`fulfills` / `satisfied_by`)**  
4. **Apply time-based rules (optional)**  
5. **Resolve conflicts or cycles**  
6. **Return evaluated result to API/MCP or automation caller**

The runtime store is updated **only when an API/MCP update explicitly stores data**.

### 9.4 Dependency Evaluation (`depends_on`)

Rule:

```
A depends_on B  →  A cannot be considered Pass unless B is Pass or NA
```

Details:

- If B = Fail → A is considered constrained  
- If B = Other → A is constrained  
- If B = NA → A may pass  
- If B has no result/status → A is unresolved  
- If multiple dependencies exist, all must be satisfied

The state machine may return:

- `ok`: dependency chain satisfied  
- `blocked`: a dependent slug is not passable  
- `cycle_detected`: circular dependency chain  
- `missing_target`: dependency refers to unknown `address_id`

These flags do **not** automatically update the slug status; they inform client logic or automations.

### 9.5 Roll-Up Evaluation (`fulfills` / `satisfied_by`)

A slug may contribute its status to one or more higher-level slugs.

```
B fulfills A  →  A aggregates the status of B
A satisfied_by B  →  equivalent to B fulfills A
```

Aggregation rules (default):

- If all fulfilling slugs are Pass or NA: roll-up = Pass  
- If any fulfilling slug is Fail: roll-up = Fail  
- If fulfilling slugs are mixed (e.g., Pass + NA + Other): roll-up = Other  
- If fulfilling slugs have no status: roll-up = NA (indeterminate)

The state machine must avoid infinite propagation in the presence of cycles.  
See Section 9.7.

### 9.6 Time-Based Rules (Optional)

Some procedures represent cyclical or scheduled checks (e.g., daily, monthly).  
Deployments may register time-based rules such as:

- auto-marking a slug as Fail if its timestamp exceeds a threshold  
- inserting a reminder event rather than modifying the slug  
- marking status as NA after a certain date  
- rotating status back to “unverified”

The specification requires:

- all time-based rules must be explicit  
- no hidden updates to the database  
- updates must occur only via explicit API/MCP calls  
- evaluation may return “expired”, “stale”, or “due” flags instead of modifying status

### 9.7 Handling Cycles

Cycles are permitted in the relationship graph but handled explicitly:

#### depends_on cycles
A cycle creates a chain that cannot resolve:

```
A depends_on B
B depends_on A
```

Result:

- Both slugs are marked as `cycle_detected` in evaluation traces  
- Neither can be considered Pass  
- Client tools may choose to:
  - flag the issue,
  - warn the user,
  - highlight the cycle visually

No automatic changes are made to database data.

#### fulfills cycles
These indicate contradictory hierarchy definitions:

```
A fulfills B
B fulfills A
```

Semantics:

- Aggregation is invalid  
- Roll-up result becomes `Other`  
- Evaluation traces mark the cycle

The state machine must not loop indefinitely.

### 9.8 Conflict Handling

When evaluating a slug, the state machine may emit:

- `inconsistent_dependency`  
- `missing_target`  
- `cycle_detected`  
- `contradictory_rollup`  
- `indeterminate`  

These flags:

- do not modify slug fields  
- do not automatically turn status to Fail or Other  
- are returned to the caller via API/MCP or logs  
- guide authors or agents in revising Markdown or workflows

### 9.9 Integration With Agents and MCP

Agents invoking state evaluation through MCP:

- may retrieve dependency graphs  
- may ask for evaluation traces to explain constraints  
- may automatically draft Markdown modifications  
- may propose new relationships, subject to human approval  

MCP tools must provide:

- clear schema for returned flags  
- stable formats for incoming/outgoing edges  
- predictable evaluation outcomes

### 9.10 State Change and Persistence

The state machine:

- **never writes mutable fields** unless invoked via API/MCP  
- **never modifies addressing fields**  
- **never adds or removes relationships**

Persistence rules:

- A change to `result`, `status`, or `comment` must come from an explicit API/MCP update; `timestamp` and `entity_id` are regenerated by the runtime on each successful write.  
- A new snapshot is stored in `history` if the relevant subsystem is enabled (including regenerated `timestamp` and `entity_id`).  
- Evaluation flags are ephemeral; they do not persist unless a tool writes them out-of-band

### 9.11 Out-of-Scope Logic

The state machine does not:

- infer new relationships  
- automatically reorder procedures  
- modify Markdown  
- perform natural language classification  
- assign addressing fields  
- infer missing statuses  
- act as a workflow engine  
- perform true timeseries analysis  
- ingest sensor data

These behaviors may be implemented by extensions but are outside this core specification.

## 10. Minimal API Update Contract

The minimal API update contract defines the smallest valid operation that can modify the runtime state of a checklist slug.  
All updates occur through explicit API or MCP calls and never through implicit state machine behavior.  
Only mutable fields may be modified.

### 10.1 Update Requirements

A valid update must contain:

- a `address_id` (16-character Base32), and  
- one or more mutable fields:

```
result
status
comment
```

No other fields may be modified using the minimal update contract, and clients must not attempt to set `timestamp` or `entity_id`; the server regenerates them on each successful update.  
Addressing fields (`checklist`, `section`, `procedure`, `action`, `spec`), instructions, and authored relationships are immutable via this mechanism.

### 10.2 Example Update Payload

```json
{
  "address_id": "F7W4X2J9T3B6P8KD",
  "status": "Pass",
  "comment": "Measured at 20 °C",
  "result": "20"
}
```

### 10.3 Server Processing

When a minimal update is received:

1. The runtime store retrieves the slug by `address_id`.  
2. Only provided mutable fields are updated.  
3. `timestamp` is regenerated in UTC (ISO8601).  
4. `entity_id` is resolved from the current execution context and mapped to a 16-character Base32 ID:  
   - for human users, from the authenticated principal;  
   - for systems/agents, from a configured system identity.  
   The mapping from principal to `entity_id` is maintained in the `entities` catalog or an equivalent identity service.  
5. The updated slug is persisted to SQLite.  
6. If the history subsystem is enabled, a full snapshot of the mutable fields is written to `history` with the new timestamp and `entity_id`.  
7. The state machine evaluates dependency and roll-up context, returning any warnings or flags (Section 9).  
8. The server returns a structured response.

### 10.4 Example Response

```json
{
  "ok": true,
  "address_id": "F7W4X2J9T3B6P8KD",
  "updated_fields": ["status", "comment", "result"],
  "timestamp": "2024-12-18T15:23:11Z",
  "entity_id": "ABCDEFGHJKMNPQRS",
  "warnings": []
}
```

If evaluation generates warnings (e.g., unresolved dependencies), the `warnings` field is populated but no automatic changes are made to the slug.

### 10.5 Invalid Updates

Invalid update attempts result in structured error responses.  
Examples of invalid updates:

- missing `address_id`  
- updating addressing fields  
- malformed `status` value  
- attempts to set `timestamp` or `entity_id` directly  
- attempts to modify instructions or relationships  
- missing all mutable fields in the payload

Example error:

```json
{
  "ok": false,
  "error": "invalid_update",
  "detail": "Status must be Pass, Fail, NA, or Other"
}
```

### 10.6 Behavior With Agents and MCP

Agents using MCP rely on the same minimal contract.  
MCP tools expose this update interface with schema-based validation to ensure that:

- agents cannot modify addressing fields,  
- updates are always explicit,  
- generated updates are well-formed,  
- the state machine remains deterministic across human and automated workflows.

### 10.7 Invariants

The minimal update contract guarantees:

- addressing-field immutability  
- deterministic slug identity  
- predictable slug state transitions  
- compatibility with Markdown as authoring format  
- clear separation between structural changes (new slugs) and operational updates  
- a consistent interface for human tools and automated agents  

This contract is the foundation for all runtime modifications in APIM.

### 10.8 Full Slug Creation Contract

The full slug creation contract defines how a new checklist slug is introduced into the runtime store.  
Creation may occur through Markdown ingestion, API submission, MCP tools, or automated agent workflows.  
A new slug always results in a newly computed `address_id`.

#### 10.8.1 Required Addressing Fields

A valid creation request must include all **addressing fields**:

```
checklist
section
procedure
action
spec
```

Addressing fields define slug identity.  
Any modification to these fields produces a new `address_id` (Section 3).

#### 10.8.2 Optional Mutable Fields

The following mutable fields may be included but are not required:

```
result
status
comment
```

If omitted, they default to:

- `result = ""`
- `status = "NA"`
- `comment = ""`

#### 10.8.3 Instructions and Relationships

Two additional structural fields may be included:

- `instructions` — freeform Markdown (≤ 500 characters recommended)
- `relationships` — outgoing relationships in normalized JSON form

Relationships follow the structure:

```json
{
  "predicate": "<string>",
  "target": "<BASE32_ID>"
}
```

If omitted, the slug begins with no outgoing edges.

#### 10.8.4 Example Creation Payload

```json
{
  "checklist": "oven_check",
  "section": "Temperature Control",
  "procedure": "Oven stability",
  "action": "Verify stability",
  "spec": "180–190 C",

  "result": "",
  "status": "NA",
  "comment": "",

  "instructions": "Verify oven stability over a 10-minute interval.",
  "relationships": [
    { "predicate": "depends_on", "target": "12FF9A91C10DBE77" }
  ]
}
```

#### 10.8.5 Server Processing

Upon creation:

1. Validate addressing fields.  
2. Normalize instructions and relationship arrays.  
3. Compute the canonical `address_id` from addressing fields.  
4. Insert the slug into `slugs`.  
5. Insert any outgoing relationships into `relationships`.  
6. Generate a new `timestamp` (UTC, ISO8601).  
7. Set `entity_id` from the creation context (auth user, system import, or agent).  
8. Optionally write a snapshot to `history`.  
9. Return the newly created `address_id`.

No attempt is made to deduplicate addressing fields; new slugs are always distinct.

#### 10.8.6 Example Creation Response

```json
{
  "ok": true,
  "address_id": "F7W4X2J9T3B6P8KD",
  "created": true,
  "timestamp": "2024-12-18T15:31:22Z",
  "entity_id": "Z234567Z234567Z2"
}
```

#### 10.8.7 Creation vs. Mutation

The full creation contract is distinct from the minimal update contract:

- **Creation:** defines a brand new slug, addressing fields required.  
- **Minimal Update:** modifies mutable fields of an existing slug, addressing fields forbidden.  

Agents and automation tools must choose the correct pathway based on whether the addressing fields are being introduced or preserved.

#### 10.8.8 Authoring Workflow Integration

When a new slug is created via API/MCP rather than Markdown:

- the runtime store becomes canonical immediately, and  
- Markdown regeneration tools may produce updated authoring files to maintain human-readable artifacts.

This preserves Markdown as the authoring format without constraining automated workflows.

## 11. Reserved Characters and Encoding Rules

This section defines the character-level constraints used throughout APIM to ensure reliable parsing across Markdown, SQLite, JSON/JSONL, API, and MCP interfaces.  
These rules prevent accidental Markdown interference, shell escapes, ambiguous identifiers, or incompatible encodings.

### 11.1 Address ID Encoding

Address IDs use **Crockford Base32**, uppercase, using only the unambiguous subset:

```
ABCDEFGHJKMNPQRSTVWXYZ234567
```
#### Rules:
- 16 characters exactly  
- no padding (`=`)  
- no lowercase  
- no hyphens or spaces  
- safe for filenames, URLs, database keys, JSON keys, and shell usage  
- double-click selects entire ID in most UIs  

Address IDs MUST NOT include:
- I, L, O, U (removed in Crockford Base32 to prevent visual confusion)

### 11.2 Canonical Addressing String Delimiter

When constructing the canonical addressing string for hashing (Section 3):

```
<checklist> || <section> || <procedure> || <action> || <spec>
```

Rules:
- delimiter literal is `||` (two ASCII vertical bars, no spaces)  
- fields are used verbatim (whitespace preserved)  
- must not collapse or normalize author whitespace  
- no escaping performed before hashing  

This ensures stable hashing even if authors include punctuation or spacing.

### 11.3 Allowed Characters in Addressing Fields

Addressing fields (`checklist`, `section`, `procedure`, `action`, `spec`) may contain:

- letters (A–Z, a–z)  
- digits (0–9)  
- spaces  
- ASCII punctuation, except where noted  
- Unicode characters (allowed, but discouraged for portability)  

Recommended (not required):
- normal ASCII punctuation  
- minimal symbolic characters  
- avoidance of control characters or mixed directionality text

Addressing fields MUST NOT include:
- the delimiter sequence `||` (would corrupt canonical string)  
- NULL (`\0`)  
- isolated surrogate UTF-16 code units  
- line breaks in `checklist`, `section`, `procedure`, `action`, `spec`

Line breaks are allowed only in `instructions` and `comment`.

### 11.4 Allowed Characters in State Fields

`result`, `status`, and `comment` may contain:

- ASCII  
- Unicode text  
- spaces and line breaks (multi-line comments allowed, though short comments are recommended)

Restrictions:
- `status` is restricted to: `Pass`, `Fail`, `NA`, `Other`
- comments must avoid the literal sequence `` ``` `` inside Markdown unless escaped, to prevent terminating fenced code blocks
- NULL (`\0`) is forbidden  
- internal newlines are allowed in `comment` but discouraged in `result`

#### 11.4.1 Entity ID Encoding

`entity_id` values are 16-character Crockford Base32 identifiers using the same unambiguous alphabet as `address_id` (Section 11.1):

```
ABCDEFGHJKMNPQRSTVWXYZ234567
```

Rules:

- 16 characters exactly  
- uppercase only  
- no padding (`=`)  
- no hyphens or spaces  
- safe for filenames, URLs, database keys, JSON keys, and shell usage  

Entity IDs MUST NOT include:

- I, L, O, U (removed in Crockford Base32 to prevent visual confusion)

##### 11.4.1.1 Canonical Entity String

Each deployment defines a canonical “entity principal string” as input to hashing, for example:

```
<namespace> || <principal>
```

Examples:

- `idp:azuread || 00000000-0000-0000-0000-000000000000`
- `system || apim-ingest-worker-01`
- `agent || apim-mcp-llm-v1`

Rules:

- `||` is reserved as a delimiter in the principal string.  
- Principal strings are normalized and stable per entity.  
- The principal string itself is not stored in `slugs` or `history`; only the derived `entity_id` is.

##### 11.4.1.2 Hashing and Encoding

Entity IDs are derived as follows:

1. Compute the xxHash128 of the canonical entity principal string.  
2. Extract the lowest 80 bits (10 bytes) of the hash output.  
3. Encode the 10-byte value in Crockford Base32, uppercase.  

This yields a 16-character identifier with:

- deterministic mapping (same principal → same `entity_id`)  
- extremely low collision probability at typical entity counts  
- no embedded semantics (kind, name, or auth IDs are stored only in the `entities` catalog or IAM)

Runtime systems MAY choose alternative hashing algorithms, but MUST preserve the 16-character Crockford Base32 format and determinism for a given principal string.

### 11.5 Instructions Field Encoding

`instructions` is freeform Markdown text.  
Allowed:
- all valid UTF-8  
- headings, links, lists, inline code, fenced code blocks  
- URLs, filenames, external document references  
- multi-line content  

Restrictions:
- avoid embedding extremely large content (> 500 chars recommended)  
- avoid storing images or binary data; link instead  
- avoid misuse of triple-backtick fences inside relationships sections  
- avoid the delimiter `||` (harmless but may confuse authors reading the canonical construction rules)

Markdown parsers must treat `instructions` as opaque text; SQLite stores it as UTF-8 without transformation.

### 11.6 Relationship Predicate Encoding

Predicates must:

- be lowercase ASCII  
- contain only `[a-z0-9_]+`  
- begin with a letter  
- not contain spaces  
- not contain punctuation  
- not exceed 32 characters  

Examples:
```
depends_on
fulfills
satisfied_by
```

Predicates must be treated as literal tokens.

### 11.7 Relationship Target Encoding

Targets reference other procedures using Address IDs:

```
<predicate> <TARGET_ID>
```

Rules:
- one space between predicate and ID  
- no trailing punctuation  
- no inline commentary in the same line  
- target IDs must be valid Base32 identifiers

### 11.8 JSON/JSONL Encoding Rules

All JSON must be UTF-8 encoded.  
Required:

- double-quoted strings  
- ISO8601 UTC timestamps  
- arrays for relationships  
- explicit nulls permitted but discouraged  
- stable field names as defined in Section 3  

JSONL:
- one JSON object per line  
- no trailing commas  
- no multi-line objects  

### 11.9 SQLite Storage Encoding

APIM requires SQLite in **UTF-8 mode**.

Rules:
- all text fields stored in UTF-8  
- no binary blobs in canonical APIM tables  
- checklists, sections, procedures, actions, specs stored exactly as authored (no trimming)  
- relationship predicates stored as lowercase ASCII  
- relationship targets stored as uppercase Base32  

The database MUST NOT normalize whitespace in addressing fields.

### 11.10 Safety and Shell Compatibility

Address IDs, predicates, and JSON keys are chosen to be safe in:

- shell scripts  
- filenames  
- URLs  
- HTTP parameters  
- logging systems  

Authors should avoid:
- semicolons in addressing fields  
- unescaped `$` in shell environments  
- quoting characters that confuse Markdown authors (e.g., backticks inside `action`)

These are recommendations, not syntactic restrictions.

### 11.11 Summary of Reserved Characters

The following characters are reserved or restricted in specific contexts:

| Context | Restricted Characters |
|--------|------------------------|
| Address ID | all chars except Crockford Base32 uppercase |
| Canonical delimiter | `||` sequence reserved for hashing; forbidden inside addressing fields |
| Predicate | anything outside `[a-z0-9_]` |
| JSON keys | must follow canonical field names (Section 3) |
| Markdown relationships | target ID must be pure Base32, no punctuation |
| Addressing fields | must not include NULL or line breaks; must not include `||` |

## 12. Canonical Workflow Summary

This section summarizes how APIM checklist data flows through its lifecycle—from authoring to runtime evaluation to automation—using the canonical elements defined in Sections 1–11.

The workflow unifies four perspectives:

1. **Human authors (Markdown)**
2. **Runtime system (SQLite)**
3. **Transport interfaces (API and MCP)**
4. **Automation and evaluation (State Machine)**

Together, these components allow APIM to function as a stable, deterministic system supporting both human-readable SOPs and machine-executable procedure logic.

---

### 12.1 Authoring Workflow (Human-Centric)

1. **Authors write checklists in Markdown**, following the structure defined in Section 5.  
2. Each Markdown procedure row contains:
   - addressing fields (immutable identity)
   - instructions
   - outgoing relationships
   - initial state fields (optional)
3. Markdown ingestion tools parse the file, compute `address_id` values, and generate slugs.
4. Slugs are inserted into SQLite, becoming the canonical runtime representation.
5. Markdown and SQLite remain continuously synchronizable; updates to either layer can regenerate the other.

Markdown = **authoring truth**  
SQLite = **runtime truth**

---

### 12.2 Runtime Workflow (System-Centric)

After ingestion, all operational state is stored in SQLite:

- Each row = one slug  
- Relationships are normalized into triples  
- Mutable fields (`result`, `status`, `comment`) plus runtime metadata (`timestamp`, `entity_id`) track execution  
- History snapshots (if enabled) record state transitions over time  

SQLite holds the **canonical active state** for:

- execution
- reporting
- automation
- synchronization back to Markdown

---

### 12.3 Transport Workflow (API and MCP)

External tools, agents, and UIs interact with the runtime store via:

#### HTTP+JSON API  
- Minimal update contract (status, result, comment)  
- Full slug creation (addressing fields required)  
- Retrieval of slugs and relationship graphs  
- Exports for JSON/JSONL  

#### MCP (Model Context Protocol)  
- Self-describing interface  
- Structured schema declarations  
- Tools for get/update/create  
- Relationship inspection  
- Ideal for LLM agents and voice clients  

Both interfaces expose the **same data model**, ensuring consistency across tools.

---

### 12.4 Execution Workflow (State Machine)

When slugs are updated or retrieved, the state machine:

1. Evaluates dependency constraints (`depends_on`)
2. Aggregates roll-up logic (`fulfills`, `satisfied_by`)
3. Applies optional time-based rules
4. Detects cycles, missing IDs, or contradictions
5. Produces deterministic evaluation results and warnings

The state machine **never**:

- writes addressing fields  
- creates or deletes relationships  
- modifies instructions  
- performs hidden mutations  

It only evaluates and reports on current state.

---

### 12.5 Automation Workflow (Agent-Centric)

Agents using API/MCP can:

- query slugs and relationships  
- apply minimal-state updates  
- create new slugs  
- draft new Markdown (optional)  
- check dependency chains  
- reason over aggregated roll-ups  
- propose structural improvements  

Agents are considered **equal participants** alongside human authors.

Automation must follow:

- the minimal update contract for mutations  
- the full creation contract for new slugs  
- restrictions against modifying addressing fields in-place  
- established predicate semantics for relationships  

APIM is designed so that humans and machines can operate jointly without conflicts.

---

### 12.6 Synchronization Workflow

Because Markdown and SQLite are dual canonical layers (authoring vs runtime), synchronization may occur in either direction:

- **Markdown → SQLite**: ingestion of new/edited authoring files  
- **SQLite → Markdown**: regeneration of Markdown for review or version control  

This ensures:

- human readability remains intact  
- machine models remain authoritative  
- agents can operate without manipulating Markdown directly  
- authors see clean, updated files reflecting the current runtime state  

---

### 12.7 Reporting Workflow

Outputs such as:

- HTML (UIs and portals)  
- PDF (LaTeX)  
- JSON/JSONL exports  
- dependency graphs  
- audit reports  
- history timelines  

Are all derived from the canonical runtime store.

These are **views**, not sources of truth.

---

### 12.8 End-to-End Summary

The complete APIM lifecycle:

```
Markdown authoring (Section 5)
        ↓
Ingestion (compute address_id, normalize relationships)
        ↓
SQLite canonical runtime (Section 6)
        ↓
API + MCP transport (Section 7)
        ↓
State machine evaluation (Section 9)
        ↓
History tracking (Section 6.4)
        ↓
Automation (agents, tools, UI)
        ↓
Regeneration of Markdown (optional)
        ↓
Exports: JSONL, PDF, UI dashboards
```

This closed loop ensures:

- deterministic behavior  
- human + machine collaboration  
- separation of structure and operational state  
- strong identity semantics  
- predictable transformations at every step  

This workflow is the operational backbone of APIM.

