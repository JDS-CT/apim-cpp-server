# TODO

- p3: Checklist portal shell (triggered by "I am a human"): implement the Checklist Portal view with dropdown + rail chips, using API checklist data; keep placeholder links beyond the first portal until provided.
- p3: Action-first table layout: single-row per slug showing Action + [details], Spec, Result, Status, Comment, Ind; no action/spec editing; compact spec typography to fit one row; indicator column labeled "Ind".
- p3: Detail expander: render instructions and relationships; keep result/status/comment editable inline per spec while preserving a narrow row layout.
- p3: Relationships edit UX: design an edit mode in the details pane to add/remove outgoing edges (predicate + target Address ID) using existing API; plan validation before coding.
- p3: Column order: enforce Action | Spec | Result | Status | Comment | Ind in the human portal table.
- p3: Exports panel: keep JSONL + Markdown, drop JSON; add error feedback; validate Markdown server-side before DB writes; consider an HTML form that generates sanitized Markdown via the validator.
- p3: Checklist author portal idea: guided form that posts slug fields via existing API/Markdown import for consistency across clients.
- p3: Accessibility: keyboard/ARIA coverage for portal controls, detail toggles, exports, and indicator tooltips per indicator_plan.md.
- p2: Python portal migration (implementation): build new portal shell (layout harvested from pythonPortal) with no legacy JS, wired to current HTTP API/MCP/test endpoints; add placeholders where gaps remain.
- p3: Portal testing/diagnostics hub: repurpose overview layout as Testing/Diagnostics skeleton; surface CLI/MCP/API test affordances; label dead buttons until wired.
