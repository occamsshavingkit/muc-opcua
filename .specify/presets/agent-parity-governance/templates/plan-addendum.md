## Agent Parity Planning Checks

- Confirm the full list of maintained agent surfaces for this project.
- Plan the atomic update set: every shared-guidance change must hit each
  maintained surface in the same commit or pull request.
- Plan synchronised updates to project templates under
  `.specify/templates/` and to `.specify/memory/constitution.md` where
  shared principles change.
- Plan documentation of any intentional deviation between surfaces (what
  deviates, why, expected lifetime).
- If model-routing guidance changes, keep it agent-neutral and plan the
  update across all maintained agent surfaces; never pin model names in
  reproducible Spec-Kit feature artifacts.
- Plan a parity check task that validates every declared surface reflects
  the same shared rules at the end of the change.

## Audit Evidence Planning

- Plan audit-ready Markdown evidence for this Spec-Kit run, including owner, reviewer, evidence path, and standard-specific applicability.
- Plan how each relevant checkpoint will be recorded as `Applicable`, `N/A`, or `Open`.
- Plan concrete evidence updates under the default evidence directory for this preset; do not leave checklist templates unfilled.
- Treat `Open` as temporary: assign an owner, follow-up, and re-evaluation trigger.
