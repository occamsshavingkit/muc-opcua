## Spec-Kit Run Evidence

- Feature / Spec ID:
- Spec-Kit phase: [specify / plan / tasks / implement / review / release]
- Branch / commit / PR:
- Run date:
- Evidence owner:
- Reviewer:
- Standards / criteria checked: Agent guidance parity, shared instruction synchronization, intentional-deviation evidence, agent-neutral Spec-Kit model routing
- Decision: [Applicable / N/A / Open]
- Evidence path:
- N/A rationale, if not applicable:
- Open follow-up owner and trigger:
- Re-evaluation trigger:
- Certification-readiness note: Use this record to prove that every maintained agent surface was reviewed or that deviations were intentionally documented.

## Audit Evidence Matrix

| Checkpoint / control reference | Applicability | Evidence produced or linked | Result | Residual risk / rationale |
| --- | --- | --- | --- | --- |
| Spec-Kit run scope is identified | [Applicable / N/A / Open] | | [OK / Open / N/A] | |
| Standard-specific criteria are mapped | [Applicable / N/A / Open] | | [OK / Open / N/A] | |
| Evidence artefact path is recorded | [Applicable / N/A / Open] | | [OK / Open / N/A] | |
| N/A decisions are justified | [Applicable / N/A / Open] | | [OK / Open / N/A] | |
| Open findings have owner and trigger | [Applicable / N/A / Open] | | [OK / Open / N/A] | |

## Agent Parity Governance Agent Guidance

- Treat shared agent guidance as a single logical document with multiple
  physical surfaces. Use the project's declared parity set as the source
  of truth.
- Common surfaces include `AGENTS.md`, `CLAUDE.md`, `GEMINI.md`,
  `.github/copilot-instructions.md`, `.cursorrules`, `.windsurfrules`,
  `JUNIE.md`, or project-specific instruction files. Do not assume this
  list is complete.
- Never update one of these files in isolation when the change affects
  shared rules. If you find yourself editing only one, stop and update
  the others in the same change.
- When propagating a shared rule, also update the relevant project
  templates under `.specify/templates/` and `.specify/memory/constitution.md`.
- Document every intentional deviation: which surface deviates, what it
  deviates from, why, and the expected lifetime of the deviation.
- Document every `N/A` decision with rationale; never silently omit.
- Use `agent-parity-checklist-template` as the standard verification
  artefact for shared-guidance changes.

## Spec-Kit Model Routing

- Treat model choice as operational agent-routing guidance, not as a
  feature requirement.
- Do not pin model names in `spec.md`, `plan.md`, `tasks.md`, or
  individual feature specs. Those artifacts must remain reproducible even
  when model names change or a different AI agent is used.
- Each agent maps these recommendations to its currently available
  models. Do not derive a fixed vendor or model requirement from this
  preset.
- For Spec-Kit specification, clarification, planning, task generation,
  and analysis, prefer the strongest available frontier reasoning/coding
  model.
- For complete long-running `speckit.implement` or `/speckit-implement`
  runs, prefer the strongest available long-running agent model; use a
  frontier model when maximum judgment quality is more important than
  runtime stability.
- For focused review or CI fixes, prefer a coding-optimized model.
- For trivial cleanup, formatting, or low-risk mechanical edits, a fast
  small coding model is acceptable.

## Audit-Ready Spec-Kit Evidence

- When this preset applies, generated or updated Markdown evidence must include the Spec-Kit run, owner/reviewer, evidence path, applicability decision, N/A rationale where relevant, and open follow-up tracking.
- Do not treat an unfilled starter template as evidence. Evidence exists only after the current run has recorded concrete decisions, paths, and rationale.
