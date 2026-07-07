# Peer Review

## Command: `speckit.spec-validate.review`

Record peer review decisions for a validated spec. A team member (not the author) reviews the spec with missed items highlighted.

## Prerequisites

- Approval state exists with `review_status: "pending"`
- Reviewer is NOT the same person who validated the spec (unless timeout self-approval path)

## Execution Flow

### Step 1: Load State

1. Determine the current feature from `.specify/feature.json`
2. Run `read-approval-state.sh <feature>` to load approval state
3. Verify `review_status` is `"pending"` — if `"not-required"` or `"approved"`, inform the user and exit
4. Prevent self-review: compare the current user/session to the `validated_by` field in private state. If same person, error: "You cannot review your own validation. Ask a team member to run this command."
   - Exception: timeout self-approval path (see Step 4)

### Step 2: Display Review Context

1. Read the full `spec.md` from the feature directory
2. Highlight items listed in `spec_missed_items` from the approval state — render them with a visual marker (e.g., bold, prefix with `MISSED:`)
3. Display author's first-attempt accuracy: compute from `spec_critical_count` and `spec_missed_items` length
4. Show any existing `review_comments`

### Step 3: Collect Review Decision

Present the reviewer with three options:

**A. Approve**
- Update `review_status` to `"approved"`
- Update `approval_status` to `"allowed"`
- Write updated state via `write-approval-state.sh`

**B. Request Changes**
- Prompt reviewer for specific comments/feedback
- Update `review_status` to `"changes-requested"`
- Append comments to `review_comments` array
- Keep `approval_status` as `"blocked"`
- Write updated state

**C. Add Notes**
- Prompt reviewer for notes on specific items
- Append to `review_comments` without changing `review_status` or `approval_status`
- Write updated state

Record `reviewer` identity and `reviewed_at` timestamp in all cases.

### Step 4: Timeout Handling

Check if the review SLA has expired:

1. Read `review_requested_at` from approval state
2. Read `review.sla_hours` from config (default: 24)
3. If current time > `review_requested_at` + `sla_hours`:
   - Transition `review_status` to `"timed-out"`
   - Inform the author: "Review SLA has expired. You may self-approve with a recorded reason."
   - Prompt for reason: "Why are you proceeding without peer review?"
   - Record in approval state:
     ```json
     {
       "timeout_self_approval": {
         "used": true,
         "reason": "<author's reason>"
       }
     }
     ```
   - Update `approval_status` to `"allowed-with-warning"`
   - Add warning: "Review timed out after 24h SLA — author self-approved"
   - Write updated state

### Step 5: Report

Display review outcome:
- Review decision (approved / changes-requested / notes added / timed-out)
- Updated approval status
- Next steps (proceed to implementation if approved, address feedback if changes requested)

## Important Constraints

- Review decisions are stored in git-tracked state (visible in PRs)
- Personal performance data (attempt details, distractor selections) stays in local private state — never exposed to the reviewer
- The reviewer sees only: full spec, which items were missed (IDs), and accuracy percentage
- Timeout self-approval reason is stored in git-tracked state as an audit trail
