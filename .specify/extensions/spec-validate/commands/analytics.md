# Comprehension Analytics

## Command: `speckit.spec-validate.analytics`

Display private and aggregated comprehension analytics. Personal metrics are visible only to the individual and maintainers. Team-facing views show aggregated metrics only.

## Execution Flow

### Step 1: Load Data

1. Read all local private state files from `.specify/extensions/spec-validate/local/*.private.json`
2. Read all git-tracked approval state files from `.specify/extensions/spec-validate/status/*.json`
3. If no data exists, report: "No validation data found. Run /speckit.spec-validate.validate first."

### Step 2: Compute Per-Person Metrics

For the current user (matched by `validated_by` field in private state):

| Metric | Computation |
|--------|-------------|
| First-attempt accuracy | Count of correct first attempts / total critical items across all features |
| Average attempts per critical item | Always 1 (no retries) — track miss rate instead |
| Most-missed section types | Group missed items by section, rank by frequency |
| Review trigger frequency | Count of features where review was required / total features validated |

### Step 3: Compute Per-Spec Metrics

Across all validators for each feature:

| Metric | Computation |
|--------|-------------|
| Frequently missed items | Items appearing in `spec_missed_items` across multiple validators |
| Ambiguity candidates | Items missed by > 40% of validators (threshold from `analytics.flag_ambiguous_threshold` in config) |
| Most-selected distractors | From private state attempts, group by `selected_option` for incorrect answers |
| Review bottleneck rate | Features where review timed out / total features requiring review |

### Step 4: Agent Segmentation

- Group analytics by `agent.name` and `agent.version` from private state
- Do NOT compare individuals across different agents without noting the segmentation
- Display agent distribution: how many validations per agent family

### Step 5: Display Report

```markdown
## Personal Summary

| Metric | Value |
|--------|-------|
| Features validated | <count> |
| Overall accuracy | <percentage>% |
| Most-missed section | <section-name> |
| Review trigger rate | <percentage>% |

## Spec Quality Report

### Ambiguity Candidates
| Feature | Item ID | Miss Rate | Section |
|---------|---------|-----------|---------|
| <feature> | <id> | <rate>% | <section> |

### Common Misconceptions
| Item | Most-Selected Wrong Answer | Times Selected |
|------|---------------------------|----------------|
| <item> | <distractor text> | <count> |

## Team Trends

| Metric | Value |
|--------|-------|
| Total validations | <count> |
| Avg team accuracy | <percentage>% |
| Review bottleneck rate | <percentage>% |
| Agents used | <list> |
```

## Privacy Rules

- **Personal metrics**: Visible only to the individual running the command and to maintainers
- **Team views**: Show ONLY aggregated metrics — no individual names attached to scores
- **Detailed history**: Retained only for the current feature (short-lived)
- **Git-tracked state**: Contains NO personal mistake data (wrong answers, distractor selections)
- When displaying team trends, use aggregate counts and percentages, never individual performance
