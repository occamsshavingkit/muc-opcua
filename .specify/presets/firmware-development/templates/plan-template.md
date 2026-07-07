# Implementation Plan: [FEATURE]

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]
**Milestone**: [M1/M2/M3/M4/M5]

## Summary

[Primary requirement + technical approach]

## Target Platform

**MCU**: [STM32H7 / RP2040 / other]
**Flash budget**: [KB] (cumulative from previous milestone)
**RAM budget**: [KB] (cumulative from previous milestone)
**Jitter target**: ≤ [us] p99 at [Hz] scan rate
**Language**: C++17/20 (-fno-exceptions -fno-rtti)

## Dependencies

| Dep | Source | Version |
|-----|--------|---------|
| FreeRTOS | SDK | 10.x+ LTS |
| LwIP | SDK | 2.2.x |
| muC/OPC-UA | git | [tag] |
| Rust toolchain | host | produce .qplcx envelope v[N] |

## Interface Contracts

Headers this milestone establishes:

| Header | Purpose | Consumed by |
|--------|---------|-------------|
| `[header.h]` | [description] | [downstream milestone] |

## Acceptance Criteria

| Criterion | Method | Target |
|-----------|--------|--------|
| [what] | [how to measure] | [pass/fail threshold] |
| Flash ≤ [N] KB | arm-none-eabi-size | ≤ [N] KB |
| RAM ≤ [N] KB | arm-none-eabi-size + runtime watermark | ≤ [N] KB |
| Jitter ≤ [N] us p99 | DWT cycle counter histogram | ≤ [N] us |

## ADR References

| ADR | Title | Relevance |
|-----|-------|-----------|
| ADR-00XX | [title] | [how it constrains this milestone] |

## Risk Register

| # | Risk | Likelihood | Impact | Mitigation |
|---|------|------------|--------|------------|
| 1 | [risk description] | High/Med/Low | High/Med/Low | [mitigation plan] |

## SWOT

| | Positive | Negative |
|---|---|---|
| **Internal** | Strengths: [..] | Weaknesses: [..] |
| **External** | Opportunities: [..] | Threats: [..] |

## Effort Estimate

**Estimated**: [N] person-weeks (AI-assisted)

## Constitution Check

*GATE: Must pass before implementation.*

[Constitution gates relevant to this milestone]
