# Spec Validate

Comprehension validation, review gating, and approval state for spec-kit artifacts.

## Overview

Spec Validate ensures developers actually read and understand generated specifications before building. It adds a staged-reveal multiple-choice quiz, peer review workflow, and a hard gate before implementation.

## Install

1. Copy `spec-kit-spec-validate/` into your project's `.specify/extensions/` directory
2. Register the extension with spec-kit (the hooks in `extension.yml` are auto-detected)
3. Optionally copy `config-template.yml` to `.specify/extensions/spec-validate/spec-validate-config.yml` and customize

## Commands

### `/speckit.spec-validate.validate`

Validate your comprehension of the current spec. Presents sections one at a time with multiple-choice questions for critical items.

```
/speckit.spec-validate.validate
```

- Specs with 0 critical items are auto-passed
- Wrong answers are recorded (no retries) and may trigger peer review
- Results are stored as JSON with a SHA-256 content hash

### `/speckit.spec-validate.validate-tasks`

Same validation mechanism applied to `tasks.md`.

```
/speckit.spec-validate.validate-tasks
```

### `/speckit.spec-validate.gate`

Hard gate before implementation. Runs automatically as a mandatory `before_implement` hook.

- **Blocks** when: no validation, failed validation, stale hash, pending review, changes requested
- **Allows** when: validated + review met, stale + review approved, maintainer override
- **Warns** when: review timed out + author self-approved

### `/speckit.spec-validate.review`

Peer review for validated specs. Reviewer sees missed items highlighted.

```
/speckit.spec-validate.review
```

- Approve, request changes, or add notes
- 24-hour SLA — times out to warning with author self-approval

### `/speckit.spec-validate.status`

Read-only display of current validation and approval state.

```
/speckit.spec-validate.status
```

### `/speckit.spec-validate.analytics`

Private and aggregated comprehension analytics.

```
/speckit.spec-validate.analytics
```

## Workflow

```
/speckit.specify          → spec.md generated
      ↓
[after_specify hook]      → "Run comprehension validation?" (optional)
      ↓
/speckit.spec-validate.validate  → staged quiz
      ↓
/speckit.tasks            → tasks.md generated
      ↓
[after_tasks hook]        → "Run task validation?" (optional)
      ↓
/speckit.spec-validate.validate-tasks  → staged quiz
      ↓
/speckit.implement
      ↓
[before_implement hook]   → gate check (MANDATORY)
      ↓
      ├── allowed         → proceed
      ├── blocked         → STOP, show actions needed
      └── warning         → proceed with warnings
```

## Configuration

Edit `.specify/extensions/spec-validate/spec-validate-config.yml`:

| Setting | Default | Description |
|---------|---------|-------------|
| `complexity.low_max_items` | 3 | Specs with ≤N items = low complexity |
| `complexity.high_min_items` | 8 | Specs with ≥N items = high complexity |
| `always_critical_sections` | See config | Sections always treated as critical |
| `review.require_on_any_miss` | true | Force review on wrong answers |
| `review.sla_hours` | 24 | Hours before review times out |
| `review.timeout_policy` | warning-with-author-reason | Timeout behavior |
| `gate.block_without_validation` | true | Block if not validated |
| `gate.allow_maintainer_override` | true | Allow override with reason |
| `privacy.personal_metrics_visibility` | self-and-maintainers | Who sees personal metrics |
| `question_generation.mode` | deterministic-first | Distractor generation strategy |

## Solo Developer Mode

Solo developers skip peer review entirely. Self-validation is the only gate, regardless of artifact complexity. No 24-hour timeout wait needed.

## Troubleshooting

### "BLOCKED: Spec has not been validated"
Run `/speckit.spec-validate.validate` before attempting implementation.

### "BLOCKED: Spec content has changed since validation"
Spec was edited after validation. Re-run `/speckit.spec-validate.validate`.

### "BLOCKED: Reviewer requested changes"
Address reviewer feedback, update the spec, then re-validate.

### "BLOCKED: Peer review is pending"
Wait for reviewer, or use maintainer override if urgent.

### "WARNING: Review timed out after 24h SLA"
Reviewer didn't respond in time. Author self-approved — implementation proceeds with warning.

## Privacy

- **Git-tracked**: Approval state, review outcomes, override reasons (visible in PRs)
- **Local only**: Wrong answers, distractor selections, personal accuracy (never in git)
- **Team analytics**: Aggregated only — no individual names on scores

## License

MIT
