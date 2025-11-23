# APIM Principles: Action vs. Procedure

## Why this distinction exists

- Planning (nouns, procedures): Work Breakdown Structures lean on nouns to anchor deliverables (example: "Switch bring-up" as a procedure).
- Execution (verbs, actions): Field work is verb-driven. Operators need to see the action to take ("Verify uplink"), the spec they are targeting, and where to report mutable fields (result/status/comment).
- Bridge the gap: APIM keeps procedures for structure and identity, but surfaces actions front-and-center so operators execute without reinterpreting planning nouns.

## How the Checklist Portal applies it

- Action-first rows: Each slug shows the action text; selection carries the procedure/Address IDentity implicitly.
- Spec stays adjacent: Action and spec sit together so the target is visible without opening edit forms.
- Inline mutable fields: Result, Status, and Comment are editable in-row with minimal friction for updates.
- Compact column order: `Action | Spec | Result | Status | Comment | Ind` with tight typography for the spec column.
- Shared surface: The human portal mirrors the MCP/HTTP contract; no human-only shortcuts.

## Relationship to the spec

- Addressing fields (checklist/section/procedure/action/spec) define identity and Address IDs; they stay immutable in place.
- Mutable fields (result/status/comment/timestamp) update through the minimal contract used by every client.
- Relationships (predicate + target_id) flow through the same API so web/MCP/CLI stay consistent.

## Goals for field users

- Execution is obvious: show the action verb to perform and the spec to hit.
- Planning nouns remain available in details without getting in the way of acting.
- Every UI affordance maps to an API call, keeping agents and humans aligned.

