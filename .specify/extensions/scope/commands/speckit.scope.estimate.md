---
description: "Estimate effort from spec complexity, requirement count, task phases, and technical decisions"
---

# Effort Estimation

Analyze spec artifacts and produce a structured effort estimate — breaking down work by phase, requirement, and complexity. Gives product managers and tech leads a data-driven answer to "how long will this take?" based on what's actually in the spec, not gut feelings.

## User Input

```text
$ARGUMENTS
```

You **MUST** consider the user input before proceeding (if not empty). The user may specify:
- Team size (e.g., "solo developer", "team of 3")
- Experience level (e.g., "senior", "junior", "mixed")
- Time unit preference (e.g., "hours", "days", "story points")
- Confidence level (e.g., "optimistic", "realistic", "pessimistic")
- Specific phase or requirement to estimate

## Prerequisites

1. Confirm you are inside a git repository.
2. Verify `.specify/` directory exists with at least `spec.md`.
3. Read `spec.md` to extract all requirements, scenarios, and success criteria.
4. If `plan.md` exists, read it for technical complexity signals (new tech, integrations, migrations).
5. If `tasks.md` exists, read it for phase structure and task granularity.

## Outline

1. **Calculate Complexity Score**: Analyze spec artifacts to produce a complexity rating.

   - **Requirement count**: Number of distinct requirements in spec.md
   - **Scenario count**: Number of user scenarios
   - **Integration count**: External services, APIs, databases mentioned
   - **Decision count**: Technical decisions in plan.md
   - **Task count**: Total tasks in tasks.md
   - **Phase count**: Number of implementation phases

   ```markdown
   ## Complexity Analysis

   | Factor | Count | Weight | Score |
   |--------|-------|--------|-------|
   | Requirements | 8 | x2 | 16 |
   | User Scenarios | 3 | x3 | 9 |
   | External Integrations | 2 | x5 | 10 |
   | Technical Decisions | 4 | x2 | 8 |
   | Task Items | 14 | x1 | 14 |
   | Implementation Phases | 3 | x3 | 9 |
   | **Total Complexity** | | | **66** |

   | Range | Level |
   |-------|-------|
   | 0-30 | 🟢 Simple |
   | 31-60 | 🟡 Moderate |
   | 61-100 | 🟠 Complex |
   | 100+ | 🔴 Very Complex |

   **This spec: 🟠 Complex (66)**
   ```

2. **Estimate by Phase**: Break down effort per implementation phase.

   ```markdown
   ## Phase Estimates

   | Phase | Tasks | Estimate | Includes |
   |-------|-------|----------|----------|
   | Phase 1: Setup | 4 | 4-6 hours | Project scaffold, dependencies, config |
   | Phase 2: Core Auth | 6 | 12-18 hours | Signup, login, JWT, middleware |
   | Phase 3: Admin | 4 | 8-12 hours | Dashboard, user management |
   | Testing & QA | — | 6-10 hours | Unit, integration, E2E tests |
   | Code Review & Polish | — | 3-5 hours | PR prep, documentation |
   | **Total** | **14** | **33-51 hours** | |
   ```

3. **Estimate by Requirement**: Show effort per individual requirement.

   ```markdown
   ## Requirement Estimates

   | Requirement | Complexity | Estimate | Risk |
   |-------------|-----------|----------|------|
   | REQ-001: User signup | Medium | 4-6h | Low |
   | REQ-002: User login | Medium | 3-5h | Low |
   | REQ-003: JWT tokens | High | 5-8h | Medium (security-critical) |
   | REQ-004: Rate limiting | High | 4-6h | Medium (perf testing needed) |
   | REQ-005: Email verification | High | 6-8h | High (external SMTP dependency) |
   ```

4. **Risk Buffer Calculation**: Add buffer based on uncertainty.

   ```markdown
   ## Risk Buffer

   | Risk Factor | Buffer |
   |-------------|--------|
   | External integrations (2) | +15% |
   | Security-critical features | +10% |
   | First time with this tech stack | +20% |
   | **Total Buffer** | **+25%** (compound, capped) |

   | Estimate | Hours |
   |----------|-------|
   | Base estimate | 33-51h |
   | With risk buffer (+25%) | 41-64h |
   | **Recommended** | **~50 hours** (midpoint with buffer) |
   ```

5. **Three-Point Estimate**: Provide optimistic, realistic, and pessimistic.

   ```markdown
   ## Three-Point Estimate

   | Scenario | Hours | Days (8h/day) | Weeks |
   |----------|-------|---------------|-------|
   | 🟢 Optimistic | 33h | 4.1 days | ~1 week |
   | 🟡 Realistic | 50h | 6.3 days | ~1.5 weeks |
   | 🔴 Pessimistic | 72h | 9.0 days | ~2 weeks |
   ```

6. **Output**: Deliver the complete estimate report.

## Rules

- **Read-only** — never modify any files, only analyze and report
- **Data-driven** — every estimate must trace to specific spec content (requirement count, task count, integration count), never arbitrary
- **Range-based** — always give ranges, never single-point estimates
- **Three-point** — always include optimistic, realistic, and pessimistic scenarios
- **Buffer-explicit** — clearly show what risk buffer is added and why
- **No false precision** — don't estimate to the minute; use hour ranges for small specs, day ranges for large ones
- **Transparent methodology** — show the complexity scoring formula so estimates can be challenged and refined
- **Team-aware** — adjust estimates if user specifies team size or experience level
