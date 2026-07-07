---
description: "Compare two specs or two versions of a spec to quantify scope differences"
---

# Spec Scope Comparison

Compare two specs (or two versions of the same spec) side-by-side and quantify the scope difference — how many requirements were added, removed, or changed, and what that means for effort and timeline.

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty). The user may specify:
- Two spec file paths to compare (e.g., "feature-a/spec.md vs feature-b/spec.md")
- A git ref for historical comparison (e.g., "compare against main", "compare against v1.0")
- Output format (e.g., "markdown", "json", "summary")
- Focus area (e.g., "requirements only", "full comparison")

## Prerequisites

1. Confirm you are inside a git repository.
2. Determine the two versions to compare:
   - If two file paths given: read both specs
   - If git ref given: read current spec and `git show <ref>:.specify/spec.md`
   - If nothing given: compare current spec against the most recent committed version
3. Read both versions of spec.md completely.
4. If plan.md and tasks.md exist for both versions, include them in comparison.

## Outline

1. **Extract Requirements from Both Versions**: Build complete requirement lists.

   - Parse Version A (e.g., original/main) requirements, scenarios, criteria
   - Parse Version B (e.g., current/branch) requirements, scenarios, criteria
   - Match requirements across versions by keyword similarity

2. **Classify Differences**: Categorize each change.

   ```markdown
   ## Scope Differences

   | Change | Type | Version A | Version B |
   |--------|------|-----------|-----------|
   | Rate limiting | Modified | IP-based (5/15min) | Account-based (10/30min) |
   | PDF export | Added | — | New requirement |
   | SMS notifications | Removed | Was required | Dropped |
   | Email verification | Unchanged | ✓ | ✓ |
   ```

3. **Quantify Scope Delta**: Calculate the net change.

   ```markdown
   ## Scope Delta

   | Metric | Version A | Version B | Delta |
   |--------|-----------|-----------|-------|
   | Requirements | 8 | 9 | +1 (+12.5%) |
   | User Scenarios | 3 | 4 | +1 (+33.3%) |
   | Success Criteria | 5 | 6 | +1 (+20.0%) |
   | Task Items | 14 | 18 | +4 (+28.6%) |
   | External Integrations | 2 | 3 | +1 (+50.0%) |
   | **Complexity Score** | **52** | **71** | **+19 (+36.5%)** |
   ```

4. **Effort Impact**: Estimate how the scope change affects timeline.

   ```markdown
   ## Effort Impact

   | Scenario | Version A | Version B | Additional |
   |----------|-----------|-----------|------------|
   | Optimistic | 33h | 42h | +9h |
   | Realistic | 50h | 65h | +15h |
   | Pessimistic | 72h | 95h | +23h |

   **Scope increased by ~36%. Estimated additional effort: +15 hours (realistic).**
   ```

5. **Visual Comparison**: Show a clear before/after summary.

   ```markdown
   ## Summary

   Version A (original)  ████████████████░░░░ 52 complexity
   Version B (current)   ██████████████████████░░ 71 complexity
                                              ^^^^^^ +36.5%

   Added:    1 requirement, 1 scenario, 4 tasks, 1 integration
   Modified: 1 requirement (rate limiting)
   Removed:  1 requirement (SMS notifications)
   Net:      +1 requirement, +19 complexity points
   ```

6. **Output**: Deliver the full comparison report.

## Rules

- **Read-only** — never modify any files, only compare and report
- **Version-explicit** — always clearly label which version is A and which is B
- **Quantitative** — produce numeric deltas, not just "scope increased"
- **Effort-linked** — always translate scope changes into estimated effort impact
- **Git-aware** — support comparing against any git ref (branch, tag, commit hash)
- **Matched comparison** — align requirements across versions by semantic similarity, not just line position
- **Net and gross** — show both gross changes (total added + removed) and net change (final delta)
