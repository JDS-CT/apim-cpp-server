# Example Checklist Table

## Procedure Noun

- **Action**: Procedure Verb
- **Spec**: Checklist loaded
- **Result**:
- **Status**:
- **Comment**:

### Procedure Details

Add any prose that operators need while they walk this row. The workspace keeps
this text attached to the row metadata so exports and state saves include the
same guidance.

## Procedure Follow-up

- **Action**: Confirm telemetry upload
- **Spec**: Checklist renders default content
- **Result**:
- **Status**:
- **Comment**:

### Additional Notes

- Document "done" criteria or escalation steps here.
- Drop Markdown checklists in this directory to surface them in the shared
  workspace UI.

Each `*.md` file becomes a new slug derived from the filename. Filenames can
include uppercase letters, spaces, and punctuation - the runtime lowercases the
name and URL-encodes it for you (e.g., `ATR Precheck.md` -> slug `atr precheck`).
The slug shows up as another tab in `/workspace/checklists`, and the portal
renders the sections/rows declared in the Markdown file. Shared checklists now
get their own sqlite namespace (and JSONL cache) automatically, so edits through
the workspace APIs persist just like the legacy PM/Survey slugs.
- The Checklist Portal uses the exact filename casing but omits the extension
  for its toolkit button label. Rename the file when you want to adjust the
  displayed name.

#### Guidelines

1. Use `#` for table/group headings and `##` for the interactive rows. Each row
   header becomes the interaction point for API commands and UI edits.
2. Provide the standard `Action`, `Spec`, `Result`, `Status`, and `Comment`
   bullet formatting inside each `##` block so the parser can populate the
   workspace table.
3. Workspace APIs (update/state export/import) are fully enabled for these
   slugs. If you need to reset the data back to the Markdown baseline, run the
   workspace "Reset" action or delete the generated JSONL under
   `docs/checklists/.generated/`.
4. This README doubles as the generic default checklist, so a clean checkout
   renders something useful even before custom asset packs are loaded, and the runtime
   falls back to it whenever slug-specific markdown (for example `pm.md` or `survey.md`) is missing. See
   `sample.md` for an expanded example with multiple sections.

#### Canonical JSONL rows

Markdown rows are converted into canonical JSONL entries by
`apim_common.markdown_checklist.build_checklist_jsonl`. Each `#` heading becomes a
`section`, every `##` heading becomes a `procedure`, and the bullet fields map directly to
the canonical schema consumed by storage, workspace ingestion, and the TeX exporters:

```json
{
  "checklist": "<slug derived from the filename>",
  "section": "<# heading text>",
  "procedure": "<camelCased ## heading>",
  "action": "<Action bullet text (defaults to procedure)>",
  "spec": "<Spec bullet text>",
  "result": "<Result bullet text>",
  "status": "<Status bullet text>",
  "comment": "<Comment bullet text>",
  "timestamp": "<ISO8601 string added during parsing>"
}
```

Optional metadata parsed from the markdown (`sameAs`, `alias`, front matter, etc.) lands
under a `metadata` key, and downstream tooling should normalize to this layout when
reading or writing checklist rows so everything speaks the same language.

#### Template overrides

- The default TeX layout lives at `docs/checklists/tex/templates/checklist_template.tex`
  and only renders the generated tables.
- `README.tex` mirrors that fallback and is the template the runtime uses whenever a slug does not ship its own `.tex` file. You can safely delete `sample.tex` (or even the default template) when shipping custom branding; the runtime logs the absence and continues with the README-driven table export.
- Place an optional `<slug>.tex` file inside the same directory when you need
  slug-specific branding (for example `sample.tex`, `arbitraryName.tex`). When
  present, the slug-specific template entirely replaces the shared fallback.
- Generated outputs are saved under `docs/checklists/tex/reports/<slug>/<timestamp>/`
  so each run gets its own folder containing the `.tex` (and optional `.pdf`)
  artefacts.
