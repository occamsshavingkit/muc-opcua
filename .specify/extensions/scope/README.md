# spec-kit-scope

A [Spec Kit](https://github.com/github/spec-kit) extension that estimates effort from spec artifacts, detects scope creep, compares spec versions, and generates time budgets — turning "how long will this take?" from a guess into a data-driven answer.

## Problem

After writing a spec, the first question is always: "How long will this take?" Today, the answer is a guess:

- Developers estimate from memory, not from what's actually in the spec
- Nobody tracks how much a spec has grown since it was first written
- Scope creep happens silently — requirements multiply but the deadline doesn't move
- Sprint planning ignores spec complexity — a 5-requirement feature gets the same time as a 15-requirement feature
- There's no way to compare two specs and say "this one is 3x bigger"
- Risk buffers are arbitrary ("let's add 20%") instead of based on actual complexity signals

Spec-driven development produces structured requirements — but nobody uses that structure to estimate work.

## Solution

The Spec Scope extension adds four commands that turn specs into project estimates:

| Command | Purpose | Modifies Files? |
|---------|---------|-----------------|
| `/speckit.scope.estimate` | Estimate effort from spec complexity with three-point ranges | No — read-only |
| `/speckit.scope.compare` | Compare two specs to quantify scope differences and effort impact | No — read-only |
| `/speckit.scope.creep` | Detect scope creep by comparing current spec against git history | No — read-only |
| `/speckit.scope.budget` | Generate a time budget with hours per phase, requirement, and buffer | No — read-only |

## Installation

```bash
specify extension add --from https://github.com/Quratulain-bilal/spec-kit-scope/archive/refs/tags/v1.0.0.zip
```

## How It Works

### The Estimation Gap

```
/speckit.specify    → spec.md      Requirements defined
/speckit.plan       → plan.md      Architecture decided
/speckit.tasks      → tasks.md     Work broken down
                    → ???          How long will this take?

/speckit.scope.estimate → report   ← Spec Scope answers this
```

### What Gets Estimated

**`/speckit.scope.estimate`** produces a data-driven effort estimate:

```markdown
## Complexity Score: 66 (🟠 Complex)

| Factor | Count | Weight | Score |
|--------|-------|--------|-------|
| Requirements | 8 | x2 | 16 |
| User Scenarios | 3 | x3 | 9 |
| External Integrations | 2 | x5 | 10 |
| Technical Decisions | 4 | x2 | 8 |
| Task Items | 14 | x1 | 14 |

## Three-Point Estimate

| Scenario | Hours | Days | Weeks |
|----------|-------|------|-------|
| 🟢 Optimistic | 33h | 4 days | ~1 week |
| 🟡 Realistic | 50h | 6 days | ~1.5 weeks |
| 🔴 Pessimistic | 72h | 9 days | ~2 weeks |
```

**`/speckit.scope.creep`** detects how much a spec has grown:

```markdown
## Scope Growth: +154% in 27 days 🔴 CRITICAL

Mar 15 ████░░░░░░░░░░░░░░░░ 28 (original)
Mar 22 ██████░░░░░░░░░░░░░░ 38 (+PDF export)
Mar 28 ██████████░░░░░░░░░░ 52 (+admin dashboard)
Apr 02 █████████████░░░░░░░ 61 (+email verification)
Apr 11 ██████████████████░░ 71 (current)

Total creep: +30 hours beyond original estimate
```

**`/speckit.scope.compare`** quantifies differences between specs:

```markdown
## Scope Delta

| Metric | Version A | Version B | Delta |
|--------|-----------|-----------|-------|
| Requirements | 5 | 9 | +4 (+80%) |
| Complexity Score | 52 | 71 | +19 (+36.5%) |
| Estimated Effort | 33h | 50h | +17h |
```

**`/speckit.scope.budget`** generates a sprint-ready time allocation:

```markdown
## Phase Budget

Setup      ██░░░░░░░░░░░░░░░░░░  5h (10%)
Core Auth  ██████████████████░░░░ 18h (36%)
Admin      ████████████░░░░░░░░░░ 12h (24%)
Testing    ████████░░░░░░░░░░░░░░  8h (16%)
Buffer     ███████░░░░░░░░░░░░░░░  7h (14%)

Total: 50h | Available: 40h | Gap: -10h

## Options to Fit Sprint
| Option | Savings | Fits? |
|--------|---------|-------|
| Defer admin dashboard | -12h | ✅ Yes |
| Add 1 developer | split work | ✅ Yes |
```

## Workflow

```
/speckit.specify                     ← Define the feature
       │
       ▼
/speckit.scope.estimate              ← "How long will this take?"
/speckit.scope.budget                ← "How should we allocate time?"
       │
       ▼
/speckit.plan → /speckit.tasks       ← Plan and break down
       │
       ▼
/speckit.implement                   ← Build it
       │
       ▼
/speckit.scope.creep                 ← "Did scope grow?"
/speckit.scope.compare               ← "How much changed?"
```

## Commands

### `/speckit.scope.estimate`

Data-driven effort estimation from spec artifacts:

- Complexity scoring: requirements, scenarios, integrations, decisions, tasks, phases
- Three-point estimates: optimistic, realistic, pessimistic
- Risk buffer calculation based on actual complexity signals
- Phase-level and requirement-level breakdowns
- Team-aware: adjusts for team size and experience level

### `/speckit.scope.compare`

Side-by-side spec comparison with scope quantification:

- Compare two spec files or two git versions of the same spec
- Requirement-level diff: added, modified, removed, unchanged
- Complexity score delta with percentage change
- Effort impact: translates scope changes into hours
- Git-aware: compare against any branch, tag, or commit

### `/speckit.scope.creep`

Scope creep detection via git history analysis:

- Compares current spec against its first committed version
- Growth timeline showing when each addition happened
- Severity classification: Healthy, Watch, Warning, Critical
- Pinpoints exactly what crept in and when
- Effort impact of each scope addition
- Actionable recommendations to address creep

### `/speckit.scope.budget`

Sprint-ready time budget generation:

- Hours allocated per phase, requirement, and overhead category
- Overhead budget: setup, code review, documentation, risk buffer
- Sprint fit analysis: does the spec fit available hours?
- Trade-off options when scope exceeds budget
- Visual bar chart for quick comprehension
- Dependency ordering for work sequencing

## Hooks

The extension registers one optional hook:

- **after_specify**: Offers to generate an effort estimate after writing a spec

## Design Decisions

- **All commands are read-only** — never modifies any files, only analyzes and reports
- **Range-based estimates** — always gives ranges, never single-point guesses
- **Transparent methodology** — shows the scoring formula so estimates can be challenged
- **Git-authoritative** — creep detection uses git history as the source of truth
- **Sprint-aware** — budget command checks if scope fits available time
- **Trade-off oriented** — when scope exceeds budget, suggests concrete options to fit
- **Non-prescriptive** — reports data and options, lets teams make the decisions

## Requirements

- Spec Kit >= 0.4.0
- Git >= 2.0.0

## License

MIT
