# Validate Task Comprehension

## Command: `speckit.spec-validate.validate-tasks`

Validate the current feature's `tasks.md` using the same staged-reveal multiple-choice mechanism as spec validation. Results are stored independently from spec validation with a separate content hash.

## Prerequisites

- A generated `tasks.md` exists for the current feature
- Shell scripts are available: `compute-hash.sh`, `read-approval-state.sh`, `write-approval-state.sh`

## Execution Flow

This command follows the **exact same flow** as `validate.md`, with these differences:

### Differences from Spec Validation

1. **Target artifact**: Read `tasks.md` instead of `spec.md`
2. **Hash field**: Store hash as `tasks_hash` (not `spec_hash`)
3. **Validation field**: Update `tasks_self_validation` (not `spec_self_validation`)
4. **Timestamp field**: Update `tasks_validated_at` (not `spec_validated_at`)
5. **Item classification for tasks**:
   - **Simple**: Straightforward setup tasks, single-file creation tasks, obvious configuration
   - **Critical**: Tasks with complex dependencies, multi-step acceptance criteria, edge case handling, integration tasks, tasks referencing specific contracts or data model constraints
6. **Private analytics**: Write attempt details separately from spec analytics in the same private state file, using `"artifact": "tasks.md"` in the attempts array

### Preserved Behavior

- Staged reveal (one section/phase at a time)
- Multiple-choice with deterministic-first distractor generation
- No retries on wrong answers
- Auto-pass when 0 critical items
- Same review trigger rules (missed items, complexity)
- Same agent-adaptive UX (vscode/askQuestions fallback)
- Same privacy model (git-tracked approval state + local private analytics)

### State Update

When writing approval state, **preserve all spec-related fields**. Only update:
- `tasks_hash`
- `tasks_self_validation`
- `tasks_validated_at`

Do NOT overwrite `spec_hash`, `spec_self_validation`, or `spec_validated_at`.

## Important Constraints

- Tasks validation is independent from spec validation — each has its own hash and state
- Both must pass (or be auto-passed) for the gate to allow implementation
- The gate evaluates both `spec_self_validation` and `tasks_self_validation`
