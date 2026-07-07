# Agent Parity Governance Preset

Version: `0.3.0`
Requires: `spec-kit >= 0.8.0` (uses the `wrap` and `append` composition
strategies introduced in 0.8.x).

Purpose:

- prevent silent process drift between AI-agent guidance files
- enforce that shared rules land atomically across the project's
  declared AI-agent guidance surfaces
- keep Spec-Kit model-routing guidance agent-neutral and out of
  reproducible feature artifacts

Primary source chapter from `home-baseline` constitution:

- `IX. Agent Guidance Parity & Template Synchronization`

What it covers:

- project-declared list of maintained agent surfaces
- atomic-change discipline (one change → all surfaces)
- propagation into project templates and `.specify/memory/constitution.md`
- explicit documentation of intentional deviations
- agent-neutral Spec-Kit model routing by work type
- parity-verification artefact (`agent-parity-checklist-template`)

Preset strategy:

- append parity governance to `constitution-template`, `spec-template`,
  `plan-template`, and `tasks-template`
- provide a standalone agent-guidance addendum template for projects that
  maintain agent instruction files
- wrap `speckit.specify`, `speckit.plan`, and `speckit.tasks` with a
  shared parity workflow
- provide a parity checklist starter

When to use:

- any project that maintains more than one AI-agent guidance file
- any team that wants atomic, auditable changes to shared agent
  instructions
- any project where AI-agent surfaces are part of the contributor
  contract
- any project that wants model routing guidance without pinning a
  provider-specific model in specs, plans, or tasks

Common surfaces include `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`,
`.github/copilot-instructions.md`, `.cursorrules`, `.windsurfrules`,
`JUNIE.md`, or other project-specific instruction files. The preset does
not require a specific vendor or agent mix.

When not to use:

- projects with only one agent guidance file and no plans to add another
- one-off prototypes without long-term agent contributors

Release notes:

- `v0.3.0` adds audit-ready Spec-Kit run evidence fields so generated Markdown documents and checklists can record applicability, N/A rationale, reviewer, evidence path, residual risk, and follow-up per standards-relevant Spec-Kit run.

Recommended standalone install priority:

- `60`
