# Codacy Issue Triage — 2026-07-04

**Total issues**: 3,598 across 5 languages, 6 categories, 77 patterns.

## Severity Distribution

| Severity | Count | % |
|----------|-------|---|
| Error    | 323   | 9% |
| High     | 325   | 9% |
| Warning  | 2,949 | 82% |
| Info     | 1     | <1% |

## Category Distribution

| Category     | Count | Notes |
|-------------|-------|-------|
| ErrorProne  | 2,504 | 70% — mostly MISRA style rules |
| Security    | 586   | 16% — mostly memset/memcpy/strlen tool noise |
| Complexity  | 284   | 8% — Lizard thresholds |
| UnusedCode  | 217   | 6% — unused struct members |
| BestPractice | 4    | <1% |
| Compatibility | 3  | <1% |

---

## Triage: 4 Tiers

### TIER 1 — Security Findings Requiring Manual Review (~50 issues)

These are the **real** security findings (not memset/memcpy noise). Each should be reviewed individually.

| Pattern | Count | Severity | Location | Risk |
|---------|-------|----------|----------|------|
| `fopen/open` (Semgrep + flawfinder) | 14 | High | 7 test files | Low — test code only |
| `dangerous-subprocess-use-audit` | 2 | Error | `scripts/run_speed_bench.py` | Medium — script tools |
| `dangerous-subprocess-use-tainted-env-args` | 1 | Error | `scripts/run_speed_bench.py` | Medium |
| `printf/vprintf` format string | 1 | Error | `tests/benchmark/hotpath_benchmark.c` | Low — benchmark |
| `fprintf/vfprintf` format string | 2 | Error | `tests/benchmark/audit_latency_benchmark.c` | Low — benchmark |
| `snprintf/vsnprintf` format string | 1 | Error | `tests/benchmark/audit_latency_benchmark.c` | Low — benchmark |
| `strcpy/strncpy` insecure | 3 | Error/High | `tests/unit/test_pubsub.c` | Low — test |
| `read` (flawfinder) | 5 | Error | `server.c`, `host_tcp_adapter.c`, `value_source.c` | **Review** |
| `usleep` (flawfinder + Semgrep) | 4 | High | `examples/minimal_server/main.c`, `examples/pubsub_server/main.c` | Low — examples |
| Bandit `assert` | 2 | High | `tests/interop/interop_smoke.py` | Low — test |
| Bandit `subprocess` | 3 | High/Warn | `scripts/run_speed_bench.py` | Low — script |
| Bandit `hardcoded tmp` | 2 | High | `tests/tools/test_speed_benchmark.py` | Low — test |
| Prospector bandit | 7 | High | Python test/script files | Low — non-production |
| `shellcheck_SC2094` | 2 | High | `scripts/check_build_matrix.sh` | Low — CI script |

**Recommendation**: Mark the 14 fopen/open findings as `AcceptedUse` (test infrastructure). Review the `read` calls in production code and the subprocess usage in scripts. The rest are test/benchmark/examples — low risk.

---

### TIER 2 — ErrorProne (Should Fix, ~100 issues)

These are genuine code quality concerns that should be addressed:

| Pattern | Count | Severity | Description |
|---------|-------|----------|-------------|
| `intToPointerCast` | 22 | High | Casting integer literals to pointers (MISRA 11.9); mostly in test fuzz harnesses |
| `Pyright` type errors | 5 | High | Python type annotation issues in 3 files |
| `null-dereference` (C#) | 2 | Error | Potential null deref in `tests/interop/dotnet/Program.cs` |
| `knownConditionTrueFalse` | 1 | High | Always-true condition in `src/services/query.c:188` |
| `return-not-in-function` | 2 | High | `return` outside function in `scratch/check_sorted.py`, `scratch/sort_base_nodes_braces.py` |
| `PyLint W0718` | 1 | High | Broad exception catch in `tests/interop/interop_smoke.py` |

**Recommendation**: Fix these. The `intToPointerCast` issues (22) can be fixed mechanically by using named pointer constants instead of integer literal casts. The Python issues are one-liners. The `knownConditionTrueFalse` in `query.c` needs code review.

---

### TIER 3 — MISRA Style Violations (Mechanically Fixable, ~1,600 issues)

These are the bulk of the 2,504 ErrorProne warnings. All are mechanical style changes. Fix strategy:

#### 3a. Add braces to single-statement bodies (MISRA 15.6) — 297 issues

**Rule**: "The body of an iteration-statement or a selection-statement shall be a compound-statement"

```c
// Current (297 instances):
if (condition)
    do_thing();

// Required:
if (condition) {
    do_thing();
}
```

**Fix**: Automated script or bulk edit. Low risk. Adds ~600 lines.

#### 3b. Single point of exit (MISRA 15.5) — 467 issues

**Rule**: "A function should have a single point of exit at the end"

```c
// Current pattern (467 instances):
opcua_status_t foo(void) {
    if (bad) return BAD;
    if (bad2) return BAD2;
    return GOOD;
}

// MISRA-required pattern:
opcua_status_t foo(void) {
    opcua_status_t result = GOOD;
    if (bad) { result = BAD; }
    else if (bad2) { result = BAD2; }
    return result;
}
```

**Fix**: Manual refactoring per function. **Consider whether to apply** — many embedded codebases reject this rule because the early-return pattern is safer (fewer state variables to track). The MISRA 2012 amendment allows this with deviation documentation.

#### 3c. Check return values (MISRA 17.7) — 392 issues

**Rule**: "The value returned by a function having non-void return type shall be used"

```c
// Current:
memcpy(dst, src, len);   // 392 instances
memset(buf, 0, size);

// Required:
(void)memcpy(dst, src, len);
```

**Recommendation**: Add `(void)` casts. Mechanical, scriptable.

#### 3d. Explicit operator precedence (MISRA 12.1) — 278 issues

**Rule**: "The precedence of operators within expressions should be made explicit"

```c
// Current:
if (a && b || c && d)

// Required:
if ((a && b) || (c && d))
```

**Fix**: Add parentheses. Semi-mechanical — needs manual review for correctness.

#### 3e. Type safety (MISRA 10.4, 11.5, 11.6, 7.3) — 294 issues

**Rules**: Same essential type for arithmetic operands, proper void pointer casts, no lowercase `l` suffix.

**Fix**: Manual review. Most are pointer cast sites that need explicit casts.

**Note on MISRA**: This codebase **does not** target MISRA compliance. These are cppcheck running MISRA rules because they're enabled. Unless MISRA compliance is a goal, these are lower priority.

---

### TIER 4 — Tool Noise / Intentional Design (~1,300 issues)

These should be **suppressed** in Codacy configuration. They are false positives or accepted design decisions.

#### 4a. memset "insecure" for sensitive data — 221 issues, 81 files

**Pattern**: `insecure-use-memset`

Semgrep flags every `memset(ptr, 0, size)` that occurs before `free(ptr)` or function return. But in this codebase, memset(0) on outgoing buffers is the **correct** zeroization pattern.

**Verdict**: FALSE POSITIVE. Suppress the pattern in `.codacy.yaml`.

#### 4b. memcpy "unsafe" — 265 issues (135 Semgrep + 130 flawfinder), ~52 files

Every `memcpy` in a binary protocol serialization/deserialization library is flagged. These are all bounded copies with explicit length checking.

**Verdict**: FALSE POSITIVE for internal use. Suppress.

#### 4c. strlen on "non-null-terminated" strings — 82 issues (41 Semgrep + 41 flawfinder), 18 files

The flagged strlen calls are on internal OPC UA strings that are guaranteed null-terminated by construction. No production risk.

**Verdict**: FALSE POSITIVE. Suppress.

#### 4d. missingIncludeSystem — 265 issues

cppcheck cannot resolve include paths for the embedded toolchain headers. Not actual missing includes.

**Verdict**: FALSE POSITIVE. Fix cppcheck include path configuration.

#### 4e. unusedStructMember — 214 issues

Flagged struct members are part of the OPC UA information model and may be used in specific profiles or externally. Not truly dead code in most cases.

**Verdict**: PARTIALLY ACTIONABLE. Some are genuine unused members. Needs per-case review but 200+ is noise.

#### 4f. Lizard complexity thresholds — 284 issues

| Metric | Threshold | Issues | Files |
|--------|-----------|--------|-------|
| Cyclomatic complexity (ccn-medium) | 10 | 134 | 43 |
| Function length (nloc-medium) | 50 | 118 | 49 |
| File length (file-nloc-medium) | 1000 | 17 | 17 |
| Parameter count (param-medium) | 5 | 15 | 13 |

Many flagged functions are only slightly over threshold (e.g., CCN of 11-12). The functions flagged are service dispatch handlers and encoding functions — naturally longer due to protocol requirements.

**Recommendation**: Raise thresholds or add per-function exclusions for known complex functions.

#### 4g. Comma operator (MISRA 12.3) — 102 issues

Used intentionally in macros (e.g., `assert`-like patterns). Standard C macro idiom.

**Verdict**: ACCEPTED USE. These are macro expansion sites.

#### 4h. I/O routines (MISRA 21.6) — 68 issues

`printf`/`fprintf` usage in test code, benchmarking, and debug logging. Not in production library code.

**Verdict**: ACCEPTED USE for test/debug infrastructure.

#### 4i. Union usage (MISRA 19.2) — 27 issues

OPC UA protocol encoding uses unions for variant storage and chunk framing. Essential to the design.

**Verdict**: ACCEPTED USE. Core protocol requirement.

#### 4j. Thread safety — 27 issues

cppcheck flags shared variable access. Most are false positives due to single-threaded event loop architecture.

**Verdict**: REVIEW the 5 production code instances (subscription.c, session.c, discovery.c, server.c, tcp_connection.c). Rest are test code.

---

## Summary: Action Plan

| Priority | Action | Issues | Effort |
|----------|--------|--------|--------|
| **P0 - Now** | Configure Codacy to suppress known false positives (memset, memcpy, strlen, missing includes) | ~833 issues eliminated | 1 hour |
| **P1 - Now** | Review and fix TIER 1 security findings (read, subprocess) | ~5 real findings | 2 hours |
| **P2 - This week** | Fix TIER 2 ErrorProne (intToPointerCast, pyright, null-deref) | ~32 fixes | 3 hours |
| **P3 - This week** | Add `(void)` casts for MISRA 17.7 (unused returns) | 392 fixes | 2 hours (scripted) |
| **P4 - This sprint** | Add braces for MISRA 15.6 (single-statement bodies) | 297 fixes | 3 hours (scripted) |
| **P5 - This sprint** | Add parentheses for MISRA 12.1 (operator precedence) | 278 fixes | 4 hours (semi-automated) |
| **P6 - This sprint** | Review thread safety findings in production code | 5 reviews | 2 hours |
| **P7 - Consider** | Single-return refactor (MISRA 15.5) | 467 fixes | 8-16 hours |
| **P8 - Consider** | Type safety casts (MISRA 10.4, 11.x) | 294 fixes | TBD |
| **Suppress** | Mark remaining false positives + accepted use | ~1,300 issues | 1 hour |

**Net result after P0-P6**: ~3,598 → ~1,100 remaining issues (mostly MISRA 15.5 single-return and type safety)

**If MISRA is not a goal**: Only ~80 issues are real code quality concerns. The rest can be suppressed.
