# Implementation Gate

## Command: `speckit.spec-validate.gate`

Enforce the hard gate before `/speckit.implement`. This command is registered as a **mandatory** `before_implement` hook — it runs automatically and blocks implementation when validation or review requirements are not met.

## Execution Flow

1. Determine the current feature from `.specify/feature.json`
2. Run `check-gate.sh <feature>` to evaluate the gate
3. Parse the JSON result containing `outcome`, `reason`, `actions_required`, and `warnings`

### Gate Outcomes

**`blocked` (exit code 1):**
- Display the reason clearly
- List all required actions as a numbered checklist
- **FAIL the hook** — this prevents `/speckit.implement` from proceeding
- Example output:
  ```
  BLOCKED: Spec has not been validated
  
  Required actions:
  1. Run /speckit.spec-validate.validate to validate spec.md
  ```

**`allowed` (exit code 0):**
- Proceed silently — no output needed unless warnings exist
- Implementation continues

**`allowed-with-warning` (exit code 0):**
- Display all warnings clearly
- Proceed with implementation
- Example output:
  ```
  WARNING: Review timed out after 24h SLA. Author self-approved.
  Proceeding with implementation.
  ```

### Maintainer Override

If the gate returns `blocked` and the user indicates they are a maintainer:

1. Prompt: "This feature is blocked. As a maintainer, you can override the gate. Provide a reason for the override:"
2. Record the override reason in the approval state:
   ```json
   {
     "override": {
       "used": true,
       "reason": "<maintainer's reason>",
       "by": "<maintainer identifier>"
     },
     "approval_status": "override-approved"
   }
   ```
3. Write updated state via `write-approval-state.sh`
4. Re-run `check-gate.sh` — it should now return `allowed`

## Important Constraints

- This command is **separate from status** — status reports, gate enforces
- The gate MUST fail the hook (stop the workflow) when outcome is `blocked`
- Override reasons are stored in git-tracked state and visible in PR diffs
- The gate does NOT evaluate review SLA timeout — that is the review command's responsibility. The gate reads the current `review_status` and `timeout_self_approval` fields as-is.
