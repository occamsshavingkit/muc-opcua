# Validate Spec Comprehension

## Command: `speckit.spec-validate.validate`

Validate the current feature's `spec.md` through staged-reveal multiple-choice quizzes.

## Prerequisites

- A generated `spec.md` exists for the current feature
- Shell scripts are available: `compute-hash.sh`, `read-approval-state.sh`, `write-approval-state.sh`

## Execution Flow

### Step 1: Load and Hash

1. Determine the current feature directory from `.specify/feature.json`
2. Read `spec.md` from the feature directory
3. Run `compute-hash.sh <path-to-spec.md>` to get the SHA-256 hash
4. Store the hash for later — this binds validation to this specific content version

### Step 2: Classify Items

1. Parse spec.md into sections (User Scenarios, Requirements, Success Criteria, Edge Cases, etc.)
2. Load `always_critical_sections` from the extension config (`spec-validate-config.yml`):
   - Default critical sections: "Acceptance Criteria", "Edge Cases", "Error Handling", "Security Considerations"
3. For each item in the spec:
   - If item is in a critical section → classify as **critical** (multiple-choice confirmation)
   - If item is straightforward/obvious → classify as **simple** (acknowledge-only)
4. Count critical items. If **0 critical items**: auto-pass — skip to Step 5 with `spec_self_validation: "passed"` and `spec_critical_count: 0`

### Step 3: Staged Reveal

Present sections **one at a time**, blocking progression until the current section is complete.

**For simple items:**
- Display the item
- Ask: "Does this look correct?" (acknowledge-only)
- Move to next item

**For critical items:**
1. Load `templates/distractor-patterns.yml` to determine the distractor pattern category:
   - Behavioral requirement → behavioral pattern
   - Error-handling requirement → error_handling pattern
   - Data/entity requirement → data_constraint pattern
   - Edge case → edge_case pattern
   - Security requirement → security pattern
2. **Deterministic-first**: Generate 2 distractors using the matched pattern template
   - `decoy_1`: Apply the weaker/generic fallback pattern
   - `decoy_2`: Apply the conflicting/opposite behavior pattern
3. **AI fallback**: If deterministic generation cannot produce distinct, plausible distractors for this specific item, use AI to generate contextually appropriate alternatives
4. Present 3 options in **shuffled order** (1 correct + 2 decoys)
5. Wait for user selection
6. **On correct**: Mark item as `confirmed`
7. **On wrong**: Highlight the correct answer, mark item as `missed`, **move to next item (no retries)**

**Agent adaptivity:**
- For agents supporting `vscode/askQuestions`: use that tool for clean multiple-choice UI
- For CLI agents: present options as numbered list in conversational flow
- Fall back gracefully between modes

**Do NOT reveal the next section until the current section is complete.**

### Step 4: Record Results

After all sections are validated:

1. **Git-tracked state** — Run `write-approval-state.sh <feature> <json>` with:
   ```json
   {
     "feature": "<feature-id>",
     "spec_hash": "<hash from Step 1>",
     "spec_self_validation": "passed",
     "spec_validated_at": "<ISO8601 timestamp>",
     "spec_critical_count": <number>,
     "spec_missed_items": ["<item-ids that were missed>"],
     "review_status": "<computed in Step 5>",
     "approval_status": "<computed in Step 5>"
   }
   ```
   Preserve existing `tasks_hash`, `tasks_self_validation`, and other fields.

2. **Local private state** — Write to `.specify/extensions/spec-validate/local/<feature>.private.json`:
   ```json
   {
     "feature": "<feature-id>",
     "validated_by": "<current user or session id>",
     "agent": {"name": "<agent-name>", "version": "<agent-version>"},
     "attempts": [
       {"artifact": "spec.md", "item_id": "<id>", "selected_option": "<text>", "correct": true/false}
     ],
     "analytics": {
       "first_attempt_accuracy": <float 0-1>,
       "total_items": <int>,
       "critical_items": <int>,
       "items_missed": ["<item-ids>"]
     }
   }
   ```
   Create the `local/` directory if it doesn't exist. This directory MUST be gitignored.

### Step 5: Determine Review Requirement

Evaluate review policy from config:

1. **Solo developer** (team size = 1 or no reviewers available): Skip review entirely. Set `review_status: "not-required"`, `approval_status: "allowed"`
2. **Any missed items** AND `review.require_on_any_miss: true`: Set `review_status: "pending"`, `approval_status: "blocked"`, record `review_requested_at`
3. **Medium/high complexity** (critical items >= `complexity.high_min_items` or between low and high) AND `review.require_on_medium_or_high_complexity: true`: Set `review_status: "pending"`
4. **Perfect score + low complexity** (critical items <= `complexity.low_max_items`): Set `review_status: "not-required"`, `approval_status: "allowed"`
5. **Perfect score + medium/high complexity**: Apply complexity-based review rule from config

### Step 6: Report

Display validation summary:
- Total items validated
- Critical items count
- First-attempt accuracy percentage
- Items missed (list IDs)
- Review status (required / not required)
- Next steps (run `/speckit.spec-validate.review` if review pending, or proceed to planning/tasks)
