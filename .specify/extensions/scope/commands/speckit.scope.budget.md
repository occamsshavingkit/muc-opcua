---
description: "Generate a time budget showing estimated hours per phase, requirement, and risk buffer"
---

# Scope Budget

Generate a structured time budget that allocates estimated hours across phases, requirements, and overhead categories. Designed for sprint planning, resource allocation, and stakeholder communication — turns a spec into a project plan with time allocations.

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty). The user may specify:
- Available hours (e.g., "we have 40 hours this sprint")
- Team size (e.g., "2 developers")
- Priority constraints (e.g., "must ship auth first")
- Budget format (e.g., "sprint plan", "gantt-style", "summary")

## Prerequisites

1. Confirm you are inside a git repository.
2. Verify `.specify/` directory exists with at least `spec.md`.
3. Read `spec.md` to extract requirements and scenarios.
4. If `plan.md` exists, read it for technical complexity and architecture decisions.
5. If `tasks.md` exists, read it for phase structure and task breakdown.
6. Run complexity estimation on spec artifacts.

## Outline

1. **Calculate Total Budget**: Estimate total effort from spec complexity.

   - Use complexity scoring methodology (same as `/speckit.scope.estimate`)
   - Produce base estimate range
   - Add risk buffer

2. **Allocate by Phase**: Distribute hours across implementation phases.

   ```markdown
   ## Phase Budget

   | Phase | Tasks | Hours | % of Total | Priority |
   |-------|-------|-------|------------|----------|
   | Phase 1: Setup | 4 | 5h | 10% | P1 — blocking |
   | Phase 2: Core Auth | 6 | 18h | 36% | P1 — core feature |
   | Phase 3: Admin | 4 | 12h | 24% | P2 — enhancement |
   | Testing & QA | — | 8h | 16% | P1 — quality gate |
   | Buffer & Review | — | 7h | 14% | — risk buffer |
   | **Total** | **14** | **50h** | **100%** | |
   ```

3. **Allocate by Requirement**: Show hours per individual requirement.

   ```markdown
   ## Requirement Budget

   | Requirement | Hours | Phase | Dependencies |
   |-------------|-------|-------|-------------|
   | REQ-001: Signup | 5h | Phase 2 | None |
   | REQ-002: Login | 4h | Phase 2 | REQ-001 |
   | REQ-003: JWT | 5h | Phase 2 | REQ-002 |
   | REQ-004: Rate limiting | 5h | Phase 2 | REQ-002 |
   | REQ-005: Admin dashboard | 8h | Phase 3 | REQ-001, REQ-002 |
   ```

4. **Overhead Allocation**: Budget non-feature work.

   ```markdown
   ## Overhead Budget

   | Category | Hours | Justification |
   |----------|-------|---------------|
   | Environment setup | 2h | Docker, CI config, env variables |
   | Code review | 3h | 2 rounds of review estimated |
   | Documentation | 2h | API docs, README updates |
   | Risk buffer | 7h | 14% buffer for unknowns |
   | **Total Overhead** | **14h** | **28% of budget** |
   ```

5. **Sprint Fit Analysis**: Check if the spec fits available time.

   ```markdown
   ## Sprint Fit

   | Metric | Value |
   |--------|-------|
   | Available hours | 40h (user-specified) |
   | Required hours | 50h (realistic estimate) |
   | Deficit | -10h (25% over budget) |
   | Status | 🔴 Does NOT fit |

   ## Options to Fit Sprint
   | Option | Cut | New Total | Fits? |
   |--------|-----|-----------|-------|
   | Defer Phase 3 (admin) | -12h | 38h | ✅ Yes |
   | Defer rate limiting | -5h | 45h | 🟡 Tight |
   | Reduce testing to unit only | -4h | 46h | 🟡 Risky |
   | Add 1 developer (split work) | — | 25h/person | ✅ Yes |
   ```

6. **Visual Budget**: Show allocation as a visual chart.

   ```markdown
   ## Budget Visualization

   Setup      ██░░░░░░░░░░░░░░░░░░  5h (10%)
   Core Auth  ██████████████████░░░░ 18h (36%)
   Admin      ████████████░░░░░░░░░░ 12h (24%)
   Testing    ████████░░░░░░░░░░░░░░  8h (16%)
   Buffer     ███████░░░░░░░░░░░░░░░  7h (14%)

   Total: 50h | Available: 40h | Gap: -10h
   ```

7. **Output**: Deliver the complete budget report.

## Rules

- **Read-only** — never modify any files, only analyze and report
- **Additive** — all phase budgets must sum to the total budget exactly
- **Buffer-explicit** — always include a visible risk buffer (10-25% depending on complexity)
- **Sprint-aware** — if user provides available hours, always check if the spec fits
- **Options-oriented** — if scope exceeds budget, suggest concrete trade-offs to fit
- **Dependency-ordered** — show which requirements block others to guide sequencing
- **Overhead-honest** — always budget for setup, review, and documentation, not just feature work
- **Visual** — include bar chart visualization for quick comprehension
