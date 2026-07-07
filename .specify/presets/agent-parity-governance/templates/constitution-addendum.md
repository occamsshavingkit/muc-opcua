## Agent Guidance Parity

### Mandate

Shared AI-agent guidance is only valid when all maintained agent surfaces
stay aligned. Divergent agent instructions create silent process drift
where one tool follows a rule another tool ignores.

### Maintained agent surfaces

Each project MUST declare its maintained agent guidance surfaces in the
local constitution or equivalent governance document. Common surfaces
include:

- `AGENTS.md`
- `CLAUDE.md`
- `GEMINI.md`
- `.github/copilot-instructions.md`
- `.cursorrules`
- `.windsurfrules`
- `JUNIE.md`
- other project-specific instruction files

This preset does not require any specific vendor or agent mix. Only
surfaces declared by the project are mandatory for that project.

### Mandatory rules

- Shared operational rules MUST NOT be updated in only one of the
  maintained agent files.
- Any intentional deviation between surfaces MUST be documented
  explicitly in the same change that introduces it (where it deviates
  and why).
- Shared rules MUST also be propagated to:
  - the relevant project templates under `.specify/templates/`
  - `.specify/memory/constitution.md`
- Runtime guidance text MUST name all maintained agent surfaces — never
  list only one or two.

### Spec-Kit model routing

Spec-Kit model routing guidance is part of shared agent guidance. It MUST
stay agent-neutral and MUST NOT pin provider-specific model names in
feature artifacts.

- `spec.md`, `plan.md`, `tasks.md`, and individual feature specs MUST NOT
  encode model names as requirements.
- Each agent SHOULD map the routing guidance to its currently available
  models.
- Specification, clarification, planning, task generation, and analysis
  SHOULD use the strongest available frontier reasoning/coding model.
- Complete long-running implementation runs SHOULD use the strongest
  available long-running agent model, unless maximum judgment quality is
  more important than runtime stability.
- Focused review and CI fixes SHOULD use a coding-optimized model.
- Trivial cleanup, formatting, or low-risk mechanical edits MAY use a
  fast small coding model.

### Atomic change discipline

A single pull request or commit that touches shared agent guidance MUST
include all maintained surfaces. Splitting parity changes across multiple
commits is allowed only if the splitting itself is part of an explicit
migration plan recorded in the change.

### Evidence

- Default location for parity-evidence artefacts: project root and the
  project's declared agent guidance surfaces.
- A `agent-parity-checklist` SHOULD be filed for changes that touch
  shared guidance — see `agent-parity-checklist-template`.
