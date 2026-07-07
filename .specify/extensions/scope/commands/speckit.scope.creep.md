---
description: "Detect scope creep by comparing current spec against its original version in git history"
---

# Scope Creep Detection

Compare the current spec against its earliest committed version to detect scope creep — requirements that grew, acceptance criteria that expanded, and phases that multiplied since the original spec was written. Quantifies how much the spec has grown and flags the biggest scope increases.

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty). The user may specify:
- Baseline reference (e.g., "original commit", "sprint start", "specific commit hash")
- Creep threshold (e.g., "flag anything over 20% growth")
- Scope (e.g., "requirements only", "include tasks", "full analysis")

## Prerequisites

1. Confirm you are inside a git repository.
2. Verify `.specify/` directory exists with at least `spec.md`.
3. Find the original version of spec.md using `git log --follow --diff-filter=A -- .specify/spec.md` to get the first commit.
4. Read the original version using `git show <first-commit>:.specify/spec.md`.
5. Read the current version of spec.md.
6. If plan.md and tasks.md exist, check their history too.

## Outline

1. **Identify Baseline**: Find the original spec version.

   ```markdown
   ## Baseline

   | Field | Value |
   |-------|-------|
   | Original commit | `a1b2c3d` |
   | Original date | 2026-03-15 |
   | Original author | @developer |
   | Current date | 2026-04-11 |
   | Days elapsed | 27 days |
   | Spec revisions | 8 commits |
   ```

2. **Measure Growth**: Compare original vs current across all dimensions.

   ```markdown
   ## Scope Growth

   | Metric | Original | Current | Growth |
   |--------|----------|---------|--------|
   | Requirements | 5 | 9 | +4 (+80%) 🔴 |
   | User Scenarios | 2 | 4 | +2 (+100%) 🔴 |
   | Success Criteria | 3 | 7 | +4 (+133%) 🔴 |
   | Task Items | 8 | 18 | +10 (+125%) 🔴 |
   | Phases | 2 | 4 | +2 (+100%) 🔴 |
   | External Integrations | 1 | 3 | +2 (+200%) 🔴 |
   | **Complexity Score** | **28** | **71** | **+43 (+154%)** 🔴 |
   ```

3. **Identify Creep Sources**: Pinpoint exactly what was added.

   ```markdown
   ## What Crept In

   | # | Addition | When Added | Impact |
   |---|---------|------------|--------|
   | 1 | PDF export requirement | Commit `d4e5f6` (Mar 22) | +6h effort |
   | 2 | Admin dashboard feature | Commit `g7h8i9` (Mar 28) | +12h effort |
   | 3 | Email verification flow | Commit `j1k2l3` (Apr 2) | +8h effort |
   | 4 | Rate limiting changed from simple to account-based | Commit `m4n5o6` (Apr 8) | +4h effort |

   **Total creep: +30 hours of additional effort beyond original estimate**
   ```

4. **Creep Timeline**: Show when scope grew over time.

   ```markdown
   ## Creep Timeline

   Mar 15 ████░░░░░░░░░░░░░░░░ 28 (original)
   Mar 22 ██████░░░░░░░░░░░░░░ 38 (+PDF export)
   Mar 28 ██████████░░░░░░░░░░ 52 (+admin dashboard)
   Apr 02 █████████████░░░░░░░ 61 (+email verification)
   Apr 08 ██████████████████░░ 71 (+rate limiting change)
   Apr 11 ██████████████████░░ 71 (current)

   Scope grew 154% in 27 days
   ```

5. **Creep Severity Assessment**: Rate the overall creep level.

   ```markdown
   ## Creep Assessment

   | Level | Threshold | Status |
   |-------|-----------|--------|
   | 🟢 Healthy | 0-20% growth | — |
   | 🟡 Watch | 21-50% growth | — |
   | 🟠 Warning | 51-100% growth | — |
   | 🔴 Critical | 100%+ growth | ← THIS SPEC (154%) |

   **Verdict: 🔴 CRITICAL SCOPE CREEP**
   This spec has more than doubled since its original version.
   ```

6. **Recommendations**: Suggest how to address the creep.

   ```markdown
   ## Recommendations

   1. **Split the spec** — Original scope (5 reqs) is a viable MVP. New additions could be Phase 2.
   2. **Re-estimate** — Original estimate of 30h is no longer valid. Current realistic estimate: 65h.
   3. **Prioritize** — Rank the 4 new additions by business value. Cut the lowest-value one.
   4. **Freeze scope** — No new requirements until current scope is implemented.
   ```

## Rules

- **Read-only** — never modify any files, only analyze git history and report
- **Git-authoritative** — all baseline data comes from git history, not assumptions
- **Percentage-based** — always show growth as percentages for easy comprehension
- **Timeline-aware** — show WHEN creep happened, not just WHAT changed
- **Effort-linked** — translate every scope addition into estimated effort impact
- **Threshold-based** — classify creep severity using clear thresholds (healthy/watch/warning/critical)
- **Actionable** — always end with concrete recommendations to address creep
- **Non-judgmental** — scope changes aren't inherently bad; report facts and let teams decide
