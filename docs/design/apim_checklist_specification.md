---
- Version: 0.0.1-0.5.5
- Date: 2025-11-28T20:30:00Z
- Organization: CVMewt Inc.
- Project: Checklist Assistant
- Repository: https://github.com/JDS-CT/APIM.git
- License: MIT (for reference implementation code)
- Git-Tracked: yes
---

# APIM Checklist Specification

This document defines the canonical specification for:

- Checklist slugs
- Slug IDs and Instance IDs
- Address tuples and Address IDs
- The canonical data model (addressing, state, instructions, relationships)
- The Markdown authoring format
- The SQLite runtime storage model
- Reference API and JSON/JSONL encodings

The specification is technology-agnostic at the conceptual level, but provides a concrete reference implementation using:

- Markdown for human authoring
- SQLite for runtime state storage
- HTTP+JSON/JSONL (and MCP) for transport

Other views (HTML forms, static PDFs, dashboards, etc.) are derived from the canonical model and the SQLite runtime store.

---

## 1. Scope and Purpose

APIM checklists are intended to be **active SOPs**: procedures that are authored, executed, updated, and reviewed continuously, rather than static documents that drift out of sync with real work. This work can manifest as actions performed by any actor, be it intangible or tangible work. This helps bridge the gap between physically perfromed tasks giving them a digital hook to feed electronic codified business processes thereby increasing the efficiency of workflow.

In typical project management practice (WBS, task trackers, etc.), work is described with **noun-like labels** (e.g., “Software installation,” “Pressure check”) optimized for planning, reporting, and coordination. Operator-facing SOPs, training videos, and tacit know-how tend instead to use **verb-like instructions** (“Install software,” “Measure pressure”). APIM checklists deliberately carry both views at once: each checklist slug has a **procedure** (noun phrase) and an **action** (verb phrase), allowing the same row to participate simultaneously in management structures (WBS, dashboards) and in execution tools (UIs, automations, agents). The goal is to reduce drift between administrative abstractions and actionable implementation by giving both sides a stable shared addressable unit.

This specification defines:

1. **The data model of a checklist slug**, including:  
   - immutable template fields (checklist, section, procedure, action, spec, instructions)  
   - deterministic **Slug ID** (template identity)  
   - deterministic **Instance ID** (deployment/asset identity)  
   - **address tuple** `(slug_id, instance_id)` as canonical runtime identity  
   - optional composite **Address ID** (32-character Base32, slug_id concatenated with instance_id)  
   - mutable state fields (result, status, comment)  
   - runtime metadata (timestamp, entity_id)  
   - relationships to other slugs

2. **Representations:**
   - **Markdown** as the human authoring format (client-side).
   - **SQLite** (or another database implementation) as the canonical runtime store (server-side).

3. **Reference encodings and interfaces:**
   - A minimal API update contract:
     - conceptually: `(slug_id, instance_id) + field(s) to update`  
     - convenience: a single `address_id` token may stand in for the tuple  
   - Example JSON/JSONL payloads for transport
   - A relationship model for dependency graphs between procedures

Operationally:

- Stakeholders author or revise checklists in Markdown using a standard template.
- In the reference implementation, a client-side parser ingests Markdown into the canonical data model and persists it via API calls into SQLite. Direct writes to SQLite are reserved for migration and maintenance tools, not for normal clients.
- Runtime systems (CLI, UI, automations, MCP tools) interact with a server that exposes the state via an API backed by SQLite.
- All client tools must, at minimum, support reading and writing checklist slugs through the API; they should not reach around the API to mutate the runtime store directly.
- At runtime, the minimal information required to update a procedure state is a **runtime address** plus one or more state fields:
  - either the address tuple `(slug_id, instance_id)`, or
  - a single composite `address_id` that reversibly encodes that tuple.  
  The runtime regenerates `timestamp` and `entity_id` from the current execution context.

The specification is written to allow:

- **Provisional rows** (new procedures added by front-line users) that are immediately executable, and can later be reviewed/edited without breaking identity for existing rows.
- **Deterministic updates**: given a stable slug template and instance principal, the same logical row always maps back to the same `(slug_id, instance_id)` pair.
- **Multiple instances** of the same checklist (or template) co-existing cleanly: each physical system, asset, or deployment uses its own `instance_id`, avoiding state bleed between machines while still sharing the same slug template definitions.

---

## 2. Terminology

This section defines the core terms used throughout the specification. Later sections (data model, storage, API, MCP) rely on these definitions.

### 2.1 Core Checklist Concepts

- **Checklist**  
  A collection of related procedures grouped for a specific context (e.g., “Computer Checks,” “Maintenance Visit,” “Site Walkthrough,” “Installation”).  
  A checklist is the primary human-visible grouping. A single checklist may be instantiated against one or more assets or deployments.

- **Checklist Section**  
  A logical grouping of procedures within a checklist (e.g., “Water Cooling System,” “Electronics,” “Floor Plan”).  
  Sections help organize large checklists for authors and operators.  
  Sections are **semantically relevant** for identity in APIM: moving a row between sections is treated as a structural change that yields a new template identity.

- **Checklist Template**  
  The authored definition of a checklist in Markdown, independent of any specific asset or deployment.  
  A template contains one or more **Checklist Template Rows**, organized into sections.  
  Templates are instance-agnostic: they do not know which specific machine, asset, or deployment will use them.

- **Checklist Template Row**  
  The immutable structural definition of one checklist item within a checklist template.  
  For the reference implementation, a template row is defined by:

  - `checklist` (the checklist name/slug)  
  - `section`  
  - `procedure` (noun phrase)  
  - `action` (verb phrase)  
  - `spec` (expected target or standard)  
  - `instructions` (freeform SOP text)

  Template rows do **not** include mutable state (`result`, `status`, `comment`) and do **not** carry runtime metadata (`timestamp`, `entity_id`). Mutable and runtime fields may be present in null or NaN states in intermediate representations, but they are not considered part of template identity.

- **Checklist State**  
  The mutable part of a checklist row (that is, a checklist slug) at runtime, representing execution outcomes for a specific instance:

  - `result`  
  - `status`  
  - `comment`  
  - `timestamp` (runtime-managed)  
  - `entity_id` (runtime-managed)

  Checklist state always attaches to a **runtime address** (see Address Tuple below).

- **Checklist Slug**  
  The canonical logical unit APIM works with: one checklist template row plus its runtime state, for a particular instance.  
  Conceptually:

  - Template fields: `checklist`, `section`, `procedure`, `action`, `spec`, `instructions`  
  - Identity fields: `slug_id`, `instance_id` (see Address Tuple)
  - Mutable state: `result`, `status`, `comment`  
  - Runtime metadata: `timestamp`, `entity_id`  
  - Relationships: outgoing links to other slugs

  In other words, a checklist slug is “this specific template row, for this specific instance, with this current state.”

### 2.2 Slug and Instance Identity

- **Slug ID**  
  A 16-character Crockford Base32 identifier that represents the identity of a **Checklist Template Row**.  
  Derived solely from the immutable template fields:

  - `checklist`  
  - `section`  
  - `procedure`  
  - `action`  
  - `spec`  
  - `instructions`

  Two template rows with identical values for all of these fields must produce the same `slug_id`.  
  Any change to any of these fields (including `instructions`) creates a **new** slug identity and thus a new `slug_id`.  
  `slug_id` has no embedded semantics; it is a compact, deterministic handle for the template row.

- **Checklist Instance**  
  A stable, deployment-defined description of the specific asset, system, or context where a checklist is being applied.  
  This is represented as an **instance principal string**, for example:

  - `machine || model=Machine_A || serial=1234`  
  - `line || line_id=Furnace_2 || plant=CT`  
  - `room || building=Lab || room=201`

  The content and structure of the principal string are defined by the deployment; the APIM core treats it as opaque text.

- **Instance ID**  
  A 16-character Crockford Base32 identifier derived from the instance principal string:

  - deterministic (same principal → same `instance_id`)  
  - opaque (no embedded semantics)  
  - stable for the lifetime of that instance in a deployment

  `instance_id` identifies **which copy** of the template we are talking about (which building, which oven, which deployment, etc.).  
  The underlying principal string is not stored in core tables; it may be kept in separate catalog tables or external systems.

### 2.3 Addressing

APIM uses two layers of “address”:

1. **Template-level identity** via `slug_id`  
2. **Runtime-level identity for a specific instance** via an **Address Tuple** `(slug_id, instance_id)`

- **Address Tuple**  
  The ordered pair:

  - `(slug_id, instance_id)`

  This is the canonical runtime identity of “one checklist slug for one checklist instance.”  
  All state and history operations conceptually target this address tuple.

  Examples:

  - “Row X of the maintenance checklist for system serial 1234”  
  - “Row Y of the installation checklist for Room 201”

- **Address ID (Composite Token)**  
  An optional 32-character Crockford Base32 identifier used as a convenience container for the address tuple.  
  Defined as the exact concatenation:

  - first 16 characters: `slug_id`  
  - last 16 characters: `instance_id`

  Formally:

  - `address_id = slug_id || instance_id`

  Properties:

  - fully reversible: given a valid 32-character `address_id`, the runtime can recover `slug_id` and `instance_id` by splitting it in the middle  
  - human/CLI-friendly: single token for logs, URLs, labels, “hello world” examples  
  - optional: the canonical key remains the address tuple `(slug_id, instance_id)`; `address_id` is a derived convenience

  APIs and tools MAY allow clients to use only `address_id` in simple update calls; internal logic MUST still treat `(slug_id, instance_id)` as the true identity.

### 2.4 State, Entities, and Relationships

- **State Fields**  
  Mutable fields and runtime metadata that represent the current outcome of a checklist slug for a given address:

  - `result` — concise observed outcome  
  - `status` — enumerated outcome (`Pass`, `Fail`, `NA`, `Other`)  
  - `comment` — freeform clarifying text  
  - `timestamp` — ISO 8601 UTC, last update, runtime-managed  
  - `entity_id` — 16-character Base32 identifier of the actor that last updated the slug’s state, runtime-managed

- **Entity**  
  Any actor that mutates slug state (human, automation, background job, or agent).  
  Entities are identified by an `entity_id` and may have additional metadata in a separate catalog.

- **Entity ID**  
  A 16-character Crockford Base32 identifier for the entity that last updated a slug’s state. It is:

  - opaque (no embedded semantics)  
  - stable per entity within a deployment  
  - set by the runtime from authentication or system context  
  - not authored in Markdown and not supplied by clients in minimal update calls

  Clients obtain the active `entity_id` from the server (auth/session metadata or a discovery endpoint); the runtime echoes it in responses and history.

- **Instructions**  
  Freeform text providing detailed guidance, SOP steps, or references associated with a template row.  
  Instructions are treated as part of template identity (they are included in the `slug_id` computation), so changing instructions creates a new slug identity. This ensures that old instructions and new instructions cannot silently share the same identity.

- **Semantic Relationship**  
  A directed triple describing how one slug relates to another at the template level:

  - `(subject_slug_id, predicate, target_slug_id)`

  Relationships are authored in terms of template identities (`slug_id`). At runtime, they are evaluated in the context of instances by combining with `instance_id` as needed.

- **Relationship Predicate**  
  A lowercase ASCII token (e.g., `depends_on`, `fulfills`, `satisfied_by`) describing how one slug relates to another.  
  Predicates are defined in the relationship model and interpreted by the state machine.

### 2.5 Storage and Runtime Terms

These terms are used when discussing the reference SQLite implementation and connectors. They do not change the conceptual model above; they are implementation details that optimize storage and indexing.

- **Normalized ID Columns** (`checklist_id`, `section_id`, `procedure_id`, `action_id`, `spec_id`, `instructions_id`)  
  Internal database identifiers that reference deduplicated lookup tables for commonly repeated text (e.g., many rows may share the same spec text “5 V”).  
  These IDs:

  - are not exposed in the canonical API/MCP surface  
  - are not part of the conceptual identity model  
  - exist solely to reduce storage duplication and improve performance

- **Runtime Store**  
  The SQLite database that holds all slugs (template plus state), relationships, and history for a given deployment.  
  The runtime store is the canonical source of truth for operational state.

- **Connector**  
  Any export/import mechanism that maps between the runtime store and external systems (JSON/JSONL exports, JSON-LD, RDF triples, etc.).

- **State Machine**  
  The logic that consumes current slug states and relationships to derive or validate updated states (for example, enforcing `depends_on` constraints or time-based rules).  
  The state machine operates strictly on the canonical identities `(slug_id, instance_id)` and the relationship graph between `slug_id`s.

---

## 3. Canonical Data Model

This section defines what a checklist slug *is*, independent of any particular storage or transport format.  
Section 2 defined the concepts; this section fixes the exact field sets and how identity, state, and relationships fit together.

A **checklist slug** at runtime consists of:

1. Template fields (immutable within a slug)
2. Identity fields (`slug_id`, `instance_id`)
3. Addressing (`(slug_id, instance_id)` and optional `address_id`)
4. State fields (mutable)
5. Relationships (template-level graph)
6. Runtime metadata (`timestamp`, `entity_id`)

---

### 3.1 Template Fields (Immutable Per Slug ID)

Template fields define *what* a row represents, independent of the instance or current state.

#### `checklist`
Stable identifier for the checklist as a whole.

Checklist names must:
- Use characters valid on Windows filesystems
- Remain stable across versions
- Match or derive from the filename when possible
- As an alternative, derive from the front-matter

A checklist may include a front-matter metadata block (author, version, date, organization) to maintain versioning independently of the filesystem. A recommended form is:

    ---
- Filename: checklist template
- Version: 0.0.1-0.5.5
- Date: 2025-11-29T15:03:00Z
- Organization: cvmewt
- Project: APIM
    ---

This metadata does not participate in slug identity unless explicitly included in other template fields.

#### `section`
Logical grouping within the checklist.  
Moving a row between sections constitutes a structural change and therefore yields a new **Slug ID**.  
Examples: `Water Cooling System`, `Oven Temperature Stability`, `Floor Plan`.

#### `procedure`
Short noun phrase naming the work item.

- Used for WBS-like structures and higher-level planning
- Should be concise; soft limit: ~32 characters
- Examples: `Oven Temperature Test`, `Oven Display Check`

#### `action`
Short verb phrase describing the operator-facing task.

- Used for execution UIs and automation prompts
- Should be concise; soft limit: ~32 characters
- Examples: `Verify Temperature Stability`, `Verify Display Reading`

#### `spec`
Expected target or standard.

- Should be concise; soft limit: ~32 characters
- Examples: `180–190 °C`, `±2 °C`

#### `instructions`
Freeform SOP text that provides the details required to perform the action.

Guidelines:
- Long form allowed
- Recommended limit: 4096 characters
- If more detail is needed, include an external link
- Use to explain procedures without bloating `procedure`/`action` names

**Identity rule:**  
All template fields above are immutable with respect to identity. Any change—including wording changes in `instructions`—produces a new `slug_id`.

---

### 3.2 Identity Fields and Addressing

Identity is split into template identity and instance identity.

#### `slug_id` (template identity)
A 16-character Crockford Base32 identifier for a template row.  
Derived deterministically from:

- `checklist`
- `section`
- `procedure`
- `action`
- `spec`
- `instructions`

Two rows with identical fields must yield the same `slug_id`.  
Any change to any field produces a new `slug_id`.

#### `instance_id`
A 16-character Crockford Base32 identifier derived from the instance principal string (Section 2.2).  
Identifies which copy of the template is in use.

#### Address tuple
The ordered pair:

    (slug_id, instance_id)

This is the canonical runtime identity for a slug.  
All state, history, and evaluation attach to this tuple.

#### `address_id` (optional)
A reversible 32-character composite token:

- First 16 characters: `slug_id`
- Last 16 characters: `instance_id`

Formally:

    address_id = slug_id || instance_id

`address_id` is a convenience for:
- APIs
- CLIs
- Logs
- Labels
- Training material (“hello world”)

Internally, the canonical key remains the address tuple `(slug_id, instance_id)`.

---

### 3.3 State Fields (Mutable Per Address)

State fields store the outcome of performing the action for a specific `(slug_id, instance_id)`.

#### `result`
Observed outcome.

- Soft UI limit: ~32 characters
- Examples: `182 °C`, `OK`, `20 mbar`

#### `status`
Enumerated outcome:

- `Pass`
- `Fail`
- `NA`
- `Other`

#### `comment`
Clarifying operator remarks.

- Recommended upper limit: 512 characters
- Should remain concise; longer narratives belong in external documents or attachments

#### `timestamp`
ISO 8601 UTC timestamp of the last update for this address.  
Runtime-managed; clients never set this field directly.

#### `entity_id`
16-character Crockford Base32 identifier of the actor that performed the update.

- Derived from runtime auth or environment
- Not authored in Markdown
- Not provided by minimal update calls

Section 7 defines how runtimes derive `entity_id` (including local-development defaults).

#### State field rules

- State fields may change arbitrarily; they do not affect `slug_id` or `instance_id`.
- `timestamp` and `entity_id` are regenerated by the runtime for every successful update.

##### Logging note (optional extension)

The core spec does not require recording every historical state change. Deployments that need logs may:

- Write client-side logs using the slug returned from each API update, or
- Maintain a separate logging database, or
- Add extension tables outside the core runtime schema.

The canonical runtime store is optimized for current state, not time-series telemetry.

---

### 3.4 Relationships (Template-Level Graph)

Relationships link template rows using `slug_id`s. They are evaluated at runtime by combining template identity with instance identity.

A relationship is a directed triple:

- `subject_slug_id`
- `predicate`
- `target_slug_id`

Where:
- `subject_slug_id` is the slug declaring the relationship
- `predicate` is a lowercase ASCII token (for example, `depends_on`, `fulfills`, `satisfied_by`)
- `target_slug_id` is the referenced slug

Key principles:

- Relationships are authored at the template level using `slug_id`s only.
- At runtime, the state machine lifts these into instance context by pairing each `slug_id` with the relevant `instance_id`.

Example: if the template declares

    SLUG_A depends_on SLUG_B

then, for instance `INST_X`, evaluation concerns

    (SLUG_A, INST_X) depends_on (SLUG_B, INST_X)

Cross-instance relationships (for example, one instance depending on another instance) are not part of the core model and, if needed, must be modeled in extensions or higher-level constructs.

#### 3.4.1 Address-Level Relationships (Optional Extension)

Template-level relationships (Section 3.4) are defined purely in terms of `slug_id`:

- `(subject_slug_id, predicate, target_slug_id)`

and are evaluated per instance by combining each `slug_id` with an `instance_id`.

Some deployments also need relationships between **specific instances**, not just “same-instance” dependencies. For this, APIM defines an optional **address-level relationship** extension.

**Address-level relationship**  
A directed triple over composite address identities:

- `(subject_address_id, predicate, target_address_id)`

where each `address_id` is the 32-character composite:

- `address_id = slug_id || instance_id`

Semantics:

- `subject_address_id` identifies one specific slug at one specific instance.
- `target_address_id` identifies another specific slug at another specific instance (which may be the same instance or a different one).
- `predicate` uses the same predicate vocabulary as template relationships (e.g., `depends_on`, `paired_with`, `satisfied_by`).

This allows patterns such as:

- “Instance A of Cube must be paired with Instance B of Cube before step Y passes.”
- “Room 201’s HVAC verification depends on Room 101’s main plant state.”

**Separation from template relationships**

Address-level relationships:

- MUST NOT replace or redefine the template-level graph.
- MUST be treated as an **overlay** on top of the template-level relationships.
- MUST NOT change semantics for other instances that are not explicitly mentioned.

A deployment may interpret the combined graph as:

- Start with all template-level relationships  
  `(subject_slug_id, predicate, target_slug_id)` applied per instance, and  
- Then add any explicit address-level relationships  
  `(subject_address_id, predicate, target_address_id)`  
  as additional edges for the named addresses.

**Storage**

Implementations SHOULD keep address-level relationships in a separate structure from template relationships, for example:

- `template_relationships(subject_slug_id, predicate, target_slug_id)`
- `address_relationships(subject_address_id, predicate, target_address_id)`

The conceptual model is unchanged; the second table is an optional overlay.

**API Shape (Outline)**

Section 7 defines concrete API endpoints. A typical separation is:

- Template relationships:  
  - `GET /api/v1/relationships?slug_id=…`  
  - `POST /api/v1/relationships` with `{ "subject_slug_id", "predicate", "target_slug_id" }`
- Address-level relationships:  
  - `GET /api/v1/address-relationships?address_id=…`  
  - `POST /api/v1/address-relationships` with `{ "subject_address_id", "predicate", "target_address_id" }`

Servers MAY also provide combined views, for example returning:

- `template_relationships`: list of `{ predicate, target_slug_id }`
- `address_relationships`: list of `{ predicate, target_address_id }`
- `has_address_relationships`: boolean hint

for a given `slug_id` or `address_id`, so UIs and agents can choose whether to auto-query address-level details.

**Markdown Authoring Guidance (Optional, Deployment-Local)**

Template relationships remain the canonical authoring surface and are expressed under `#### Relationships` in terms of `slug_id`:

- `SLUG_A depends_on SLUG_B`

Address-level relationships MAY be encoded in Markdown as a **deployment-local overlay**, for example by adding lines prefixed with `@address`:

- `@address AAAABBBBCCCCDDDDEEEEFFFFGGGGHHHH paired_with IIIIJJJJKKKKLLLLMMMMNNNNOOOOPPPP`

Rules:

- These `@address` lines are OPTIONAL and deployment-specific.  
- Importers that do not support address-level relationships MUST ignore `@address` lines safely.  
- Exporters that include address-level relationships SHOULD clearly label them as deployment-local, and MUST NOT require them for core template semantics.

In other words, template-level relationships are portable and shared; address-level relationships are an optional, deployment-local add-on that may be surfaced via separate APIs and tables but follows the same predicate vocabulary and overall relationship model.

---

### 3.5 Canonical Slug Shape (Conceptual Object)

At runtime, a fully realized slug for a specific instance has the following conceptual shape:

- Template fields:
  - `checklist`
  - `section`
  - `procedure`
  - `action`
  - `spec`
  - `instructions`

- Identity:
  - `slug_id`
  - `instance_id`
  - `address_id` (optional, derived)

- State:
  - `result`
  - `status`
  - `comment`
  - `timestamp`
  - `entity_id`

- Relationships:
  - `relationships`: list of objects shaped as `{ predicate, target_slug_id }`

Sections 5–7 define how this conceptual structure is realized in:

- Markdown authoring
- SQLite storage schema
- JSON/JSONL payloads
- HTTP API and MCP interfaces

### Echo Slug (Optional Runtime Feature)

Implementations MAY provide an “echo slug” response for client-side logging and audit purposes.

If enabled, a successful state update MAY return the fully resolved slug for the updated
address, including:

- all template fields  
- `slug_id`  
- `instance_id`  
- `address_id` (if applicable)  
- current state fields (`result`, `status`, `comment`, `timestamp`)  
- `entity_id`

Rules:

- Echo Slug MUST NOT alter state; it is a read-only serialization.
- Echo Slug MUST use the same canonical encoding and normalization rules defined in Section 4.
- Echo Slug MUST include `entity_id`, since this represents the authenticated or fallback
  actor responsible for the update.
- Echo Slug MUST be opt-in at the API level (e.g., via a request flag), so deployments that
  hide `entity_id` from operators may disable it.
- Echo Slug responses MUST be treated by clients as immutable log records.

This mechanism avoids embedding a historical log inside the runtime database while providing
a consistent, spec-defined method for external logging systems (CLI tools, UI clients,
automations) to capture complete state transitions.

## 4. Canonical Encoding and Normalization

This section defines the **canonicalization rules** that apply before computing IDs or storing identity-bearing fields. The goal is:

- identical human content → identical `slug_id`, `instance_id`, `entity_id` across deployments  
- strict, predictable behavior for tools and LLMs  
- explicit limits to avoid accidental bloat

These rules are **normative**. Any implementation that diverges from them may compute different IDs for the same authoring content.

### 4.1 Identity vs. Non-Identity Fields

**Identity-bearing fields** (inputs to IDs):

- Template identity (for `slug_id`):
  - `checklist`
  - `section`
  - `procedure`
  - `action`
  - `spec`
  - `instructions`
- Instance identity (for `instance_id`):
  - instance principal string
- Entity identity (for `entity_id`):
  - entity principal string
- Relationship predicates:
  - `predicate` (strict token)

**Non-identity fields** (do *not* affect any ID):

- State:
  - `result`
  - `status`
  - `comment`
  - `timestamp`
  - `entity_id` (value is an ID, not an input to another ID)
- Relationships:
  - `target_slug_id` (references, not inputs)
- Runtime/storage metadata:
  - history rows
  - internal numeric IDs (`checklist_id`, `section_id`, etc.)

### 4.2 Global Encoding Rules (All Text Fields)

All text fields in the spec **MUST** obey:

- Encoding:
  - MUST be UTF-8 on the wire and in storage.
- Unicode normalization:
  - MUST be normalized to Unicode NFC before any comparison or hashing.
- Line endings:
  - MUST normalize any `CRLF` (0x0D 0x0A) or `CR` (0x0D) to `LF` (0x0A) on ingest.
- Control characters:
  - MUST NOT contain ASCII control characters in the range 0x00–0x08, 0x0B, 0x0C, 0x0E–0x1F.
  - `LF` (0x0A) is allowed only where explicitly permitted (e.g., in `instructions`).
- Tabs:
  - Input TAB (0x09) is **not allowed** in identity-bearing fields (see 4.3); in other fields, deployments MAY normalize TAB → SPACE or reject input. The reference implementation treats TAB as invalid in all fields.

Any violation SHOULD result in a validation error at ingest time.

### 4.3 Canonicalization for Template Identity Fields

This applies to:

- `checklist`
- `section`
- `procedure`
- `action`
- `spec`
- `instructions` (with additional rules for multiline)

#### 4.3.1 `checklist`

- Purpose: stable identifier for the checklist.
- Canonicalization:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing Unicode whitespace.
  - MUST NOT be empty after trimming.
  - **Size**:
    - MUST NOT exceed 128 Unicode scalar values.
  - Origin:
    - MAY be derived from filename (without extension), or
    - MAY be taken from front-matter `Filename:` (see 3.1 examples).
    - If both exist, deployments MUST define a deterministic precedence rule; the reference implementation uses front-matter if present, otherwise filename.

The canonical string used for hashing is the **post-normalization** value.

#### 4.3.2 `section`

- Purpose: structural grouping; part of template identity.
- Canonicalization:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing whitespace.
  - MAY be empty (for checklists without sections).
  - **Size**:
    - MUST NOT exceed 64 Unicode scalar values.

Any change to `section` creates a new `slug_id`.

#### 4.3.3 `procedure`

- Purpose: noun phrase for the work item.
- Canonicalization:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing whitespace.
  - MUST NOT be empty after trimming.
  - **Size**:
    - MUST NOT exceed 32 Unicode scalar values.

Examples (non-normative):

- "Oven Temperature Test"
- "Oven Display Test"

#### 4.3.4 `action`

- Purpose: verb phrase for the operator-facing task.
- Canonicalization:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing whitespace.
  - MUST NOT be empty after trimming.
  - **Size**:
    - MUST NOT exceed 32 Unicode scalar values.

Examples (non-normative):

- "Verify temperature stability"
- "Verify temperature display"

#### 4.3.5 `spec`

- Purpose: concise expected target or standard for evaluation.
- Canonicalization:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing whitespace.
  - MAY be empty (for procedures without a numeric/explicit spec).
  - **Size**:
    - MUST NOT exceed 32 Unicode scalar values.

Examples (non-normative):

- "180–190 °C"
- "5 V ± 0.1 V"

Specifications like "within tolerance" SHOULD be avoided in favor of measurable statements.

#### 4.3.6 `instructions`

- Purpose: SOP text; part of template identity so that instruction changes produce new `slug_id`.
- Canonicalization:

  - MUST be UTF-8, NFC.
  - MUST normalize all line endings to `LF` (0x0A).
  - MUST NOT contain TAB.
  - Leading and trailing whitespace:
    - MUST trim leading and trailing lines that are entirely whitespace.
    - MUST trim leading and trailing whitespace *within* those boundary lines.
    - Internal lines MAY retain leading/trailing spaces (for formatting).
  - **Size**:
    - MUST NOT exceed 4096 Unicode scalar values.

- Newlines:

  - `instructions` MAY contain internal `LF`s.
  - For identity, the canonical string is the entire normalized multi-line text, including internal `LF`s.

Deployments SHOULD provide tools to help authors stay well below 4096 characters by linking out to external documents when longer material is needed.

### 4.4 Canonicalization for Instance Principals (`instance_id`)

The **instance principal string** is the input to `instance_id`. It is defined by the deployment and is treated as opaque text by the core.

Canonicalization pipeline for the principal string:

- MUST be UTF-8, NFC.
- MUST normalize line endings to `LF`.
- MUST NOT contain `LF` (instance principals are single-line).
- MUST NOT contain TAB.
- MUST trim leading and trailing whitespace.
- MUST NOT be empty after trimming.
- MAY use the literal delimiter string " || " internally, but deployments SHOULD treat that as part of their own formatting conventions.

**Size**:

- MUST NOT exceed 256 Unicode scalar values.

Example shapes (non-normative):

- "machine || model=Machine_A || serial=1234"
- "line || plant=CT || line_id=Furnace_2"
- "room || building=Lab || room=201"

After canonicalization, the principal string is:

1. fed into the hash function (see 4.6), and  
2. never stored in core tables unless a separate catalog is defined.

### 4.5 Canonicalization for Entity Principals (`entity_id`)

The **entity principal string** is the input to `entity_id`. It is also deployment-defined and opaque to the core.

Canonicalization pipeline:

- MUST be UTF-8, NFC.
- MUST normalize line endings to `LF`.
- MUST NOT contain `LF` (single-line).
- MUST NOT contain TAB.
- MUST trim leading and trailing whitespace.
- MUST NOT be empty after trimming.

**Size**:

- MUST NOT exceed 256 Unicode scalar values.

Example shapes (non-normative):

- "idp:azuread || 00000000-0000-0000-0000-000000000000"
- "system || checklist-ingest-worker-01"
- "agent || checklist-assistant-llm-v1"

After canonicalization, the principal string is used as the hash input for `entity_id`.

### 4.6 Canonical Address Strings for Hashing

This section fixes the canonical byte strings used to compute IDs.

#### 4.6.1 Slug ID (`slug_id`)

The `slug_id` is computed from a **canonical addressing string** built from the canonicalized template fields:

- `checklist`
- `section`
- `procedure`
- `action`
- `spec`
- `instructions`

Construction rules:

1. Canonicalize each field as per 4.3.
2. Join them with the literal delimiter:

       " || "

   That is, the exact string: space, vertical bar, vertical bar, space.
3. The resulting Unicode string is converted to UTF-8 bytes.
4. Compute the xxHash128 of those bytes.
5. Extract the lowest 80 bits (10 bytes) of the hash.
6. Encode the 10 bytes in Crockford Base32 (unambiguous alphabet, uppercase), yielding a 16-character `slug_id`.

Constraints:

- None of the fields may contain the literal " || " sequence; ingest must reject such input for identity-bearing fields.
- Any change in any template field after canonicalization (including minor edits to `instructions`) leads to a different `slug_id`.

#### 4.6.2 Instance ID (`instance_id`)

The `instance_id` is computed from the **canonical instance principal string** after processing in 4.4:

1. Take the canonical principal string (single-line, NFC, trimmed).
2. Convert to UTF-8 bytes.
3. Compute xxHash128.
4. Extract lowest 80 bits (10 bytes).
5. Encode in Crockford Base32 (uppercase), yielding a 16-character `instance_id`.

The mapping principal → `instance_id` MUST be deterministic within a deployment.

#### 4.6.3 Entity ID (`entity_id`)

The `entity_id` is computed analogously, from the canonical entity principal string (4.5):

1. Canonical principal string → UTF-8 bytes.
2. xxHash128.
3. Lowest 80 bits.
4. Crockford Base32 (uppercase) → 16-character `entity_id`.

Deployments MAY choose a different hash function, but MUST preserve:

- deterministic mapping,
- 80-bit entropy,
- 16-character Crockford Base32, uppercase encoding.

### 4.7 Non-Identity Field Rules (Result, Status, Comment)

Although non-identity fields do not affect IDs, they still have strict constraints.

#### 4.7.1 `result`

- Purpose: concise observed outcome for comparison with `spec`.
- Rules:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing whitespace.
  - MAY be empty.
  - **Size**:
    - MUST NOT exceed 32 Unicode scalar values.

Examples (non-normative):

- "182 °C"
- "4.98 V"
- "OK"

#### 4.7.2 `status`

- Purpose: enumerated evaluation outcome.
- Rules:

  - MUST be one of:
    - "Pass"
    - "Fail"
    - "NA"
    - "Other"
  - MUST be case-sensitive as above.
  - Any other value MUST be rejected at ingest.

#### 4.7.3 `comment`

- Purpose: operator or system commentary.
- Rules:

  - MUST be UTF-8, NFC.
  - MUST NOT contain `LF` (single-line).
  - MUST NOT contain TAB.
  - MUST trim leading and trailing whitespace.
  - MAY be empty.
  - **Size**:
    - MUST NOT exceed 512 Unicode scalar values.

Deployments needing multi-line commentary SHOULD store it in external systems or derived views, not in `comment`. Such a system should encourage the comment to reference this alternative verbose storage (ex. File path or URL to a photo can make for a good comment)

### 4.8 Newline Normalization Summary

For clarity:

- Fields that MUST be **single-line** (no `LF`):

  - `checklist`
  - `section`
  - `procedure`
  - `action`
  - `spec`
  - instance principal string
  - entity principal string
  - `result`
  - `comment`

- Fields that MAY be **multi-line** (with normalized `LF`):

  - `instructions`

All inputs MUST normalize any CRLF/CR sequences to LF before validation, and any remaining `LF` MUST obey the above rules.

### 4.9 Invariants

Given the rules above:

- Any two deployments that:
  - ingest the same Markdown template,
  - derive the same instance and entity principal strings,

  will compute identical:

  - `slug_id` for each template row,
  - `instance_id` for each instance principal,
  - `entity_id` for each entity principal,
  - `address_id` for each `(slug_id, instance_id)` pair.

- Identity is **stable** under:

  - storage round-trips,
  - JSON/JSONL exports,
  - SQLite migrations,

  as long as canonicalization is applied consistently on ingest.

- Deviating from these canonicalization rules (e.g., preserving CRLF, allowing TABs, skipping NFC) is considered **non-conformant** and may result in divergent IDs for otherwise identical content.

## 5. Markdown Authoring Specification (Canonical at Authoring Time)

Markdown is the canonical human authoring format. One Markdown file defines one **Checklist Template**. The runtime store (SQLite) is canonical for state; Markdown is canonical for structure and intent.

This section defines the **only** Markdown layout that tools MUST accept as canonical.

---

### 5.1 File-Level Structure

A checklist file consists of:

1. Optional metadata front-matter block.
2. One required checklist heading (level 1).
3. Zero or more sections (level 2).
4. Under each section, zero or more **procedure blocks** (level 3).
5. Fixed bullet fields and subsections under each procedure block.

The overall pattern is:

- optional front-matter metadata
- `# Checklist: <checklist>`
- `## Section: <section>`
- `### Procedure: <procedure>`
  - bullets for `Action`, `Spec`, `Result`, `Status`, `Comment`
  - `#### Instructions`
  - `#### Relationships`

---

### 5.2 Optional Front-Matter Metadata

A file MAY start with a simple metadata block:

    ---
    Filename: checklist template
    Version: 0.0.1-0.5.5
    Date: 2025-11-29T15:03:00Z
    Organization: cvmewt
    Project: Checklist Assistant
    ---

Rules:

- The delimiter lines MUST be exactly three hyphens (`---`) on their own line.
- Keys MUST be simple ASCII words with a colon separator.
- Front-matter is **not** used in `slug_id` computation.
- If a `Checklist:` key is present here, it MUST match the `<checklist>` text in the level-1 heading exactly. Otherwise, ingestion MUST treat it as an error.

Front-matter is purely informational and for versioning; ingest tools MAY ignore it except for validation and display.

---

### 5.3 Checklist Heading (Level 1)

Immediately after optional front-matter, the file MUST contain exactly one checklist heading:

    # Checklist: <checklist>

Rules:

- The literal prefix `Checklist:` MUST appear exactly (case-sensitive, with trailing colon and space).
- `<checklist>` is the checklist name used as the `checklist` template field.
- The `<checklist>` text MUST respect the constraints from Section 3 (UTF-8 NFC, character and length rules, Windows filename-safe if used as filenames).

No other level-1 headings are allowed in the file.

---

### 5.4 Section Headings (Level 2)

Each section is introduced by a level-2 heading:

    ## Section: <section>

Rules:

- The literal prefix `Section:` MUST appear exactly.
- `<section>` becomes the `section` template field for all procedure blocks that follow, until the next `## Section:` heading or end of file.
- Sections MAY appear in any order.
- Sections MUST NOT be nested (no section under another section).

A file MAY have zero or many sections. If no `## Section:` appears, ingestion MUST treat this as an error (every procedure requires a section).

---

### 5.5 Procedure Blocks (Level 3)

Each checklist row (template-level) is represented by a **procedure block**:

    ### Procedure: <procedure>
    - Action: <action>
    - Spec: <spec>
    - Result: <result or empty>
    - Status: <Pass|Fail|NA|Other or empty>
    - Comment: <comment or empty>

    #### Instructions
    <freeform markdown text>

    #### Relationships
    - <predicate> <target_slug_id>
    - <predicate> <target_slug_id>

Rules:

1. Heading:
   - The level-3 heading MUST start with `### Procedure:`.
   - `<procedure>` becomes the `procedure` template field.
   - `<procedure>` MUST obey the length and encoding limits from Section 4 (hard limit, UTF-8 NFC, no newlines).

2. Bullet field block:
   - Exactly five bullets MUST appear immediately after the procedure heading, in this fixed order:

        - Action: ...
        - Spec: ...
        - Result: ...
        - Status: ...
        - Comment: ...

   - Bullet markers MUST be a single hyphen + single space (`- `).
   - Field name capitalization and spelling MUST be exactly: `Action`, `Spec`, `Result`, `Status`, `Comment`.
   - A colon and single space (`: `) MUST follow the field name.
   - The remainder of the line is the field value; leading/trailing whitespace after the colon is trimmed.
   - `Action` and `Spec` MUST be non-empty and satisfy their length/encoding constraints.
   - `Result`, `Status`, `Comment` MAY be empty at authoring time.
   - If `Status` is non-empty, its value MUST be one of `Pass`, `Fail`, `NA`, `Other`.

3. Ordering:
   - No other content may appear between `### Procedure: <procedure>` and the first `- Action:` bullet.
   - No blank lines are required between bullets, but blank lines are allowed.

Multiple `### Procedure:` blocks MAY appear under a single `## Section:`. Each procedure block expands into one template row and one `slug_id`.

---

### 5.6 Instructions Subsection (Level 4)

Every procedure block MUST include exactly one `Instructions` subsection:

    #### Instructions
    <freeform markdown text, zero or more lines>

Rules:

- The heading MUST be exactly `#### Instructions` (no colon).
- All text from the first line after this heading up to (but not including) the next `####` or `###` or `##` or end-of-file belongs to the `instructions` field.
- Any valid Markdown is allowed in the instructions body (links, lists, inline code, etc.), subject to the size and encoding limits from Section 4.
- Authors SHOULD place detailed step-by-step SOP content, safety notes, and references here.
- For longer content, authors SHOULD link to external documents rather than embedding full procedures.

Instructions content participates in `slug_id` identity: changing instructions changes `slug_id`.

---

### 5.7 Relationships Subsection (Level 4)

Every procedure block MAY include a `Relationships` subsection:

    #### Relationships
    - <predicate> <target_slug_id>
    - <predicate> <target_slug_id>

Rules:

- The heading MUST be exactly `#### Relationships`.
- Each bullet line under this heading (until the next `####` or `###` or `##` or end-of-file) describes one outgoing relationship.
- Bullet markers MUST be `- `.
- Each bullet MUST have the form:

      <predicate> <target_slug_id>

  where:
  - `<predicate>` is a lowercase ASCII token (e.g. `depends_on`, `fulfills`, `satisfied_by`) defined in the relationship model.
  - `<target_slug_id>` is a 16-character Crockford Base32 `slug_id` in canonical form.

- A single space MUST separate `<predicate>` and `<target_slug_id>`.
- No additional trailing text is allowed on the line (no inline comments).

Notes:

- Relationships are authored in terms of template identities (`slug_id`). Practical editing workflows can:
  - generate `slug_id`s on an initial ingest, and
  - help authors insert or autocomplete `target_slug_id` values.
- Ingestion MUST treat any malformed relationship line as a warning and ignore that line, without aborting the entire file.

If a procedure has no relationships, the whole `#### Relationships` section MAY be omitted.

---

### 5.8 State Bullets at Authoring Time

The bullets:

- `Result:`
- `Status:`
- `Comment:`

are present in the canonical Markdown layout but are **not** authoritative at runtime.

Rules:

- Ingest tools MAY use initial `Result`, `Status`, `Comment` values as a seed for initial state on first import.
- Once the slug exists in the runtime store, all subsequent state changes MUST flow through the API/MCP; Markdown values are ignored on re-ingest except for compatibility or migration tools.
- Deployments that wish to treat Markdown state as advisory MUST do so explicitly and document that behavior separately from this spec.

---

### 5.9 Parsing Rules (Normative)

Given a Markdown file, ingestion MUST:

1. Normalize line endings to `\n` and text to UTF-8 NFC before parsing.
2. Optionally parse and validate front-matter, then discard it for identity.
3. Locate the single `# Checklist:` heading; extract `<checklist>` as the `checklist` template field.
4. For each `## Section:` heading in order:
   - Set the current `section` template field to `<section>`.
5. For each `### Procedure:` heading under the current section:
   - Create a new template row.
   - Read the next five bullet lines in order (`Action`, `Spec`, `Result`, `Status`, `Comment`), enforcing field names and formats.
   - Attach the immediately following `#### Instructions` block as `instructions`.
   - Attach any `#### Relationships` block as a list of `{ predicate, target_slug_id }`.
   - Compute `slug_id` from the template fields as defined in Section 4.

Validation:

- Missing or misordered bullets MUST cause ingestion to fail for that procedure block (and SHOULD be reported clearly).
- Missing `#### Instructions` MUST cause ingestion to fail for that procedure block.
- Unknown or malformed `Status` values MUST be rejected.
- Violations of length/encoding constraints MUST be rejected.

A file with any invalid procedure blocks MAY still ingest valid blocks, but deployments SHOULD log and surface such partial-ingest conditions to authors.

---

### 5.10 Minimal Example

A minimal well-formed checklist file:

    # Checklist: Oven Temperature Checks

    ## Section: Temperature Stability

    ### Procedure: Oven Temperature Test
    - Action: Verify oven temperature stability
    - Spec: 180–190 °C
    - Result: 
    - Status: 
    - Comment: 

    #### Instructions
    Preheat the oven to setpoint and monitor for at least 10 minutes.
    Record the stabilized reading from the reference thermometer.

    #### Relationships
    - depends_on ABCDEFGHJKMNPQRS

    ## Section: Display Verification

    ### Procedure: Oven Display Test
    - Action: Verify oven temperature display
    - Spec: Display within ±2 °C of reference
    - Result: 
    - Status: 
    - Comment: 

    #### Instructions
    Compare the oven's built-in display with the reference thermometer at the stabilized temperature.

    #### Relationships
    - depends_on ABCDEFGHJKMNPQRS

Each `### Procedure:` block above becomes one `slug_id`. At runtime, combining each `slug_id` with a chosen `instance_id` yields the address tuple `(slug_id, instance_id)` and (optionally) the composite `address_id`.

Note: In the typical workflow, each `### Procedure:` block becomes one `slug_id`. This is usually sufficient for authoring and review. However, as defined in Section 2.2, `slug_id` depends on ALL immutable template fields (`checklist`, `section`, `procedure`, `action`, `spec`, `instructions`). Any change to any of these fields creates a NEW `slug_id`. Authors should be aware that revising a checklist (renaming sections, editing procedures, or updating instructions) will create new slugs and may leave older slugs present in a deployment until they are explicitly removed or migrated.

## 6. SQLite Storage Specification (DB storage is Canonical at Runtime)

SQLite is the canonical runtime store for APIM checklists. All slug data,
relationships, entities, and state histories are persisted here. Markdown and
the Entity Principal Template Checklist are transformed (via the API/MCP)
into the canonical data model and stored in SQLite; all API operations read
and write exclusively through this runtime database.

The schema is optimized for:

- deterministic identity (`slug_id`, `instance_id`, `entity_id`, `address_id`)
- compact storage
- fast lookup by ID
- efficient traversal of relationship graphs
- compatibility with future connectors and exports

All writes (including ingestion) MUST go through the API/MCP layer; no
component is permitted to bypass the API and write directly to SQLite.

> Principle: The core of the server should be able to scale and run on small hardware if needed, allowing very secure localized embeded deployments capable of communicating with clients. This allows for local checklist utilization for security sensitive situations and procedures (i.e. government, research, proprietary processes) yet still maintains the same format for easily sharing exports and or security agnostic summaries with a larger more broadly accessible deployment

### 6.1 Overview of Tables

Core tables:

1. `slugs` — current state of every checklist slug, keyed by `address_id`
2. `template_relationships` — directed template-level edges keyed by `slug_id`
3. `address_relationships` — optional address-level edges keyed by `address_id`
4. `history` — append-only snapshots of mutable state over time
5. `entities` — catalog of entities (humans, robots, agents) keyed by `entity_id`

Optional governance/metadata tables:

6. `predicates` — catalog of known relationship predicates (standard/extension)
7. `instance_catalog` — catalog of instances keyed by `instance_id` (deployment-local)

Additional indexes support fast lookup and graph traversal.

### 6.2 Table: `slugs`

`slugs` stores the canonical representation of each checklist slug at runtime:
template fields, identity, mutable state, and runtime metadata.

    CREATE TABLE slugs (
        address_id   TEXT PRIMARY KEY,  -- 32-char Base32 = slug_id || instance_id

        -- Template fields (immutable per slug_id)
        checklist    TEXT NOT NULL,
        section      TEXT NOT NULL,
        procedure    TEXT NOT NULL,
        action       TEXT NOT NULL,
        spec         TEXT NOT NULL,
        instructions TEXT NOT NULL,

        -- Identity decomposition (redundant but query-friendly)
        slug_id      TEXT NOT NULL,     -- 16-char Base32
        instance_id  TEXT NOT NULL,     -- 16-char Base32

        -- Mutable state
        result       TEXT,
        status       TEXT CHECK (status IN ('Pass','Fail','NA','Other')),
        comment      TEXT,

        -- Runtime metadata
        timestamp    TEXT,              -- ISO8601 UTC
        entity_id    TEXT NOT NULL,     -- 16-char Base32

        FOREIGN KEY (entity_id) REFERENCES entities(entity_id)
    );

Rules:

- `address_id` is the canonical primary key: `slug_id || instance_id`.
- Template fields (`checklist`, `section`, `procedure`, `action`, `spec`,
  `instructions`) MUST match canonicalized template content and MUST NOT be
  mutated in place. Any change to these fields produces a new `slug_id` and
  therefore a new `address_id`.
- `slug_id` and `instance_id` are stored explicitly for convenience and
  indexing; they MUST match the decomposition of `address_id`.
- `result`, `status`, `comment`, `timestamp`, `entity_id` are mutable and
  updated only via the minimal update contract (Section 10).
- `entity_id` MUST always reference a valid row in `entities`.

### 6.3 Relationship Tables

APIM stores relationships at two distinct layers:

- **Template-level** — between template identities (`slug_id`)
- **Address-level** — between runtime addresses (`address_id`)

The core model only requires template-level relationships; address-level
relationships are an optional overlay.

#### 6.3.1 Table: `template_relationships`

Template relationships encode relationships between template rows, independent
of instances. They implement the triples described in Section 3.4.

    CREATE TABLE template_relationships (
        subject_slug_id  TEXT NOT NULL,    -- 16-char Base32
        predicate        TEXT NOT NULL,    -- lowercase token
        target_slug_id   TEXT NOT NULL,    -- 16-char Base32

        FOREIGN KEY (subject_slug_id) REFERENCES slugs(slug_id),
        FOREIGN KEY (target_slug_id)  REFERENCES slugs(slug_id)
    );

Semantics:

- Each row is one directed triple  
  `(subject_slug_id, predicate, target_slug_id)`.
- Relationships are authored in Markdown in terms of `slug_id` and projected
  to instances by pairing with `instance_id` at evaluation time.
- The state machine uses these edges to evaluate dependency and roll-up logic.

#### 6.3.2 Table: `address_relationships` (Optional Extension)

Address-level relationships are an optional overlay for deployments that need
relationships between specific instances, as defined in Section 3.4.1.

    CREATE TABLE address_relationships (
        subject_address_id  TEXT NOT NULL,   -- 32-char Base32
        predicate           TEXT NOT NULL,
        target_address_id   TEXT NOT NULL,   -- 32-char Base32

        FOREIGN KEY (subject_address_id) REFERENCES slugs(address_id),
        FOREIGN KEY (target_address_id)  REFERENCES slugs(address_id)
    );

Rules:

- `address_relationships` MUST NOT replace `template_relationships`; they are
  strictly an overlay.
- State machine semantics remain defined in terms of predicates; address-level
  edges simply restrict those semantics to specific `(slug_id, instance_id)`
  pairs.
- A deployment MAY omit this table entirely if it does not use address-level
  relationships.

#### 6.3.3 Table: `predicates` (Optional Governance)

The `predicates` table tracks vocabulary for relationship predicates without
preventing the use of free-text predicates.

    CREATE TABLE predicates (
        name        TEXT PRIMARY KEY,   -- lowercase token, e.g. 'depends_on'
        kind        TEXT NOT NULL,      -- e.g. 'standard', 'extension'
        status      TEXT NOT NULL,      -- e.g. 'active', 'deprecated'
        description TEXT,
        meta        TEXT                -- optional JSON (aliases, owner, etc.)
    );

Rules:

- Relationship rows (`template_relationships`, `address_relationships`) store
  `predicate` as plain `TEXT`.
- No foreign key from relationships to `predicates` is required by the core
  spec; deployments MAY enable one after their vocabulary stabilizes.
- The API/MCP layer MAY:
  - accept any lowercase ASCII predicate token,
  - warn when the predicate does not appear in `predicates` or is marked
    `deprecated`,
  - still ingest the relationship without failure.
- Free-text predicates are allowed and primarily signal “needs review” or
  provide hints for downstream tooling.

### 6.4 Table: `history` (Optional Audit Log)

The `history` table records snapshot state changes over time. It is optional
but strongly recommended for auditability, review, and analytics.

    CREATE TABLE history (
        address_id  TEXT NOT NULL,   -- 32-char Base32
        timestamp   TEXT NOT NULL,   -- ISO8601 UTC
        result      TEXT,
        status      TEXT,
        comment     TEXT,
        entity_id   TEXT NOT NULL,   -- 16-char Base32

        FOREIGN KEY (address_id) REFERENCES slugs(address_id)
            ON UPDATE CASCADE
            ON DELETE CASCADE,
        FOREIGN KEY (entity_id) REFERENCES entities(entity_id),

        PRIMARY KEY (address_id, timestamp)
    );

Rules:

- Each row is a snapshot of mutable state for one `address_id` at one
  `timestamp`.
- The primary key `(address_id, timestamp)` ensures unique historical points.
- Snapshots are appended when meaningful changes occur (status, result,
  comment), not necessarily every read or evaluation.
- `entity_id` records which entity performed the update, resolved via the
  entity-principal checklist and identity process.

### 6.5 Table: `entities` (Canonical Entity Catalog)

The `entities` table is the canonical catalog of entities that can perform
updates (humans, robots, agents, CI jobs, etc.). Every `entity_id` used in
`slugs` and `history` MUST appear here.

    CREATE TABLE entities (
        entity_id    TEXT PRIMARY KEY,   -- 16-char Base32
        principal    TEXT NOT NULL,      -- canonical entity principal string
        kind         TEXT NOT NULL,      -- e.g. 'human', 'robot', 'agent', 'system'
        display_name TEXT,               -- e.g. "J. Smith", "ci-runner-01"
        meta         TEXT                -- optional JSON (auth IDs, roles, etc.)
    );

Rules:

- `entity_id` is derived deterministically from the canonical entity principal
  string (Section 4.5 and 4.6.3).
- `principal` is the canonical UTF-8/NFC, single-line string used as the hash
  input; it is produced by the Entity Principal Template Checklist (see
  Section 6.8.2).
- `kind` and `display_name` are deployment-defined and used for display and
  reporting.
- `meta` MAY contain structured data (e.g. external auth IDs, email, rights,
  group membership); handling of PII is deployment-specific.
- `entities` is REQUIRED; no `slugs` or `history` rows may reference an
  `entity_id` that is not in this table.

### 6.6 Table: `instance_catalog` (Optional)

Deployments MAY choose to catalog instances for easier lookup and UI labeling:

    CREATE TABLE instance_catalog (
        instance_id  TEXT PRIMARY KEY,   -- 16-char Base32
        principal    TEXT NOT NULL,      -- canonical instance principal string
        label        TEXT,               -- human-friendly label (e.g. "SEM-3")
        meta         TEXT                -- optional JSON (location, serial, etc.)
    );

Rules:

- This table is optional and purely informational; the runtime identity rules
  for `instance_id` remain governed by canonicalization (Sections 4.4 and
  4.6.2).
- `principal` is the canonical instance principal string used for hashing; it
  SHOULD match the content derived from the instance checklist.
- `label` and `meta` are deployment-local and may be omitted or redacted in
  exports.

### 6.7 Indexes

Recommended indexes for performance:

    CREATE INDEX idx_slugs_slug_id          ON slugs(slug_id);
    CREATE INDEX idx_slugs_instance_id      ON slugs(instance_id);
    CREATE INDEX idx_slugs_checklist        ON slugs(checklist);

    CREATE INDEX idx_trel_subject           ON template_relationships(subject_slug_id);
    CREATE INDEX idx_trel_target            ON template_relationships(target_slug_id);

    CREATE INDEX idx_arel_subject           ON address_relationships(subject_address_id);
    CREATE INDEX idx_arel_target            ON address_relationships(target_address_id);

    CREATE INDEX idx_history_entity         ON history(entity_id);
    CREATE INDEX idx_entities_kind          ON entities(kind);

Deployments MAY add further indexes for frequently queried fields (e.g.
`section`, `procedure`, or `checklist` + `status` combinations).

### 6.8 Mapping Markdown / Entity Principal Template / API → SQLite

All mutations of the runtime store MUST occur via API/MCP endpoints. Markdown
files and the Entity Principal Template Checklist are treated as clients that
produce canonical objects and send them through the same write contracts as
any other tool.

#### 6.8.1 Markdown Checklist Ingestion

1. **Client-side parsing**
   - Normalize Markdown to UTF-8 NFC and `\n` line endings.
   - Parse `checklist`, `section`, `procedure`, `action`, `spec`,
     `instructions`, and relationship bullets as defined in Section 5.
   - Create in-memory template rows and template relationships.

2. **ID derivation**
   - Canonicalize template fields and compute `slug_id`.
   - Construct the instance principal string for the deployment’s default
     instance or a specific instance checklist and compute `instance_id`.
   - Compute `address_id = slug_id || instance_id`.

3. **API/MCP submission**
   - For each `(slug_id, instance_id)` pair:
     - Create or upsert the slug via the **full slug creation contract** or
       minimal update contract (Section 10).
   - For each template relationship:
     - Call the template-relationship creation endpoint with
       `(subject_slug_id, predicate, target_slug_id)`.

4. **Server-side behavior**
   - The server validates fields, computes IDs, writes into `slugs`,
     `template_relationships`, and `history` (if enabled), and returns
     structured responses.
   - Any state-machine evaluation is read-only and may attach warnings or
     evaluation flags to responses.

Markdown ingestion is “just another client”: it does not bypass the API, and
it uses the same contracts as UI tools, agents, and automation.

#### 6.8.2 Entity Principal Template Checklist (Required)

Entity identity is defined via a unified **Entity Principal Template
Checklist**. For each entity (human, robot, agent, CI job, webhook, etc.):

1. An external authentication provider (SSO/OIDC/API-key/CI, etc.) validates
   credentials and yields a set of validated attributes (issuer, subject,
   handle, robot-id, fingerprint, roles, etc.).
2. A client or gateway fills in the Entity Principal Template Checklist using
   these attributes as `result` values for its rows.
3. The entity-principal logic (client or gateway) builds the canonical
   entity principal string from those checklist values, following the
   canonicalization rules in Sections 4.5 and 11.4.1.
4. The client computes `entity_id` from the principal string or submits the
   principal string to the server, which computes `entity_id`.
5. The client calls the API to:
   - ensure the entity’s slugs in `slugs` are present (the principal checklist
     itself is stored like any other checklist), and
   - upsert a row in `entities` with `(entity_id, principal, kind,
     display_name, meta)`.

Rules:

- No `entity_id` may appear in `slugs` or `history` unless it has been issued
  via this entity principal process.
- Authentication remains external: APIM does not accept or validate raw
  passwords, secrets, or OAuth codes. It only canonicalizes already-validated
  identity attributes.
- Every actor that can update slugs MUST have a corresponding `entity_id`
  derived from the Entity Principal Template Checklist.

This creates symmetry with instance identity: both instances and entities are
identified by principal strings, derived from checklists, and hashed into
stable IDs.

### 6.9 Treatment of Updates

State updates via API/MCP:

- identify the target slug by `address_id`
- modify only mutable fields: `result`, `status`, `comment`
- cause the server to:
  - regenerate `timestamp` (current UTC, ISO8601)
  - resolve the active `entity_id` (derived via the entity principal process)
  - write the updated row in `slugs`
  - append a snapshot row in `history` (if enabled)

Rules:

- Addressing fields (`checklist`, `section`, `procedure`, `action`, `spec`,
  `instructions`, `slug_id`, `instance_id`, `address_id`) MUST NOT be mutated
  by the minimal update contract.
- Any change to template fields MUST create a new slug (new `slug_id`,
  `address_id`) via the full creation contract.
- Relationships MUST be created/updated via dedicated relationship endpoints,
  never by directly mutating relationship tables.
- Internal server-side automation (e.g., state-machine-driven roll-up logic,
  dependency enforcement) MUST write changes using the same minimal update
  contract as external clients; internal modules MUST NOT bypass the API
  semantics when persisting state.

### 6.10 Deletions, Deactivation, and Regeneration

Deletions:

- Slugs SHOULD be soft-deleted by marking them inactive via a checklist or
  deployment-local field (e.g., a dedicated “Retired” checklist) rather than
  dropping rows.
- Hard deletion of slugs or entities SHOULD be reserved for administrative
  cleanup, privacy/PII erasure, or repair of corrupted data.
- When a slug is deleted, template and address-level relationships referencing
  it SHOULD be removed or marked invalid as part of a maintenance process.

Regeneration:

- Given the complete set of Markdown templates and entity/instance principal
  checklists, a deployment can rebuild:
  - slugs (template and instance identity),
  - template relationships,
  - entities,
  - optionally instance_catalog.
- `history` is not regenerated from authoring content and MUST be backed up
  separately if audit trails are required.

### 6.11 SQLite Configuration Notes

Recommended SQLite configuration:

- `PRAGMA encoding = "UTF-8";`
- `PRAGMA journal_mode = WAL;` for better concurrency.
- `PRAGMA foreign_keys = ON;` to enforce `entities` / `slugs` / `history`
  consistency.
- `PRAGMA synchronous = NORMAL;` or `FULL` depending on durability needs.

With appropriate indexing and WAL mode, SQLite can comfortably support
hundreds of thousands to millions of slugs, relationships, and history rows
for typical APIM deployments, while remaining embeddable and easy to back up.

## 7. Transport Interfaces (HTTP API, JSON-RPC, MCP)

This section defines the canonical transport interfaces for APIM:

- REST/HTTP+JSON API for general use.
- JSON-RPC for batch and graph/evaluation operations.
- MCP tools for LLM/agent access.

All transports:

- Operate only on the canonical SQLite runtime store (Section 6).
- Respect identity and canonicalization rules (Sections 3–4, 11).
- Use the minimal update and full creation contracts (Section 10).
- Never mutate addressing fields of existing slugs in place.

### 7.1 API Shape, Versioning, Envelopes

#### 7.1.1 Base Paths and Versioning

- REST base path: `/api/v1/`
- JSON-RPC base path: `/rpc/v1`
- MCP tools: deployment-defined, but MUST target v1 semantics.

A deployment MAY expose `/api/v2/`, `/rpc/v2`, etc. Each version MUST:

- be self-contained, and
- not silently alter v1 semantics.

#### 7.1.2 Content Types

- REST and JSON-RPC:
  - `Content-Type: application/json; charset=utf-8`
- JSONL exports (where provided):
  - `Content-Type: application/x-ndjson; charset=utf-8`

#### 7.1.3 Response Envelope (REST and JSON-RPC)

All successful responses MUST use a fixed envelope:

```json
{
  "ok": true,
  "data": { },
  "warnings": []
}
```

- `data` MAY be an object or an array, depending on endpoint.
- `warnings` MAY be omitted if empty.

All errors MUST use:

```json
{
  "ok": false,
  "error": {
    "code": "INVALID_FIELD",
    "message": "Status must be Pass, Fail, NA, or Other",
    "details": {
      "field": "status",
      "allowed": ["Pass","Fail","NA","Other"]
    }
  }
}
```

Rules:

- `code` is a stable, machine-readable string (e.g., `NOT_FOUND`,
  `INVALID_FIELD`, `UNAUTHORIZED`, `CONFLICT`, `INTERNAL_ERROR`).
- `message` is human-readable, in English by default.
- `details` MAY contain structured context for debugging or UI.

This envelope MUST be used consistently for:

- REST endpoints,
- JSON-RPC method results,
- MCP tool responses (modulo MCP’s own wrapping, if any).

#### 7.1.4 Common Query Parameters

List endpoints MAY accept:

- `limit` (integer, page size, default and max deployment-defined),
- `offset` or `cursor` (for pagination),
- basic filters (e.g., `checklist`, `status`, `kind`).

Pagination behavior MUST be stable and documented per endpoint.

### 7.2 Authentication and Entity Resolution

APIM assumes:

- **External authentication** (OAuth/OIDC/SSO/API key/CI token) validates
  the caller.
- APIM receives a set of validated identity attributes (issuer, subject,
  client, robot-id, etc.), not raw credentials.

#### 7.2.1 Auth Model

- External auth is REQUIRED for normal usage.
- Local API keys MAY be supported for bootstrap and maintenance.
- APIM itself:
  - MUST NOT accept or store passwords or raw OAuth codes.
  - MUST treat external auth as the canonical source of “who is calling”.

#### 7.2.2 Entity Principal Resolution

Every authenticated caller MUST map to a stable `entity_id` via the **Entity
Principal Template Checklist** (Section 6.8.2):

1. External auth produces a set of validated attributes.
2. A gateway or client fills in the Entity Principal Template Checklist with
   those attributes as `result` fields.
3. A canonical entity principal string is derived from that checklist.
4. The principal string is canonicalized and hashed to a 16-character
   `entity_id` (Sections 4.5 and 4.6.3).
5. APIM ensures a row exists in `entities` with that `entity_id`.

All write operations:

- MUST resolve the caller to an `entity_id`.
- MUST record that `entity_id` in `slugs` and `history`.

APIM MAY expose helper endpoints to assist this process (below), but the
identity mapping logic MUST be deterministic and deployment-controlled.

### 7.3 REST API: Entities

These endpoints manage and inspect entries in the `entities` catalog (Section 6.5).

#### 7.3.1 POST `/api/v1/entities`

Purpose: ensure an `entity_id` exists for a canonical principal.

Request:

```json
{
  "principal": "idp:azuread || 00000000-0000-0000-0000-000000000000",
  "kind": "human",
  "display_name": "J. Smith",
  "meta": {
    "email": "jsmith@example.com",
    "roles": ["admin"]
  }
}
```

Behavior:

- Canonicalize `principal` per Section 4.5.
- Derive `entity_id` (Sections 4.5, 4.6.3).
- Upsert into `entities`:
  - `entity_id`, `principal`, `kind`, `display_name`, `meta`.
- Return:

```json
{
  "ok": true,
  "data": {
    "entity_id": "ABCDEFGHJKMNPQRS",
    "principal": "...",
    "kind": "human",
    "display_name": "J. Smith",
    "meta": { ... }
  },
  "warnings": []
}
```

Notes:

- If the row already exists with the same `principal` and `entity_id`, this
  is idempotent.
- If `kind` / `display_name` / `meta` differ, the server MAY update them
  or MAY reject with `CONFLICT`, depending on deployment policy.

#### 7.3.2 GET `/api/v1/entities/{entity_id}`

Return one entity:

```json
{
  "ok": true,
  "data": {
    "entity_id": "ABCDEFGHJKMNPQRS",
    "principal": "...",
    "kind": "human",
    "display_name": "J. Smith",
    "meta": { ... }
  },
  "warnings": []
}
```

- `NOT_FOUND` if unknown.

#### 7.3.3 GET `/api/v1/entities`

List entities with optional filters:

- `kind` (e.g., `human`, `robot`, `agent`, `system`)
- `limit`, `cursor` or `offset`

Response:

```json
{
  "ok": true,
  "data": {
    "items": [
      { "entity_id": "...", "kind": "...", "display_name": "...", "meta": { } }
    ],
    "next_cursor": "..."
  },
  "warnings": []
}
```

Deleting entities is deployment-specific and MAY be restricted; the core spec
does not require a delete endpoint.

### 7.4 REST API: Slugs

Slugs are the central operational object. REST endpoints cover:

- Creation (full slug creation contract).
- Retrieval and listing.
- Minimal state updates.
- Optional bulk operations.

#### 7.4.1 Slug Representation (REST)

REST returns slugs with the canonical shape:

```json
{
  "address_id": "SLUGIDSLUGIDINSTIDINSTID",
  "slug_id": "SLUGIDSLUGIDSLUG",
  "instance_id": "INSTIDINSTIDINS",
  "checklist": "Oven Temperature Checks",
  "section": "Temperature Stability",
  "procedure": "Oven Temperature Test",
  "action": "Verify oven temperature stability",
  "spec": "180–190 °C",
  "instructions": "Preheat the oven ...",

  "result": "182 °C",
  "status": "Pass",
  "comment": "",
  "timestamp": "2025-11-29T15:20:11Z",
  "entity_id": "ABCDEFGHJKMNPQRS"
}
```

#### 7.4.2 POST `/api/v1/slugs`

Create a new slug (full creation contract; see Section 10.8).

Request:

```json
{
  "checklist": "Oven Temperature Checks",
  "section": "Temperature Stability",
  "procedure": "Oven Temperature Test",
  "action": "Verify oven temperature stability",
  "spec": "180–190 °C",
  "instructions": "Preheat the oven ...",

  "instance_principal": "machine || model=Machine_A || serial=1234",

  "result": "",
  "status": "NA",
  "comment": "",

  "relationships": [
    {
      "kind": "template",    // "template" or "address"
      "predicate": "depends_on",
      "target_slug_id": "1234567890ABCDEF"   // for kind=template
    }
  ]
}
```

Behavior:

1. Canonicalize template fields and compute `slug_id` (Section 4.6.1).
2. Canonicalize `instance_principal` and compute `instance_id` (Section 4.6.2).
3. Compute `address_id = slug_id || instance_id`.
4. Insert into `slugs` if `address_id` does not exist; otherwise:
   - either return `CONFLICT`, or
   - treat as idempotent (deployment-specific, but MUST be documented).
5. Insert any requested relationships:
   - `kind=template` → `template_relationships`.
   - `kind=address`  → `address_relationships` (only allowed if `target_address_id` supplied instead of `target_slug_id`).
6. Set `timestamp` = current UTC.
7. Resolve caller’s `entity_id` and store it.

Response:

```json
{
  "ok": true,
  "data": {
    "address_id": "SLUGIDSLUGIDINSTIDINSTID",
    "slug_id": "SLUGIDSLUGIDSLUG",
    "instance_id": "INSTIDINSTIDINS",
    "timestamp": "2025-11-29T15:20:11Z",
    "entity_id": "ABCDEFGHJKMNPQRS"
  },
  "warnings": []
}
```

#### 7.4.3 GET `/api/v1/slugs/{address_id}`

Fetch a single slug by address:

```json
{
  "ok": true,
  "data": {
    "address_id": "...",
    "slug_id": "...",
    "instance_id": "...",
    "checklist": "...",
    "section": "...",
    "procedure": "...",
    "action": "...",
    "spec": "...",
    "instructions": "...",
    "result": "...",
    "status": "NA",
    "comment": "",
    "timestamp": "2025-11-29T15:20:11Z",
    "entity_id": "ABCDEFGHJKMNPQRS"
  },
  "warnings": []
}
```

- `NOT_FOUND` if unknown.

#### 7.4.4 GET `/api/v1/slugs`

List slugs with filters:

Supported query parameters (non-exhaustive):

- `checklist` (exact match)
- `section` (exact match)
- `status` (`Pass`, `Fail`, `NA`, `Other`)
- `slug_id`
- `instance_id`
- `limit`, `cursor`

Response:

```json
{
  "ok": true,
  "data": {
    "items": [ { ...slug... } ],
    "next_cursor": "..."
  },
  "warnings": []
}
```

#### 7.4.5 PATCH `/api/v1/slugs/{address_id}` (Minimal Update Contract)

Implements the minimal update contract (Section 10.1–10.4).

Request:

```json
{
  "result": "182 °C",
  "status": "Pass",
  "comment": "Stabilized after 12 min"
}
```

Behavior:

1. Lookup slug by `address_id`. If missing → `NOT_FOUND`.
2. Update the provided fields among:
   - `result`
   - `status`
   - `comment`
3. Regenerate `timestamp` (UTC).
4. Resolve `entity_id` from the caller.
5. Write updated row into `slugs`.
6. Append snapshot to `history` (if enabled).
7. Optionally run evaluation (read-only; see 7.7.1) and attach warnings.

Response:

```json
{
  "ok": true,
  "data": {
    "address_id": "SLUGIDSLUGIDINSTIDINSTID",
    "updated_fields": ["result","status","comment"],
    "timestamp": "2025-11-29T15:24:03Z",
    "entity_id": "ABCDEFGHJKMNPQRS"
  },
  "warnings": []
}
```

- Attempts to update addressing fields (`checklist`, `section`, etc.) MUST
  yield `INVALID_FIELD`.

#### 7.4.6 POST `/api/v1/slugs/bulk-update`

Bulk minimal updates:

```json
{
  "updates": [
    {
      "address_id": "SLUG...INST...",
      "status": "Pass",
      "result": "OK"
    },
    {
      "address_id": "SLUG...INST...",
      "status": "Fail",
      "comment": "Leak detected"
    }
  ]
}
```

Response:

```json
{
  "ok": true,
  "data": {
    "results": [
      {
        "address_id": "SLUG...INST...",
        "ok": true,
        "updated_fields": ["status","result"],
        "timestamp": "2025-11-29T15:24:03Z",
        "entity_id": "ABCDEFGHJKMNPQRS",
        "warnings": []
      },
      {
        "address_id": "SLUG...INST...",
        "ok": false,
        "error": {
          "code": "NOT_FOUND",
          "message": "Slug not found",
          "details": { "address_id": "..." }
        }
      }
    ]
  },
  "warnings": []
}
```

This endpoint MUST treat each update independently; partial failure is allowed.

### 7.5 REST API: Relationships

Both template and address-level relationships are available via REST for simple clients. Graph and evaluation operations SHOULD prefer JSON-RPC (Section 7.8).

#### 7.5.1 POST `/api/v1/relationships/template`

Create a template-level relationship triple.

Request:

```json
{
  "subject_slug_id": "1234567890ABCDEF",
  "predicate": "depends_on",
  "target_slug_id": "234567890ABCDEF1"
}
```

Behavior:

- Validate:
  - `subject_slug_id` and `target_slug_id` format and existence.
  - `predicate` lower-case token (Section 11.6).
- Insert row into `template_relationships`.
- If `predicate` not in `predicates` table or marked deprecated, add a
  `warnings` entry; ingestion MUST still succeed.

Response:

```json
{
  "ok": true,
  "data": {
    "subject_slug_id": "1234567890ABCDEF",
    "predicate": "depends_on",
    "target_slug_id": "234567890ABCDEF1"
  },
  "warnings": [
    {
      "code": "UNKNOWN_PREDICATE",
      "message": "Predicate 'depends_on' not registered in predicates table",
      "details": { "predicate": "depends_on" }
    }
  ]
}
```

#### 7.5.2 POST `/api/v1/relationships/address`

Create an address-level relationship triple (optional feature).

Request:

```json
{
  "subject_address_id": "SLUGA...INSTA...",
  "predicate": "depends_on",
  "target_address_id": "SLUGB...INSTB..."
}
```

Behavior:

- Validate address IDs, existence of both slugs, and predicate token.
- Insert into `address_relationships`.
- Same warning behavior for unknown/extension predicates as 7.5.1.

#### 7.5.3 GET `/api/v1/relationships/template`

List template-level relationships with optional filters:

Query parameters:

- `subject_slug_id`
- `target_slug_id`
- `predicate`
- `limit`, `cursor`

Response:

```json
{
  "ok": true,
  "data": {
    "items": [
      {
        "subject_slug_id": "1234567890ABCDEF",
        "predicate": "depends_on",
        "target_slug_id": "234567890ABCDEF1"
      }
    ],
    "next_cursor": null
  },
  "warnings": []
}
```

#### 7.5.4 GET `/api/v1/relationships/address`

List address-level relationships:

Query parameters:

- `subject_address_id`
- `target_address_id`
- `predicate`
- `limit`, `cursor`

Response analogous to template relationships.

#### 7.5.5 GET `/api/v1/relationships/for/{address_id}`

Combined view for a specific address:

```json
{
  "ok": true,
  "data": {
    "address_id": "SLUG...INST...",
    "template_outgoing": [
      { "predicate": "depends_on", "target_slug_id": "..." }
    ],
    "template_incoming": [
      { "predicate": "fulfills", "source_slug_id": "..." }
    ],
    "address_outgoing": [
      { "predicate": "depends_on", "target_address_id": "..." }
    ],
    "address_incoming": [
      { "predicate": "paired_with", "source_address_id": "..." }
    ]
  },
  "warnings": []
}
```

This endpoint MUST be read-only.

### 7.6 REST API: History and Instance Catalog

#### 7.6.1 GET `/api/v1/history/{address_id}`

Return snapshots for a given address:

Query parameters:

- `since` (ISO8601, optional)
- `until` (ISO8601, optional)
- `limit`, `cursor`

Response:

```json
{
  "ok": true,
  "data": {
    "items": [
      {
        "address_id": "SLUG...INST...",
        "timestamp": "2025-11-29T15:20:11Z",
        "result": "182 °C",
        "status": "Pass",
        "comment": "",
        "entity_id": "ABCDEFGHJKMNPQRS"
      }
    ],
    "next_cursor": null
  },
  "warnings": []
}
```

#### 7.6.2 GET `/api/v1/instances/{instance_id}` (Optional)

If `instance_catalog` is enabled, allow inspecting an instance:

```json
{
  "ok": true,
  "data": {
    "instance_id": "INSTIDINSTIDINS",
    "principal": "machine || model=Machine_A || serial=1234",
    "label": "SEM-3",
    "meta": {
      "room": "201",
      "building": "Lab"
    }
  },
  "warnings": []
}
```

#### 7.6.3 GET `/api/v1/instances`

List instances (if catalog enabled), with filters:

- `label` (substring match, deployment-defined)
- `limit`, `cursor`

### 7.7 REST API: Evaluation (Read-Only)

Evaluation endpoints are **strictly read-only** and MUST NOT perform database
writes. They interpret graph and state according to the state machine (Section 9).

#### 7.7.1 POST `/api/v1/evaluate/slug`

Evaluate the dependency/roll-up context for one slug.

Request:

```json
{
  "address_id": "SLUG...INST..."
}
```

Response:

```json
{
  "ok": true,
  "data": {
    "address_id": "SLUG...INST...",
    "status": "Pass",                 // current stored status
    "effective_status": "Pass",       // status after considering relationships
    "flags": [
      "all_dependencies_passed"
    ],
    "dependency_summary": {
      "depends_on": [
        {
          "address_id": "SLUGB...INST...",
          "status": "Pass",
          "effective_status": "Pass"
        }
      ]
    },
    "rollup_summary": {
      "fulfills": [
        {
          "target_address_id": "SLUGC...INST...",
          "contribution": "Pass"
        }
      ]
    }
  },
  "warnings": []
}
```

Notes:

- `effective_status` is NON-authoritative; it is an evaluation result, not a
  stored value.
- Clients MAY choose to update slugs using the minimal update contract based
  on evaluation results, but the server MUST NOT update them automatically
  through this endpoint.

#### 7.7.2 POST `/api/v1/evaluate/graph`

Evaluate a small graph around given addresses.

Request:

```json
{
  "root_address_ids": [
    "SLUGA...INSTA...",
    "SLUGB...INSTB..."
  ],
  "max_depth": 3,
  "include_incoming": true,
  "include_outgoing": true
}
```

Response:

```json
{
  "ok": true,
  "data": {
    "nodes": [
      {
        "address_id": "SLUGA...INSTA...",
        "status": "Pass",
        "effective_status": "Pass",
        "flags": [],
        "warnings": []
      }
    ],
    "edges": [
      {
        "subject_address_id": "SLUGA...INSTA...",
        "predicate": "depends_on",
        "target_address_id": "SLUGB...INSTB..."
      }
    ]
  },
  "warnings": []
}
```

This endpoint is intended for visualization, diagnostics, and agent reasoning.

### 7.8 JSON-RPC API

JSON-RPC is used for:

- Batch operations (slugs, relationships, ingestion results).
- Graph/evaluation operations.
- Advanced queries that would otherwise need multiple REST calls.

#### 7.8.1 JSON-RPC Envelope

Requests:

```json
{
  "jsonrpc": "2.0",
  "method": "slug.bulk_upsert",
  "params": { },
  "id": "request-123"
}
```

Responses:

```json
{
  "jsonrpc": "2.0",
  "id": "request-123",
  "result": {
    "ok": true,
    "data": { },
    "warnings": []
  }
}
```

Errors:

```json
{
  "jsonrpc": "2.0",
  "id": "request-123",
  "error": {
    "code": -32000,
    "message": "Application error",
    "data": {
      "ok": false,
      "error": {
        "code": "INVALID_FIELD",
        "message": "Status must be Pass, Fail, NA, or Other",
        "details": { "field": "status" }
      }
    }
  }
}
```

The APIM error envelope is nested in `error.data`.

#### 7.8.2 `slug.bulk_upsert`

Batch creation/update of slugs using full or minimal contracts.

Parameters:

```json
{
  "items": [
    {
      "mode": "create",   // or "update"
      "payload": {
        "checklist": "...",
        "section": "...",
        "procedure": "...",
        "action": "...",
        "spec": "...",
        "instructions": "...",
        "instance_principal": "...",
        "result": "",
        "status": "NA",
        "comment": "",
        "relationships": []
      }
    },
    {
      "mode": "update",
      "payload": {
        "address_id": "SLUG...INST...",
        "status": "Pass",
        "result": "OK"
      }
    }
  ]
}
```

Result:

```json
{
  "ok": true,
  "data": {
    "results": [
      {
        "mode": "create",
        "ok": true,
        "address_id": "SLUG...INST...",
        "timestamp": "2025-11-29T15:30:00Z",
        "entity_id": "ABCDEFGHJKMNPQRS",
        "warnings": []
      },
      {
        "mode": "update",
        "ok": false,
        "error": {
          "code": "NOT_FOUND",
          "message": "Slug not found",
          "details": { "address_id": "..." }
        }
      }
    ]
  },
  "warnings": []
}
```

#### 7.8.3 `relationship.bulk_upsert`

Batch upsert for template/address relationships.

Parameters:

```json
{
  "items": [
    {
      "kind": "template",
      "subject_slug_id": "1234567890ABCDEF",
      "predicate": "depends_on",
      "target_slug_id": "234567890ABCDEF1"
    },
    {
      "kind": "address",
      "subject_address_id": "SLUGA...INSTA...",
      "predicate": "paired_with",
      "target_address_id": "SLUGB...INSTB..."
    }
  ]
}
```

Result:

```json
{
  "ok": true,
  "data": {
    "results": [
      { "ok": true, "kind": "template" },
      { "ok": true, "kind": "address" }
    ]
  },
  "warnings": []
}
```

#### 7.8.4 `graph.evaluate`

Graph evaluation equivalent to `/api/v1/evaluate/graph`, but optimized for
agent use.

Parameters:

```json
{
  "root_address_ids": ["SLUGA...INSTA..."],
  "max_depth": 3,
  "include_incoming": true,
  "include_outgoing": true
}
```

Result uses the same shape as 7.7.2, inside the standard envelope.

#### 7.8.5 `ingest.apply`

Optional method for applying **pre-parsed** ingest batches (e.g., a Markdown
ingestor that already computed slugs on the client, or that runs in a
separate “ingest” container).

Parameters:

```json
{
  "source": "markdown",
  "items": [
    {
      "checklist": "...",
      "section": "...",
      "procedure": "...",
      "action": "...",
      "spec": "...",
      "instructions": "...",
      "instance_principal": "...",
      "relationships": [ ... ]
    }
  ]
}
```

Semantics:

- MUST use the same rules as `POST /api/v1/slugs` and relationship endpoints.
- MUST NOT bypass the slug/relationship validation pipeline.
- Intended for batch use only; simple clients SHOULD use REST.

### 7.9 MCP Tools

MCP tools are logical wrappers around the same runtime operations. They expose an LLM/agent-friendly schema over the same semantics.

#### 7.9.1 `apim.get_slug`

Input:

```json
{
  "address_id": "SLUG...INST..."
}
```

Output: full slug object as in 7.4.1 (within MCP’s own envelope).

#### 7.9.2 `apim.list_slugs`

Input:

```json
{
  "checklist": "Oven Temperature Checks",
  "section": "Temperature Stability",
  "status": "NA",
  "limit": 50
}
```

Output: list of slugs plus `next_cursor` if applicable.

#### 7.9.3 `apim.update_slug`

Input (minimal update contract):

```json
{
  "address_id": "SLUG...INST...",
  "result": "182 °C",
  "status": "Pass",
  "comment": "Stabilized after 12 min"
}
```

Output:

```json
{
  "ok": true,
  "address_id": "SLUG...INST...",
  "timestamp": "2025-11-29T15:24:03Z",
  "entity_id": "ABCDEFGHJKMNPQRS",
  "warnings": []
}
```

#### 7.9.4 `apim.create_slug`

Input (full creation contract):

```json
{
  "checklist": "Oven Temperature Checks",
  "section": "Temperature Stability",
  "procedure": "Oven Temperature Test",
  "action": "Verify oven temperature stability",
  "spec": "180–190 °C",
  "instructions": "Preheat the oven ...",
  "instance_principal": "machine || model=Machine_A || serial=1234",
  "relationships": []
}
```

Output:

```json
{
  "ok": true,
  "address_id": "SLUG...INST...",
  "slug_id": "SLUGIDSLUGIDSLUG",
  "instance_id": "INSTIDINSTIDINS",
  "timestamp": "2025-11-29T15:20:11Z",
  "entity_id": "ABCDEFGHJKMNPQRS",
  "warnings": []
}
```

#### 7.9.5 `apim.relationships`

Input:

```json
{
  "address_id": "SLUG...INST..."
}
```

Output: combined relationship view as in 7.5.5.

#### 7.9.6 `apim.evaluate_slug` / `apim.evaluate_graph`

MCP versions of 7.7.1 and 7.7.2. MUST be read-only and MUST not update stored
status or other fields.

### 7.10 Error Codes and Warnings

Common `error.code` values:

- `INVALID_FIELD`
- `MISSING_FIELD`
- `NOT_FOUND`
- `CONFLICT`
- `UNAUTHORIZED`
- `FORBIDDEN`
- `RATE_LIMITED`
- `INTERNAL_ERROR`

Warnings (non-fatal) MAY use:

- `UNKNOWN_PREDICATE`
- `DEPRECATED_PREDICATE`
- `MISSING_RELATIONSHIP_TARGET`
- `CYCLE_DETECTED`
- `INDETERMINATE_STATUS`

Warnings MUST never change database state by themselves; they are advisory.

### 7.11 Invariants

The transport layer MUST uphold:

- **Identity immutability**:
  - Addressing fields cannot be changed in place.
  - New addressing → new slug via full creation contract.
- **Minimal update scope**:
  - Only `result`, `status`, `comment` are writable via minimal update.
  - `timestamp` and `entity_id` are regenerated server-side.
- **Single authority**:
  - All mutations go through REST or JSON-RPC, including server-internal
    automation.
- **Read-only evaluation**:
  - Evaluation endpoints never write; all state changes are explicit updates.
- **Determinism**:
  - Identical inputs (template fields, principals) produce identical IDs and
    results across deployments.

This completes the canonical API, JSON-RPC, and MCP surface for APIM.

## 8. Relationship Model

The APIM relationship model defines how checklist slugs refer to each other and
how these references are stored, transported, and evaluated.

Relationships are:

- Authored in Markdown at the **template** level.
- Stored in SQLite as normalized triples.
- Exposed over REST, JSON-RPC, and MCP.
- Interpreted by the state machine (Section 9) for dependency and roll-up logic.

The model distinguishes:

- **Template-level relationships**: between `slug_id`s.
- **Address-level relationships** (optional): between `address_id`s.
- **Predicates**: the vocabulary that defines what a relationship means.

### 8.1 Goals

The relationship model is designed to:

- Make dependencies and roll-ups explicit and machine-readable.
- Preserve portability across deployments by modeling relationships at the
  template level.
- Allow deployments to add **address-specific overlays** without changing
  template semantics.
- Support a small set of **standard predicates** with defined system behavior,
  while still allowing free-text/extension predicates without hard failure.
- Keep the server core simple: storage and evaluation are well-defined; any
  additional automation remains a client/extension concern using the API.

### 8.2 Conceptual Graph

At the conceptual level APIM maintains two related directed graphs:

1. **Template graph** (mandatory):

   Nodes:
   - `slug_id` (template identity for checklist rows).

   Edges:
   - Triples of the form:

     ```text
     (subject_slug_id, predicate, target_slug_id)
     ```

   This graph is **portable** across deployments and instances.

2. **Address graph** (optional overlay):

   Nodes:
   - `address_id` (composite of `slug_id || instance_id`).

   Edges:
   - Triples:

     ```text
     (subject_address_id, predicate, target_address_id)
     ```

   This graph is **deployment-local** and may encode cross-instance semantics
   or special wiring between particular copies of procedures.

Both graphs:

- Are directed multigraphs (multiple edges between same nodes allowed).
- May contain cycles (Section 9 describes how evaluation handles them).
- Use the same predicate vocabulary.

### 8.3 Template Relationships

Template relationships are the primary, canonical mechanism for expressing
how procedures relate logically at design time.

#### 8.3.1 Definition

A template relationship is a triple:

```text
(subject_slug_id, predicate, target_slug_id)
```

Where:

- `subject_slug_id` is the `slug_id` of the row that **declares** the relationship.
- `predicate` is a lowercase ASCII token (Section 11.6).
- `target_slug_id` is the `slug_id` being referenced.

Semantics:

- They describe the **intended structural and logical relationships** between
  procedures, independent of any specific instance.
- At runtime they are lifted into instance contexts by pairing each `slug_id`
  with one or more `instance_id`s.

For a given instance `INST_X`, the conceptual evaluation concerns:

```text
(SUBJECT_SLUG_ID, INST_X) --predicate--> (TARGET_SLUG_ID, INST_X)
```

#### 8.3.2 Storage

Template relationships are stored in a dedicated table:

```sql
CREATE TABLE template_relationships (
    subject_slug_id  TEXT NOT NULL,
    predicate        TEXT NOT NULL,
    target_slug_id   TEXT NOT NULL,

    -- optional FK constraints if desired:
    -- FOREIGN KEY(subject_slug_id) REFERENCES slugs(slug_id),
    -- FOREIGN KEY(target_slug_id)  REFERENCES slugs(slug_id)
);
```

Recommended indexes:

```sql
CREATE INDEX idx_template_rels_subject  ON template_relationships(subject_slug_id);
CREATE INDEX idx_template_rels_target   ON template_relationships(target_slug_id);
CREATE INDEX idx_template_rels_predicate ON template_relationships(predicate);
```

The `slugs` table remains keyed by `address_id`; `slug_id` is redundant but
indexed for joining with `template_relationships`.

#### 8.3.3 Authoring in Markdown

Template relationships are authored using `slug_id` targets, as defined in
Section 5.7:

```markdown
#### Relationships
- depends_on ABCDEFGHJKMNPQRS
- fulfills  Z234567Z234567Z2
```

Rules:

- Each bullet line under `#### Relationships` describes **one template
  relationship**.
- Format:

  ```text
  <predicate> <target_slug_id>
  ```

- `predicate` MUST be lowercase `[a-z0-9_]+`, starting with a letter.
- `target_slug_id` MUST be a valid 16-character Crockford Base32 ID.

On ingest:

- `subject_slug_id` is the computed `slug_id` for the current procedure row.
- Each bullet becomes one row in `template_relationships`.

Malformed lines:

- MUST be ignored with a warning, without failing the entire file.
- SHOULD be surfaced to authors (e.g., via logs or UI).

### 8.4 Address-Level Relationships (Optional Overlay)

Address-level relationships describe relationships between **specific
instances** of procedures—i.e., between specific `address_id`s.

#### 8.4.1 Definition

An address-level relationship triple is:

```text
(subject_address_id, predicate, target_address_id)
```

Where:

- `subject_address_id` = `subject_slug_id || subject_instance_id`.
- `target_address_id`  = `target_slug_id  || target_instance_id`.

Semantics:

- They **overlay** the template graph; they do not replace it.
- They are used for deployment-local wiring such as:
  - pairing instances across rooms or systems,
  - expressing that one machine’s check depends on a **different** machine’s
    check,
  - modeling cross-instance interactions not encoded at the template level.

Examples:

- “Instance A of Cube must be paired with Instance B of Cube before step Y
  passes.”
- “Room 201’s HVAC verification depends on Room 101’s main plant state.”

#### 8.4.2 Storage

Address-level relationships use a separate table:

```sql
CREATE TABLE address_relationships (
    subject_address_id  TEXT NOT NULL,
    predicate           TEXT NOT NULL,
    target_address_id   TEXT NOT NULL,

    FOREIGN KEY(subject_address_id) REFERENCES slugs(address_id),
    FOREIGN KEY(target_address_id)  REFERENCES slugs(address_id)
);
```

Recommended indexes:

```sql
CREATE INDEX idx_address_rels_subject  ON address_relationships(subject_address_id);
CREATE INDEX idx_address_rels_target   ON address_relationships(target_address_id);
CREATE INDEX idx_address_rels_predicate ON address_relationships(predicate);
```

This separation ensures:

- Template semantics remain independent of specific deployments.
- Address-level overlays are easily discoverable and can be enabled/disabled,
  migrated, or pruned deployment-by-deployment.

#### 8.4.3 Creation and Management

Address relationships are typically created via:

- REST: `POST /api/v1/relationships/address`
- JSON-RPC: `relationship.bulk_upsert` with `kind="address"`
- MCP: tools that call the same backend functionality

They are **never** authored directly in the canonical Markdown templates. If
desired, a deployment MAY introduce local Markdown conventions (e.g., `@address`
lines) but those are out of scope for the core spec and MUST be treated as
deployment extensions.

### 8.5 Predicate Vocabulary

Predicates define what a relationship means. APIM supports:

- A small set of **standard predicates** with specified semantics.
- Arbitrary **extension predicates** that are accepted but not interpreted by
  the core state machine.

#### 8.5.1 Standard Predicates

The initial standard vocabulary includes:

- `depends_on`  
  Subject cannot be fully Pass unless all targets are Pass or NA. Used for
  prerequisites and ordering constraints. (Detailed behavior in Section 9.)

- `fulfills`  
  Subject contributes to a higher-level requirement represented by the target.
  Used for roll-ups and aggregation. Direction: subject → target.

- `satisfied_by`  
  Inverse of `fulfills`. Conceptually:

  ```text
  A satisfied_by B  ≡  B fulfills A
  ```

  The state machine normalizes these internally.

Deployments MAY add more standard predicates in future spec revisions.

#### 8.5.2 Extension Predicates

Any predicate matching `[a-z][a-z0-9_]*` is syntactically valid. Extension
predicates:

- MUST be accepted and stored.
- MAY produce a **warning** if not in the predicates catalog (Section 8.6).
- MUST NOT cause ingest or API calls to fail solely because they are unknown.
- Are treated as **opaque metadata** unless the deployment attaches specific
  logic to them.

This allows early drafts to use free-textish tokens (e.g. `suggests`,
`blocks`, `related_to`) without breaking the system, while giving a clear path
to standardization later.

### 8.6 Predicates Catalog (Optional but Recommended)

To support validation and gradual standardization, deployments MAY maintain a
`predicates` catalog table.

#### 8.6.1 Schema

Example schema:

```sql
CREATE TABLE predicates (
    predicate      TEXT PRIMARY KEY,          -- e.g. 'depends_on'
    kind           TEXT NOT NULL,            -- e.g. 'standard' | 'extension'
    direction      TEXT NOT NULL,            -- e.g. 'dependency' | 'rollup' | 'other'
    is_deprecated  INTEGER NOT NULL DEFAULT 0,
    notes          TEXT                      -- freeform explanation / spec reference
);
```

Usage:

- `kind="standard"` for predicates with defined semantics in Section 9.
- `kind="extension"` for predicates that exist but have no core behavior.
- `is_deprecated=1` for predicates that should not be used in new authoring, but
  must remain readable for existing data.

#### 8.6.2 Interaction With Ingest and API

When creating or ingesting relationships:

- If `predicate` exists in `predicates`:
  - No warning is required (unless deprecated).
  - If `is_deprecated=1`, ingest/API SHOULD add a warning like
    `DEPRECATED_PREDICATE` but still accept it.

- If `predicate` does **not** exist in `predicates`:
  - Relationship MUST still be stored.
  - A warning `UNKNOWN_PREDICATE` SHOULD be returned in the response.
  - The state machine treats that predicate as opaque and does not attach
    standard semantics.

This behavior implements **Option B** from earlier discussion: the system warns
but does not hard-fail on non-standard predicates.

### 8.7 Authoring vs Runtime Responsibilities

#### 8.7.1 Authoring (Markdown and Tools)

Authors are responsible for:

- Choosing meaningful predicates.
- Avoiding contradictory or confusing relationship definitions.
- Aligning relationships across checklists where necessary (e.g., shared
  dependencies, shared roll-ups).
- Gradually converging extension predicates into a curated set of standards.

Authoring tools can assist by:

- Autocompleting known predicates from the `predicates` catalog.
- Highlighting unknown or deprecated predicates.
- Suggesting alternative standardized predicates when available.

#### 8.7.2 Runtime (Server Core)

The server core is responsible for:

- Storing and retrieving template and address-level relationships.
- Enforcing syntactic constraints on predicates and IDs.
- Exposing relationships via REST, JSON-RPC, and MCP.
- Evaluating standard predicate semantics within the state machine (Section 9),
  **without** performing hidden writes.

The server core is explicitly **not** responsible for:

- Inferring new relationships.
- Rewriting predicates.
- Performing arbitrary graph transformations outside the defined evaluation
  rules.

#### 8.7.3 External Logic (Clients and Extensions)

More advanced relationship logic (e.g., domain-specific algorithms,
recommendation engines, statistical analysis) may live in:

- Client applications,
- background services,
- LLM/agent workflows.

These components:

- Pull relationships via the API.
- Evaluate or transform graphs according to their own rules.
- Write back **only** via the standard API contracts (slug creation, minimal
  updates, relationship upserts), preserving the core invariants.

### 8.8 Interaction With Evaluation and State Machine

The relationship graph is the input to the state machine (Section 9).

Key points:

- **Template relationships** are always in scope for evaluation; they define
  generic dependency and roll-up semantics that apply to every instance.
- **Address relationships** participate in evaluation only for the specific
  addresses they mention; they overlay the template semantics.
- **Standard predicates** (`depends_on`, `fulfills`, `satisfied_by`) have
  well-defined evaluation rules (dependency constraints, roll-ups).
- **Extension predicates** are visible in graph queries but are ignored by the
  core evaluation logic, unless a deployment explicitly extends the state
  machine.

The state machine is read-only. It:

- Computes `effective_status` (and flags) but does not mutate `status` or other
  fields directly.
- Returns evaluation results through the evaluation endpoints (Section 7.7) or
  JSON-RPC methods (Section 7.8.4).

Any actual change to slug state MUST occur via the minimal update contract or
full creation contract (Section 10), even when driven by evaluation results.

### 8.9 Examples

#### 8.9.1 Simple Dependency Chain

Markdown:

```markdown
### Procedure: Pre-check
- Action: Verify prerequisites
- Spec: All pre-checks complete
- Result:
- Status:
- Comment:

#### Instructions
Ensure all upstream checks are complete.

#### Relationships
- depends_on ABCDEFGHJKMNPQRS
```

Assuming `ABCDE...` is the slug_id of another row (“Valve Leak Test”), ingest
produces:

```text
(subject_slug_id = SLUG_PRECHECK,
 predicate        = "depends_on",
 target_slug_id   = "ABCDEFGHJKMNPQRS")
```

At runtime for instance `INST_X`:

```text
(SLUG_PRECHECK, INST_X) depends_on (ABCDEFGHJKMNPQRS, INST_X)
```

Evaluation:

- If `(ABCDEFGHJKMNPQRS, INST_X)` is Fail → Pre-check is constrained.

#### 8.9.2 Cross-Instance Pairing

Address-level relationship:

```json
{
  "subject_address_id": "SLUGA...INST_201...",
  "predicate": "paired_with",
  "target_address_id": "SLUGA...INST_101..."
}
```

This says: “The same procedure slug A in Room 201 is paired with slug A in
Room 101.” APIM stores this in `address_relationships` and exposes it for
evaluation and visualization.

If `paired_with` is not a standard predicate in `predicates`, the server
returns a `UNKNOWN_PREDICATE` warning but still stores and returns the edge.

#### 8.9.3 Extension Predicate

Markdown:

```markdown
#### Relationships
- related_to Z234567Z234567Z2
```

If `related_to` is not in the `predicates` catalog:

- Ingest stores the triple.
- Response (or logs) include `UNKNOWN_PREDICATE`.
- The state machine ignores `related_to` when computing
  `effective_status`, but tools can still use it for navigation or
  documentation.

---

The relationship model thus provides:

- A clear, portable template graph for standard dependency and roll-up logic.
- An optional address-level overlay for deployment-specific wiring.
- A predicate vocabulary that balances strict semantics for a small core set
  with flexible, non-fatal extension predicates for iterative refinement.

## 9. State Machine and Evaluation

The APIM state machine defines how slug state is **evaluated** based on
relationships and current fields. It is deliberately **read-only**:

- It never writes to `slugs`, `history`, or relationship tables by itself.
- All mutations still go through the API contracts in Section 10.
- It produces **derived evaluations and flags** that clients and automation
  can act on.

This keeps the server core small and predictable while still making
relationships operationally useful.

### 9.1 Goals

The state machine exists to:

- Interpret standard predicates (`depends_on`, `fulfills`, `satisfied_by`).
- Provide deterministic rules for combining multiple relationships.
- Surface constraints, cycles, and missing targets as explicit flags.
- Support both human and automated clients without hidden side effects.
- Keep evaluation logic close to the data (server-side), while allowing
  higher-level logic to live in external tools that call the API.

### 9.2 Scope and Non-Goals

**In scope:**

- Reading from:

  - `slugs` (current state).
  - `template_relationships`.
  - `address_relationships` (if used).
  - Optionally, `predicates`.

- Computing:

  - Per-slug `effective_status` (derived).
  - A minimal set of evaluation flags (constraints, cycles, unknown targets).
  - Simple roll-up summaries for standard predicates.

**Out of scope:**

- Writing to `slugs`, `history`, or relationship tables.
- Inferring new relationships or predicates.
- Modifying Markdown.
- Acting as a general workflow engine.
- Performing arbitrary domain-specific logic (that belongs in external tools).

Any change to stored state still uses:

- Minimal update contract (Section 10.1–10.6).
- Full slug creation contract (Section 10.8).

### 9.3 Evaluation Inputs and Outputs

#### 9.3.1 Inputs

For a given evaluation run, the state machine consumes:

- All or a subset of `slugs` (e.g., restricted by checklist or filters).
- All relevant entries in:

  - `template_relationships`.
  - `address_relationships` (if enabled).
  - `predicates` (optional catalog).

- Optional time context:

  - “Now” in UTC, for time-based rules (if configured).

#### 9.3.2 Outputs

For each slug (identified by `address_id`) the state machine returns:

- `address_id`
- A computed `effective_status`:

  - One of `Pass`, `Fail`, `NA`, `Other`, or `Indeterminate` (derived only;
    does not overwrite stored `status`).
- A list of `flags`, each an object with:

  - `code` (e.g. `BLOCKED_BY_DEPENDENCY`, `CYCLE_DEPENDS_ON`,
    `MISSING_TARGET`, `UNKNOWN_PREDICATE`).
  - Optional `details` (IDs or human-readable context).

Optionally, the evaluation may return:

- Aggregated roll-up information (e.g., children contributing to a parent).

### 9.4 Evaluation Unit and Terminology

The basic evaluation unit is a **slug instance** identified by:

```text
address_id = slug_id || instance_id
```

For each `address_id` we consider:

- Stored fields:

  - `status`, `result`, `comment`, `timestamp`, `entity_id`.

- Template-level relationships:

  - All triples with `subject_slug_id = this slug_id`.
  - Lifted to this instance via `instance_id`.

- Address-level relationships:

  - All triples with `subject_address_id = this address_id`.

The evaluation is purely functional: same inputs → same outputs.

### 9.5 Dependency Evaluation (`depends_on`)

`depends_on` expresses that a subject cannot be considered fully Pass unless
its targets are Pass or NA.

#### 9.5.1 Template-Level Dependencies

Given a template triple:

```text
(SUBJECT_SLUG, "depends_on", TARGET_SLUG)
```

For each instance `INST` we conceptually have:

```text
(SUBJECT_SLUG, INST) depends_on (TARGET_SLUG, INST)
```

The state machine looks up:

- Subject: `address_id = SUBJECT_SLUG || INST`.
- Target: `address_id = TARGET_SLUG || INST`.

If the target instance does not exist, a `MISSING_TARGET` flag is added.

#### 9.5.2 Address-Level Dependencies

Given an address triple:

```text
(SUBJECT_ADDR, "depends_on", TARGET_ADDR)
```

The state machine uses those exact `address_id`s, independent of
`instance_id` equality.

#### 9.5.3 Dependency Rules

For each subject–target pair:

- If target `status` ∈ `{Pass, NA}`:

  - No blocking constraint; dependency is considered satisfied.

- If target `status` ∈ `{Fail, Other}`:

  - Add a `BLOCKED_BY_DEPENDENCY` flag to subject.
  - Include the blocking `address_id` in `details`.

- If target has `status` = empty / null:

  - Add `UNRESOLVED_DEPENDENCY` flag to subject.

Dependency evaluation does **not** rewrite the subject’s stored `status`. It
only changes `effective_status` and flags.

#### 9.5.4 Effective Status from Dependencies

If **any** dependency is in a blocking state, the subject’s `effective_status`
is:

- `Indeterminate` if subject `status` is empty, or
- `Other` if subject `status` is non-empty but conflicts with dependencies.

If all dependencies are satisfied (or none exist), the subject’s
`effective_status` is initially set to its stored `status`, to be further
modified by roll-up logic (Section 9.6) and time-based rules (Section 9.8).

### 9.6 Roll-Up Evaluation (`fulfills` / `satisfied_by`)

Roll-up predicates connect “detail” procedures with higher-level
requirements.

#### 9.6.1 Normalization

For evaluation, the state machine normalizes:

```text
A satisfied_by B  ≡  B fulfills A
```

So all roll-up logic operates on a canonical direction:

```text
(B, "fulfills", A)
```

Where:

- B = child / contributor
- A = parent / roll-up target

Both template and address-level relationships are normalized this way.

#### 9.6.2 Aggregation Rules

For a given target slug (parent), consider the set of all contributors:

```text
contributors = { B1, B2, ..., Bn }
```

Each contributor has a stored `status` and local evaluation result from
dependencies and time-based rules.

Let `S` be the multiset of contributors’ **stored** `status` values:

- If `S` contains any `Fail`:

  - Parent gets a `ROLLUP_HAS_FAIL` flag.
  - Parent’s `effective_status` becomes `Fail` unless an explicit
    alternative policy is configured.

- Else if `S` contains at least one `Other`:

  - Parent gets `ROLLUP_HAS_OTHER`.
  - Parent’s `effective_status` becomes `Other`.

- Else if `S` is non-empty and all contributors are in `{Pass, NA}`:

  - Parent gets `ROLLUP_ALL_PASS_OR_NA`.
  - Parent’s `effective_status` becomes `Pass` if it was empty or NA.

- Else if `S` is empty (no contributors) or all are empty/null:

  - Parent gets `ROLLUP_NO_CONTRIBUTORS`.
  - Parent’s `effective_status` remains whatever it was, or `NA` if empty.

Roll-up evaluation never overwrites stored `status`; it only computes
`effective_status` and flags.

### 9.7 Unknown and Extension Predicates

Predicates not recognized as standard (`depends_on`, `fulfills`,
`satisfied_by`) are treated as **extension predicates**.

For each relationship with an extension predicate:

- The edge is still included in the graph.
- The state machine does **not** attach any built-in behavior to it.
- If `predicates` catalog exists and the predicate is not listed, a
  `UNKNOWN_PREDICATE` flag MAY be generated for the subject and/or recorded in
  evaluation diagnostics.

External tools can still use extension predicates for navigation, custom logic,
or visualization.

### 9.8 Optional Time-Based Rules

Some deployments may want time-aware evaluation (e.g., checks expire after a
certain period). Time-based logic is **optional** and must be configured
explicitly.

Examples (non-normative):

- If `timestamp` older than X days:

  - Add `STALE` flag.
  - Set `effective_status` to `Indeterminate`.

- If windowed tests are required (e.g., daily checks):

  - If no history entries in the last 24 hours → `DUE` flag.

Constraints:

- Time-based rules must be **pure evaluation**:

  - No auto-writing to `status`.
  - No implicit creation of new `history` rows.

- Any actual change (e.g., marking something Fail due to staleness) must be
  performed via API update calls initiated by a client or automation service.

### 9.9 Cycles and Conflicts

The relationship graph may contain cycles and conflicting relationships.
The state machine detects and flags them; it never attempts to “fix” them.

#### 9.9.1 `depends_on` Cycles

Examples:

```text
A depends_on B
B depends_on A
```

or longer chains:

```text
A depends_on B
B depends_on C
C depends_on A
```

Behavior:

- Each slug in the cycle receives a `CYCLE_DEPENDS_ON` flag.
- Their `effective_status` becomes `Indeterminate` unless explicitly set to
  Fail.
- Dependencies on those slugs by others will see them as “indeterminate
  dependencies” and can attach `UNRESOLVED_DEPENDENCY` or a more specific flag.

#### 9.9.2 Roll-Up Cycles (`fulfills` / `satisfied_by`)

Example:

```text
A fulfills B
B fulfills A
```

Behavior:

- Each slug in the roll-up cycle receives `CYCLE_ROLLUP` flag.
- Aggregated roll-up status for those slugs is treated as `Other` unless
  overridden by deployment-specific rules.

#### 9.9.3 Conflicting Graph Semantics

Examples:

- A slug both `depends_on` and `fulfills` the same target.
- Conflicting roll-up hierarchies across checklists.

In such cases:

- The state machine does not guess; it simply flags conflicts:

  - `CONFLICTING_RELATIONSHIPS`.
  - `AMBIGUOUS_ROLLUP`.

- Clients and authors can inspect relationships and decide how to resolve them
  structurally in Markdown.

### 9.10 Evaluation Interfaces

Evaluation is exposed via the same transport mechanisms as other operations.

#### 9.10.1 HTTP+JSON

Example endpoints (non-normative names):

- `POST /api/v1/evaluate/slugs`

  - Body may include filters:

    ```json
    {
      "checklist": "oven_check",
      "address_ids": ["AAA...", "BBB..."],
      "include_relationships": true
    }
    ```

  - Response:

    ```json
    {
      "results": [
        {
          "address_id": "AAA...",
          "stored_status": "Pass",
          "effective_status": "Indeterminate",
          "flags": [
            { "code": "BLOCKED_BY_DEPENDENCY", "details": { "blocking": "CCC..." } }
          ]
        }
      ]
    }
    ```

- Deployments may also expose checklist-wide or system-wide evaluation
  endpoints (e.g. `GET /api/v1/evaluate/checklist/<name>`).

#### 9.10.2 JSON-RPC

A JSON-RPC method like:

```json
{
  "method": "apim.evaluate",
  "params": {
    "checklist": "oven_check"
  },
  "id": 1
}
```

Returning the same shape as the REST example.

#### 9.10.3 MCP

An MCP tool, e.g. `apim.evaluate`, with schema:

- Inputs:

  - `checklist` (optional).
  - `address_ids` (optional list).
  - `include_relationships` (boolean).

- Outputs:

  - An array of evaluation results (`address_id`, statuses, flags).

This allows agents to:

- Fetch only the relevant slice of graph into context.
- Explain why a slug is blocked or indeterminate.
- Suggest or apply updates (via separate update/create tools) if authorized.

### 9.11 Interaction With Updates and History

The state machine is strictly **read-only** with respect to persistent tables.

- When an update is applied via API (Section 10):

  1. The server updates `slugs`.
  2. If enabled, a new `history` snapshot is appended.
  3. The state machine may be invoked to compute evaluation results, which are
     returned in the API response but not persisted.

- Evaluation results MAY be logged externally (e.g., application logs or
  client-side logs) but:

  - They are not required to be stored in SQLite.
  - They do not change canonical state by themselves.

This ensures that:

- `slugs` and `history` remain the single source of truth for stored state.
- Evaluation is easily recomputable as needed.

### 9.12 Extensibility and Versioning

Deployments may extend the state machine by:

- Adding new **standard predicates** in future spec versions, with defined
  semantics.
- Implementing additional evaluation passes that:

  - Read the same underlying tables.
  - Produce additional flags or derived metrics.
  - Do not bypass the API contracts for writes.

Any extension that **writes** to `slugs` or `history` must:

- Use the same minimal update or creation contracts as any other client.
- Be clearly documented as an external automation, not part of the core
  state machine.

This separation allows the core to remain stable and predictable, while still
supporting domain-specific behavior built on top of the same relationship
model and evaluation rules.

## 10. Write Contracts (Creation, Minimal Update, Batch Ingest)

APIM’s write model enforces strict, minimal, predictable update semantics.
All state changes—whether made by a human, script, automation service, or
agent—use **the same contracts**. This prevents “hidden state mutation” and
keeps the system auditable.

There are three write surfaces:

1. **Slug Creation Contract**  
2. **Minimal Update Contract**  
3. **Batch Ingest Contract** (Markdown, JSONL, operational data)

No other write pathways exist.  
REST, JSON-RPC, and MCP all dispatch to these contracts.

---

## 10.1 Principles

All write operations MUST obey these invariants:

**10.1.1 No mutation of addressing fields**  
Once created, the following properties of a slug are immutable:

- `slug_id`
- `instance_id`
- `address_id = slug_id || instance_id`
- `checklist_slug` (derived or explicit)
- All template-derived structural fields

**10.1.2 Updates are *minimal***  
A minimal update:

- Only changes the requested fields (`status`, `result`, `comment`).
- Does not modify other fields.
- Automatically appends a `timestamp` if provided or derived.
- Immediately writes a `history` record (if enabled).

**10.1.3 Creation is explicit**  
A slug must be fully created before it can be updated.  
Creation:

- Inserts a full row in `slugs`.
- Must provide all required fields.

**10.1.4 Idempotency**  
An update that repeats identical input MUST produce no additional changes
(other than history entry if required).  
Repeated create calls MUST fail unless explicitly allowed by ingest mode.

**10.1.5 Evaluation is not a write**  
Evaluation (Section 9) never performs creations or updates—it only computes
derived state.

---

## 10.2 Slug Creation Contract

Slug creation inserts a **new address_id** row into `slugs`.

### 10.2.1 Required Fields

Clients MUST provide:

- `slug_id` (template identity)
- `instance_id` (instance identity)
- `address_id` = computed concatenation
- `checklist_slug`
- `section`
- `procedure`
- `action`
- `spec`
- `instructions`

Clients MAY also provide initial:

- `status`
- `result`
- `comment`
- `entity_id` (who created)
- `timestamp`

### 10.2.2 REST Example

**Request:**

```json
{
  "slug_id": "ABCDEFGHJKMNPQRS",
  "instance_id": "2025-oven-01",
  "checklist_slug": "oven_startup",
  "section": "preflight",
  "procedure": "vent_check",
  "action": "verify_valve",
  "spec": "Valve is closed",
  "status": "",
  "result": "",
  "comment": ""
}
```

**Response:**

```json
{
  "ok": true,
  "data": {
    "address_id": "ABCDEFGHJKMNPQRS||2025-oven-01"
  },
  "warnings": []
}
```

### 10.2.3 Error Conditions

Creation MUST fail if:

- `address_id` already exists.
- `slug_id` malformed.
- `instance_id` malformed.
- Required fields missing.
- Invalid field formats.

---

## 10.3 Minimal Update Contract

The minimal update contract is one of the core invariants of APIM.

### 10.3.1 Updatable Fields

Only three fields are updatable:

- `status`
- `result`
- `comment`

The server MUST reject updates that attempt to change:

- `slug_id`
- `instance_id`
- `address_id`
- `checklist_slug`
- `section`, `procedure`, `action`, `spec`
- Any legacy/derived fields

### 10.3.2 Timestamp Behavior

- If client provides `timestamp`, it MUST be ISO 8601.
- If absent, server MUST generate a timestamp at update time.

### 10.3.3 History Behavior

If history is enabled:

- Every minimal update MUST append a new row in `history`.
- The history row captures:
  - `address_id`
  - `status`, `result`, `comment` (full snapshot)
  - `entity_id`
  - `timestamp` (snapshot time)
- No deltas; always full snapshots.

### 10.3.4 REST Example

**Request:**

```json
{
  "status": "Pass",
  "comment": "Valve verified closed"
}
```

**Response:**

```json
{
  "ok": true,
  "data": {
    "address_id": "ABCDEFGHJKMNPQRS||2025-oven-01",
    "stored_status": "Pass"
  },
  "warnings": []
}
```

### 10.3.5 JSON-RPC Example

```json
{
  "method": "apim.slug.update",
  "params": {
    "address_id": "ABCDEFGHJKMNPQRS||2025-oven-01",
    "status": "Fail",
    "comment": "Valve leaking"
  },
  "id": 12
}
```

---

## 10.4 Relationship Upsert Contract

Applies to:

- Template relationships
- Address relationships

### 10.4.1 Allowed Changes

Upserts may:

- Insert a new relationship triple.
- Replace a triple (if identical subject and predicate, but new target).
- Delete a relationship triple (via REST DELETE or JSON-RPC op).

### 10.4.2 Forbidden Changes

Cannot:

- Mutate `slug_id`, `address_id`, or any slug fields.
- Automatically infer relationships.
- Create slugs.

### 10.4.3 REST Example (Template Relationship)

**POST /api/v1/relationships/template**

```json
{
  "subject_slug_id": "ABCDEFGHJKMNPQRS",
  "predicate": "depends_on",
  "target_slug_id": "Z234567Z234567Z2"
}
```

### 10.4.4 Address Relationship Example

**POST /api/v1/relationships/address**

```json
{
  "subject_address_id": "SLUGA||INST1",
  "predicate": "paired_with",
  "target_address_id": "SLUGA||INST2"
}
```

Unknown predicates MUST be stored and produce a warning if not in catalog.

---

## 10.5 Slug Deletion Contract (Optional, Deployment-Specific)

APIM core does not require DELETE, but deployments MAY enable it under:

- Strict permissions.
- Soft-delete only (set `deleted_at`).
- Or full removal if history retention is not required.

If enabled:

- Deleting a slug MUST NOT cascade-delete relationships.
- Instead, relationships referencing it MUST remain but evaluation flags
  `MISSING_TARGET`.

---

## 10.6 Batch Minimal Update Contract (JSON-RPC)

Batch updates allow efficient automation.

### 10.6.1 Format

```json
{
  "method": "apim.batch.update",
  "params": {
    "updates": [
      {
        "address_id": "AAA||X",
        "status": "Pass"
      },
      {
        "address_id": "BBB||X",
        "status": "Fail",
        "comment": "Leak detected"
      }
    ]
  },
  "id": 99
}
```

Rules:

- Each update is validated independently.
- Batch MUST be atomic per-update, not per-batch. Failures for one update do
  not stop others from being applied.
- History rows must be inserted for each successful update.

---

## 10.7 Batch Ingest Contract (Markdown / JSONL)

Markdown ingest (templates) and JSONL ingest (state from external systems)
both must obey the same safety rules.

### 10.7.1 Markdown Ingest

Markdown → structured checklist template → API calls:

- Create missing slugs (creation contract).
- Upsert template relationships.
- Optional update of template metadata.

Markdown ingest MUST NOT:

- Directly modify database tables.
- Change existing slugs’ addressing fields.
- Overwrite stored `status`, `result`, or `comment`.

### 10.7.2 JSONL State Ingest

Each JSONL row represents a minimal update:

```jsonl
{"address_id":"AAA||X","status":"Pass","comment":"OK"}
{"address_id":"BBB||X","status":"Fail"}
```

Processing:

- For each row:
  - Validate fields.
  - Apply minimal update.
  - Append history (if enabled).

### 10.7.3 Ingest Mode Options

Deployments may offer an ingest mode:

- `strict`  
  Unknown predicates or malformed lines → hard fail.

- `permissive`  
  Unknown predicates allowed, warnings produced.

- `dry_run`  
  Validate but do not mutate.

---

## 10.8 Entity and Instance Creation Contracts

Entity creation and instance creation both use the **slug creation contract**.

### 10.8.1 Entity Principal Checklist

An entity (person, robot, system) is created by materializing an instance of
the **Entity Principal Template Checklist**.

This instance:

- Produces a deterministic `entity_id` from its fields.
- Uses the same minimal update/creation rules.
- Is stored in `slugs` and fully auditable.

The server MUST NOT have a separate privileged “create user” function.

### 10.8.2 Instance Principal Checklist

Instance creation for:

- Rooms,
- Physical assets,
- Machines,
- Scenarios,

also uses a dedicated template.

Every instance is created by:

1. Selecting appropriate template.
2. Filling required fields.
3. Producing deterministic `instance_id`.
4. Creating slugs for all template rows.

This ensures that instances and entities follow the same lifecycle.

---

## 10.9 Write Contract Error Model

Errors follow the unified envelope (Section 7.1.3).

Examples:

- `ADDRESS_ID_EXISTS`
- `INVALID_FIELD`
- `FORBIDDEN_MUTATION`
- `UNKNOWN_PREDICATE`
- `MISSING_REQUIRED_FIELD`
- `INVALID_TIMESTAMP_FORMAT`
- `RELATIONSHIP_CONFLICT`

---

## 10.10 Validation and Integrity Constraints

Before applying writes, server MUST:

- Validate IDs (Crockford Base32, lengths).
- Validate predicate syntax.
- Validate JSON field types.
- Reject structural mutations.

Server SHOULD:

- Warn on unknown predicates.
- Warn on deprecated predicates.
- Optionally warn on template/address relationship conflicts.

---

## 10.11 Interaction with State Machine

After any update or creation:

1. Database is mutated (minimal update or creation).
2. History snapshot is appended.
3. Evaluation MAY be invoked and returned in the API response.
4. Stored state remains unchanged except for the update itself.

Evaluation MUST NOT:

- Trigger additional updates.
- Create or delete slugs.
- Create relationships.

---

## 10.12 Summary of Contracts

| Operation                 | Allowed? | Fields Affected | History? | Notes |
|--------------------------|----------|-----------------|-----------|-------|
| Slug Creation           | Yes      | All required fields | Optional | Creates new address_id |
| Minimal Update          | Yes      | status/result/comment | Yes | No structural mutation |
| Relationship Upsert     | Yes      | relationship tables | No | Template and address both |
| Evaluate                | No write | None | No | Derived only |
| Delete Slug (optional) | Deployment | Soft-delete | Optional | No cascade |
| Batch Update           | Yes | status/result/comment | Yes | Atomic per-item |
| Markdown Ingest        | Yes | Creates slugs, upserts relationships | Maybe | Never overwrites stored status |
| JSONL Ingest           | Yes | status/result/comment | Yes | Minimal updates only |

---

The write contracts in this section guarantee:

- Consistency across all tools and clients.
- Predictable and auditable changes.
- No hidden logic or privileged mutation paths.
- Easy automation pipelines that rely on stable, minimal rules.

## 11. Identifier System (Canonical ID Model)

APIM uses a unified identifier system to ensure:

- Deterministic addressability of all checklist content.
- Stability of references across storage, transmission, and rendering.
- Collisions are extremely unlikely under normal deployment scales.
- Interoperability across REST, JSON-RPC, MCP, and external tools.
- Cross-deployment data exchange without ambiguity.

This section defines **all ID types**, their **formats**, and their **derivation rules**.

---

## 11.1 Overview of ID Types

APIM defines five major identifier classes:

1. **Checklist ID** — identifies a checklist template.
2. **Slug ID** — identifies a *template row* (section/procedure/action/spec).
3. **Instance ID** — identifies a specific *instantiation of a checklist*.
4. **Address ID** — identifies a specific slug *within* a specific instance.
5. **Entity ID** — identifies an agent (user, robot, system) performing updates.

Two auxiliary identifier domains:

- **Predicate tokens** (relationship vocabulary)
- **Checksum (optional)** for future extensions

The reference implementation uses **strict defaults** (Section 11.3) while
allowing flexibility for future versions or other deployments (Section 11.10).

---

## 11.2 Canonical Encoding Format

Unless otherwise stated, all IDs follow:

- **Crockford Base32**, uppercase:
  
  ```
  ABCDEFGHJKMNPQRSTVWXYZ23456789
  ```

- **Exclusions:**  
  Letters `I`, `L`, `O`, `U` MUST NOT appear in ID payloads or examples.  
  (These characters are excluded by the Base32 alphabet for visual safety.)

- **Padding:**  
  No `=` padding MUST be used.

- **Case:**  
  ID strings MUST be strictly uppercase.

- **Separator Rules:**  
  IDs themselves contain **no separators**.  
  Composite structures (e.g., address_id) define their own textual separators.

---

## 11.3 Reference Implementation Defaults (RDI-A)

The reference implementation included with APIM uses:

| ID Type       | Length | Checksum | Notes |
|---------------|--------|----------|-------|
| `slug_id`     | 16     | No       | Deterministic from template fields |
| `instance_id` | 16     | No       | Deterministic from instance template |
| `entity_id`   | 16     | No       | Deterministic from entity principal template + salt |
| `address_id`  | —      | —        | Concatenation: `slug_id || instance_id` |
| `checklist_id`| 16     | No       | Deterministic from checklist slug |

Separator for `address_id` (reference default):

```
address_id = slug_id || "||" || instance_id
```

This separator is chosen for readability and ease of parsing.

**Note:** Future revisions MAY adopt longer lengths, optional checksums, or
alternative separators. See Section 11.10.

---

## 11.4 Checklist ID

Each checklist template receives a stable ID derived from:

```
(checklist_slug)
```

### 11.4.1 Derivation

Procedure:

1. Normalize the checklist slug (lowercase ASCII, no spaces).
2. Hash with a deterministic algorithm (e.g., SHA-256).
3. Extract the required number of bits to produce a 16-character Base32 string.
4. Encode using Crockford Base32, uppercase.

This produces a stable `checklist_id`.

### 11.4.2 Constraints

- MUST be exactly 16 characters in the reference implementation.
- MUST NOT be regenerated if the file path changes.
- MAY be regenerated if the canonical checklist slug changes.

Checklist slug renaming is a template-level breaking change.

---

## 11.5 Slug ID

A `slug_id` uniquely identifies a *template row*.

### 11.5.1 Derivation Inputs

Slug ID is derived from the tuple:

```
(checklist_slug, section, procedure, action, spec)
```

All five fields MUST be present.

### 11.5.2 Derivation Procedure

1. Normalize each field:
   - Trim whitespace
   - Collapse internal whitespace
   - Convert to lowercase for hashing (the actual stored text remains original)
2. Concatenate fields with a delimiter known only to the hashing function.
3. Compute a deterministic hash (e.g., SHA-256).
4. Extract bits for a 16-character Base32 ID.
5. Encode uppercase.

### 11.5.3 Slug ID Immutability

- Any change to the defining fields MUST produce a new `slug_id`.
- Slug IDs MUST NOT be manually edited.

---

## 11.6 Instance ID

An `instance_id` designates a *specific instantiation of a checklist*.

For example:

- A room (Room 201)
- A microscope (ZEISS-FIB-01)
- A procedure run (“Oven Startup – Batch 2025-03-05”)

### 11.6.1 Derivation Inputs

Instance IDs are derived from the **Instance Principal Template fields**.

Examples:

- Room name
- Equipment serial
- Run date
- Configuration parameters

### 11.6.2 Derivation Procedure

Reference implementation:

1. Collect stable instance-identifying fields.
2. Normalize text (lowercase for hashing).
3. Hash deterministically (SHA-256).
4. Extract 16 Base32 characters.

### 11.6.3 Notes

- Deployments MAY add additional entropy or fields.
- Length MAY be extended in a future version.

---

## 11.7 Entity ID

Entity IDs identify users, robots, CI systems, or any agent applying updates.

### 11.7.1 Derivation Inputs

Entity IDs derive from:

```
(fields from Entity Principal Template Checklist)
+
(deployment_secret_salt)
```

Including a salt:

- Prevents collisions across deployments.
- Allows exporting slugs without exposing sensitive entity attributes.
- Keeps entity IDs stable within a deployment.

### 11.7.2 Derivation Procedure

1. Gather canonical entity template fields (username, role, etc.).
2. Normalize.
3. Combine with salt (deployment-specific secret).
4. Hash deterministically.
5. Extract 16 Base32 characters.

### 11.7.3 Notes

- Changing the salt regenerates all entity IDs (not recommended).
- SSO/OIDC/OAuth-derived attributes feed into the same template.

---

## 11.8 Address ID

The `address_id` identifies a specific slug *within* a specific instance.

### 11.8.1 Definition

```
address_id = slug_id || "||" || instance_id
```

The separator `"||"` is the reference implementation default.

### 11.8.2 Parsing

- Split at the first occurrence of `"||"`.
- Left side = `slug_id`
- Right side = `instance_id`

Because both slug_id and instance_id have fixed length in the reference implementation, parsing is unambiguous even without a separator; the separator is retained for readability.

---

## 11.9 Predicate Tokens

Predicates used in relationships (Section 8) use a distinct lexical domain.

### 11.9.1 Syntax

A valid predicate token:

```
[a-z][a-z0-9_]{0,63}
```

Rules:

- Lowercase only.
- Underscores allowed.
- Maximum length 64 characters.

### 11.9.2 Standard Predicate Set

Initial standard set:

- `depends_on`
- `fulfills`
- `satisfied_by`

### 11.9.3 Extensions

Unknown predicates:

- MUST be accepted.
- MUST NOT cause write failure.
- SHOULD generate a `UNKNOWN_PREDICATE` warning.

Predicates MAY be cataloged in the `predicates` table.

---

## 11.10 Optional Enhancements (Non-default, Implementation-Defined)

Future revisions or deployments MAY select different design choices.

These are explicitly left open to allow growth without breaking the spec.

### 11.10.1 Checksum (Optional)

Deployments MAY append a checksum character to:

- slug_id
- instance_id
- entity_id

Checksum MAY use:

- modulo arithmetic,
- BCH codes,
- or any error-detecting scheme.

Checksum MUST NOT break ID parsing rules.

### 11.10.2 ID Length Variants

Deployments MAY configure:

- 16 characters (default)
- 24 characters (larger scale)
- 32 characters (cryptographic-grade uniqueness)

Longer IDs reduce collision probability; short IDs increase readability.

### 11.10.3 Alternative Address Separators

These MAY be selected:

- `||` (default)
- `-` (filesystem-friendly)
- No separator (requires fixed-length parsing)

### 11.10.4 Custom Hash Algorithms

Deployments MAY replace SHA-256 with:

- BLAKE3  
- SHA3-256  
- SipHash (less preferred for security)

Hashes MUST be stable and deterministic.

All changes MUST be recorded in a deployment configuration file.

---

## 11.11 Collision Handling

Collisions are mathematically improbable with Base32-encoded 80-bit digests
(16 chars).

If detected:

- Server MUST reject creation of a duplicate ID with mismatched fields.
- Authors SHOULD modify fields minimally to break the collision.
- Deployments MAY increase ID length for next revision.

Collision detection is part of slug creation but not minimal update.

---

## 11.12 Validation Rules

ID validation MUST check:

- Correct length (per reference or configured policy)
- Allowed characters (Crockford Base32)
- Correct separator usage for `address_id`
- Predicate format (per Section 11.9)
- Non-null, non-empty values

Validation MUST occur on:

- Creation requests
- Update requests (address_id only)
- Relationship upserts
- Markdown ingest
- JSONL ingest

Validation MUST NOT reject unknown predicates.

---

## 11.13 Examples

### 11.13.1 Slug ID

Input fields:

```
(checklist_slug="oven_startup",
 section="preflight",
 procedure="vent_check",
 action="verify_valve",
 spec="Valve is closed")
```

Derived slug_id (example):

```
ABCD2345EFGH6789
```

### 11.13.2 Instance ID

Room example → deterministic:

```
ROOM_201    → 9X8Y7W6V5U4T3S2R
```

### 11.13.3 Address ID

```
ABCD2345EFGH6789||9X8Y7W6V5U4T3S2R
```

### 11.13.4 Entity ID

```
user=jds role=admin salt=SECRET
→ G7H8J9K1M2N3P4R5
```

---

## 11.14 Summary Table

| ID Type       | Scope | Stability | Default Length | Derived From |
|---------------|--------|-----------|----------------|--------------|
| checklist_id  | Template | High | 16 | checklist_slug |
| slug_id       | Template row | Absolute | 16 | section/procedure/action/spec |
| instance_id   | Runtime | High | 16 | instance template fields |
| entity_id     | Actor | High | 16 | entity template fields + salt |
| address_id    | Runtime | Derived | — | slug_id + instance_id |
| predicate     | Relationship | Free-form | — | lexical token |

---

## 11.15 Forward Compatibility

This section is expected to evolve in future versions:

- ID lengths may increase.
- Checksums may become standard.
- Entity and instance IDs may gain standardized field sets.
- Address separator rules may tighten for federation.

The core guarantees will remain:

- Determinism  
- Stability  
- Predictability  
- Cross-system portability  

---

## 12. Versioning, Template Evolution, and Compatibility

APIM is designed to support long-lived checklists, repeated over time, across
many deployments. This section defines how **versions** of templates,
instances, and identifiers behave under change.

Versioning rules ensure:

- Checklists can evolve without invalidating stored slugs.
- Instances from different eras remain evaluatable.
- Relationships remain stable even as templates change.
- Ingest and API actions remain predictable across revisions.
- A deployment can upgrade APIM itself without breaking stored data.

This section clarifies what may change, what must remain stable, and how the
system handles forward+backward compatibility.

---

## 12.1 Version Concepts

APIM defines four independent version dimensions:

1. **Template Version**  
   Tracks changes in Markdown checklist templates.

2. **Instance Version**  
   Tracks snapshots of generated instances (e.g., Room 201 template v3 vs v4).

3. **Data Model (Schema) Version**  
   Tracks structural changes in Section 6 tables.

4. **API Version**  
   Tracks REST + JSON-RPC interfaces (Section 7).

These dimensions evolve at different rates and are not tied to each other.

---

## 12.2 Template Version (Checklist Version)

A **template version** represents the evolution of a checklist authored in
Markdown. It is defined by changes that affect:

- the headings/structure,
- section/procedure/action/spec text,
- relationships,
- instructions or domains.

### 12.2.1 Template Version Field

Each Markdown file MAY include an optional field:

```yaml
version: 1.0.3
```

But APIM does not require this.  
Instead, APIM uses **slug_id regeneration** as the source of truth:

> If a template row changes in a way that affects hashing inputs  
> → **slug_id changes** → **this row is considered a new version**.

Thus:

- Template version is implicit, not explicit.
- APIM does not track “Template v1, v2, v3” internally.
- Instead, it tracks the **graph of slug_ids over time**.

### 12.2.2 Allowed and Disallowed Template Modifications

**Allowed without breaking instance data:**

- Reordering procedures
- Reformatting instructions (non-hash fields)
- Changing comments, notes, metadata sections
- Adding/remove relationships (safe; affects evaluation only)

**Changes that create new slug_ids (breaking changes):**

- Modifying section name  
- Modifying procedure name  
- Modifying action text  
- Modifying spec text  
- Renaming checklist slug

APIM treats these as creation of new procedural atoms.

---

## 12.3 Instance Version

An **instance** (e.g., specific machine, room, run) is tied to the template at
the moment of instantiation.

If a template is revised:

- Existing instances **do not** automatically migrate.
- New instances use new slug_ids.
- Old instances remain evaluatable because address_ids remain valid.

This is a deliberate design choice:

> Stored data is immutable.  
> Templates evolve.  
> Instances preserve the template shape they were created from.

### 12.3.1 Instance Migration (Optional)

Deployments MAY choose to create migration tools:

- Compare old vs new slug_ids.
- Map corresponding procedural atoms.
- Create updated addresses for new template.
- Migrate stored data selectively.

But APIM **does not** define automatic migration.

### 12.3.2 Instance “Epochs”

Deployments MAY define “epochs” or “cycles.”  
Example:

- Checklist v1 used in 2025.
- Checklist v2 used in 2026.

Instances from 2025 and 2026 will produce different slug_ids, but can still be
evaluated side-by-side.

---

## 12.4 Schema Version and Migration (Data Model Version)

Section 6 defines the recommended SQLite schema.

Deployments MAY evolve schema via:

- Adding new tables
- Adding new columns
- Adding indexes
- Adding history retention options

Deployments MUST NOT:

- Change ID derivation rules for previously created slugs.
- Modify existing slug rows to conform to new template versions.
- Rewrite address_id or entity_id formats retroactively.

### 12.4.1 Migrating APIM Schema

Schema migrations MUST:

- Preserve existing slugs and relationships.
- Preserve history.
- Leave address_id stable.

Schema migrations MAY:

- Add cache tables for evaluation.
- Add auxiliary metadata tables.
- Add indexes for performance.

---

## 12.5 API Version Compatibility

Section 7 defines API versioning rules.

### 12.5.1 Version Stability Guarantees

API behavior MUST NOT:

- Change minimal update contract semantics.
- Change slug creation semantics.
- Change relationship semantics.
- Change evaluation behavior for standard predicates.

These form the “frozen core” of APIM.

### 12.5.2 Backward Compatibility

A new API version (e.g., `/api/v2/`) MUST:

- Continue to accept v1 data structures and ID formats.
- Preserve deterministic hashing rules.
- Preserve evaluation correctness.

### 12.5.3 Forward Compatibility

Older clients MAY:

- Use v1 endpoints against a v2 server.
- Rely on the server to translate responses into stable v1 fields.
- Ignore new fields which v2 may introduce.

---

## 12.6 Evaluation Compatibility (State Machine Versioning)

State machine evolution must preserve:

- Interpretation of `depends_on`, `fulfills`, `satisfied_by`.
- Flag codes (Section 9).
- Non-mutation rule (evaluation is read-only).

Deployments MAY extend evaluation:

- Add new flags.
- Add new standard predicates in future revisions.
- Add optional time-based logic.

All extensions MUST:

- Preserve semantics of existing behavior.
- Remain deterministic.

---

## 12.7 Relationship Evolution and Cross-Version Safety

Relationships stored in templates (`template_relationships`) and overlays
(`address_relationships`) MUST remain stable through:

- Schema changes  
- Template evolution  
- API changes  

### 12.7.1 What Happens When a Target Slug ID Changes?

If template author modifies a row:

- A new slug_id will be generated.
- Old instances still point to old slug_id.
- New instances will use the new slug_id.

Evaluation across versions works because:

- Relationships remain defined for each era.
- Cross-instance comparisons rely on explicit address_id targets.

### 12.7.2 Handling Deprecated Predicates

Predicates may be deprecated in the `predicates` catalog.

Deprecated predicates:

- MUST NOT break ingest.
- MUST NOT break evaluation.
- MAY generate `DEPRECATED_PREDICATE` warnings.
- MAY be auto-normalized in future revisions.

---

## 12.8 Ingest, Export, and Version Boundaries

APIM’s ingest rules from Section 10 ensure version-safe behavior.

### 12.8.1 Markdown Ingest Across Versions

Markdown representing checklist v1 can be:

- Re-ingested later after template evolves (new slug_ids created).
- Compared to v2 to compute differences (tooling-defined).
- Merged with relationship graphs safely.

Markdown ingest MUST NOT:

- Rewrite stored slugs.
- Rewrite history.
- Rewrite existing relationships.

### 12.8.2 JSONL Ingest Across Versions

JSONL ingest operates only on:

- `address_id`
- Minimal update fields

Thus JSONL ingest is version-independent.

---

## 12.9 Cross-Deployment Federation

Multiple deployments may exchange:

- slugs
- relationships
- checklists
- entire graphs

To enable this:

- ID rules (Section 11) guarantee deterministic reconstruction.
- Relationship triples are version-resilient.
- Stored data is portable across deployments and eras.

Deployments MUST NOT assume:

- That slug_ids are identical across organizations (different salts/extensions).
- That entity_ids have shared meaning outside local context.

Address_ids remain safe for internal use but may not be portable unless
instance_id derivation rules are aligned.

---

## 12.10 Versioning Recommendations for Authors

Authors SHOULD:

- Minimize slug-breaking edits unless intentionally revising procedures.
- Prefer adding new steps over rewriting old ones.
- Maintain clear commit history in the checklist repository.
- Use relationships (`fulfills`) to express evolution rather than overwriting.

Authors SHOULD NOT:

- Reuse the same section/procedure names to mean different things across eras.
- Attempt to “force” old instances to map to new slug_ids manually.

---

## 12.11 Versioning Recommendations for Deployments

Deployments SHOULD:

- Store all template files (v1, v2, v3…) for provenance.
- Use JSONL export/import for safe instance migrations.
- Clearly annotate epoch boundaries when changing templates.
- Avoid rewriting slug_ids through manual DB changes.

Deployments MAY:

- Provide migration tooling upstream/downstream.
- Auto-generate “mapping tables” between template eras for reporting.

---

## 12.12 Versioning Recommendations for Implementers

Implementers SHOULD:

- Keep hashing rules stable per APIM version.
- Avoid changing Base32 encoders/decoders after production deployments.
- Store derived IDs (slug_id, instance_id, entity_id) permanently.
- Use database migrations for schema changes, never in-place rewrites.

Implementers MAY:

- Introduce optional configuration keys like:

  ```
  id_length
  use_checksum
  hash_algorithm
  separator_policy
  instance_id_fields
  entity_id_fields
  ```

- Offer compatibility layers for older ID policies.

---

## 12.13 Summary of Version Boundaries

| Component | May Change? | Impact | Notes |
|----------|-------------|--------|-------|
| Checklist Template | Yes | New slug_ids | Existing instances unaffected |
| Instance Template | Yes | New instance_ids | Older instances preserved |
| Slug Definition | Yes | Changes slug graph | Relationships version-aware |
| Schema | Yes | Additive only | No rewrite of slugs |
| API | Yes | Backward compatible | Core behavior frozen |
| Evaluation | Yes | Extensions only | Deterministic, non-mutating |
| Relationships | Yes | Version-aware | Cycles/conflicts flagged |
| IDs | No (existing) | Immutable | New policies must not break old IDs |

---

## 12.14 Conclusion

The versioning model ensures that APIM remains:

- Stable  
- Evolvable  
- Backward compatible  
- Forward compatible  
- Federated  
- Auditable  
- Deterministic  

Templates evolve.  
Instances persist.  
IDs remain stable.  
Evaluation remains valid across eras.  

This enables the APIM ecosystem to scale from a single machine to large,
multi-deployment, multi-year operational environments without loss of integrity.

## Appendix

### jds Ongoing considerations

- Section 9.3.2 we need to be able to set different methods of having the state machine cause interactions I am realizing. it should not always force an update to the state, sometimes it is just ar ecommendation sometimes it is setting the one to "other" and giving a coment' or a pass causes pass relationships perhapse it simply says pass and "set by state machine" in the comments; summary is there are more than one way to interpret relationships even if the state to state is clear; also entity id responsible for the update may be a client state machine handler so that it is clear what logic is processing the data
  - Right principle update needed, we are doing this because the server has the core logic and it would require updates directly to the server to handle logic if we were to do this that complicates the code significantly for the server and now it is client responsibility to match what the server does. It gives something stable to work on.
- When in section 8.4.3 it says "- MCP: tools that call the same backend functionality" I should clarify that MCP is simply a descriptive abstraction on top of the API; fundamentally API calls are still used the MCP just teaches the agent how to use them? (I am getting a feeling that it might be extra token filled vs using agent pre-defined structured logic to do so?) is it a more expensive type of API call for systems with a variable API set? perhapse then it applies less to this system where I anticipate stabilizing and keeping API options minimal for simplicity and that we already own more complicated systems that can store the data; we have an issue with certain frontend connectivity rather than backend archival capabilities. The backend archives suffer more because the frontend is not exposing the organization of the backend in a way that is ideal.
- 8.4.3 says address id are never authored in markdown. this makes it easy to lose relationships I fear.
	- gap, identify how clients can author markdown and display and provide consistent ways to draw up relationships.
- can this core state machine section 8.5 we have mentioned not allowing the machine to mutate its own state without API calls, so that we keep all interactions through the same pipeline rather than branching to multiple and confusing methods being allowed. But if running a server can it still localhost talk to itself in an efficient way or does it add a lot of overhead to have the API? I know loopback is efficient still, but its hosting a public server and I dont know how to talk locally to a public facing server? Though last week when booting up a caddy server I started the localhost server and then it exposed it to the internet.
- #### 9.5.3 Dependency Rules I need to parse these more
- ### 9.7 Unknown and Extension Predicates I think the examples in this section use known predicates to describe unknown predicates, thats weird
- in section 10 I had to add instructions. wait its missing in sections 9 and 10 it cant really be missing entirely from there? its a required field for a slug to get hm... something is missing about instructions I only added it once in ### 10.2.1 Required Fields it literally was missing the word `instructions` in that and that makes me worried what else is getting missed because of assumptions; keeping ambiguity present.
- notice how "## 11.1 Overview of ID Types APIM defines five major identifier classes: 1. **Checklist ID** — identifies a checklist template. 2. **Slug ID** — identifies a *template row* (section/procedure/action/spec)." is missing its instructions from being part of the row, pretty sure we are missing this or the GPT is getting context ever so slightly confused because its been a long conversation, one more section to go. I think I am better off finishing this revision and making another pass.
- notice how we dropped our third level headers and somehow sections started having their sub numbering at the same heading level as the ... its starting to break down. hopefully we still get good output for one last section before context lock happens.