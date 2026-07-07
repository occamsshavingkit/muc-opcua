## Agent Parity Tasks

- Add an explicit task to update every maintained agent surface in the
  same change, based on the project's declared parity set.
- Add tasks to propagate shared rules into the relevant project
  templates under `.specify/templates/` and into
  `.specify/memory/constitution.md`.
- Add a parity-verification task using
  `agent-parity-checklist-template`.
- If model-routing guidance changes, add a task that confirms no
  provider-specific model names were written into `spec.md`, `plan.md`,
  `tasks.md`, or individual feature specs.
- Add a documentation task for any intentional deviation between
  surfaces.
- Add a final review task that confirms no maintained surface was
  silently skipped.

## Audit Evidence Tasks

- Add tasks to create or update the Markdown evidence/checklist documents for this Spec-Kit run.
- Each task must name the target evidence file, the standard or governance checkpoint, and the expected decision: `Applicable`, `N/A`, or `Open`.
- Add tasks to fill evidence rows with reviewer, date, evidence path, residual risk, and follow-up where relevant.
- Add tasks to verify that no relevant checkpoint was silently omitted.
