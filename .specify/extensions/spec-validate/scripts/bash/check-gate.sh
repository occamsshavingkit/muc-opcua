#!/usr/bin/env bash
# check-gate.sh — Evaluate implementation gate for a feature
# Usage: check-gate.sh <feature-name>
# Output: Gate result JSON to stdout per Contract 3
# Exit 0 for allowed/allowed-with-warning, exit 1 for blocked

set -e

FEATURE="${1:-}"

if [ -z "$FEATURE" ]; then
  echo "Error: No feature name provided. Usage: check-gate.sh <feature-name>" >&2
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
READ_SCRIPT="$SCRIPT_DIR/read-approval-state.sh"
HASH_SCRIPT="$SCRIPT_DIR/compute-hash.sh"

# Read current approval state
STATE=$(bash "$READ_SCRIPT" "$FEATURE" 2>/dev/null)
if [ $? -ne 0 ] || [ -z "$STATE" ]; then
  cat <<EOF
{
  "outcome": "blocked",
  "reason": "Failed to read approval state for feature: $FEATURE",
  "actions_required": ["Run /speckit.spec-validate.validate to validate spec.md"],
  "warnings": []
}
EOF
  exit 1
fi

# Extract fields using python3 or grep fallback
_get_field() {
  local field="$1"
  if command -v python3 >/dev/null 2>&1; then
    echo "$STATE" | python3 -c "import sys,json; print(json.load(sys.stdin).get('$field',''))" 2>/dev/null
  else
    echo "$STATE" | grep -o "\"$field\": *\"[^\"]*\"" | sed "s/\"$field\": *\"//" | sed 's/"$//'
  fi
}

_get_bool() {
  local field="$1"
  if command -v python3 >/dev/null 2>&1; then
    echo "$STATE" | python3 -c "import sys,json; print(str(json.load(sys.stdin).get('$field',False)).lower())" 2>/dev/null
  else
    echo "$STATE" | grep -o "\"$field\": *[a-z]*" | sed "s/\"$field\": *//"
  fi
}

SPEC_VALIDATION=$(_get_field "spec_self_validation")
TASKS_VALIDATION=$(_get_field "tasks_self_validation")
REVIEW_STATUS=$(_get_field "review_status")
STORED_SPEC_HASH=$(_get_field "spec_hash")
STORED_TASKS_HASH=$(_get_field "tasks_hash")

# Determine spec file location
if [ -n "$SPEC_VALIDATE_SPEC_DIR" ]; then
  SPEC_DIR="$SPEC_VALIDATE_SPEC_DIR"
else
  _dir="$(pwd)"
  while [ "$_dir" != "/" ]; do
    if [ -d "$_dir/.specify" ]; then break; fi
    _dir="$(dirname "$_dir")"
  done
  # Try to find feature spec directory
  if [ -f "$_dir/specs/$FEATURE/spec.md" ]; then
    SPEC_DIR="$_dir/specs/$FEATURE"
  else
    SPEC_DIR=""
  fi
fi

WARNINGS='[]'
ACTIONS='[]'
OUTCOME="blocked"
REASON=""

# Gate rule 1: No validation at all
if [ "$SPEC_VALIDATION" = "not-run" ]; then
  REASON="Spec has not been validated"
  ACTIONS='["Run /speckit.spec-validate.validate to validate spec.md"]'
  cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": $ACTIONS,
  "warnings": []
}
EOF
  exit 1
fi

# Gate rule 2: Validation failed
if [ "$SPEC_VALIDATION" = "failed" ]; then
  REASON="Spec validation failed"
  ACTIONS='["Re-run /speckit.spec-validate.validate"]'
  cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": $ACTIONS,
  "warnings": []
}
EOF
  exit 1
fi

# Gate rule 3: Check staleness (recompute hash if spec file exists)
SPEC_IS_STALE="false"
if [ -n "$SPEC_DIR" ] && [ -f "$SPEC_DIR/spec.md" ] && [ -n "$STORED_SPEC_HASH" ] && [ "$STORED_SPEC_HASH" != "null" ]; then
  CURRENT_HASH=$(bash "$HASH_SCRIPT" "$SPEC_DIR/spec.md" 2>/dev/null || echo "")
  if [ -n "$CURRENT_HASH" ] && [ "$CURRENT_HASH" != "$STORED_SPEC_HASH" ]; then
    SPEC_IS_STALE="true"
  fi
fi

# Gate rule 4: Reviewer requested changes
if [ "$REVIEW_STATUS" = "changes-requested" ]; then
  REASON="Reviewer requested changes — address feedback and re-validate"
  ACTIONS='["Address reviewer comments", "Re-run /speckit.spec-validate.validate"]'
  cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": $ACTIONS,
  "warnings": []
}
EOF
  exit 1
fi

# Gate rule 5: Stale but review approved — allow (low-friction tradeoff)
if [ "$SPEC_IS_STALE" = "true" ] && [ "$REVIEW_STATUS" = "approved" ]; then
  REASON="Self-validation is stale but peer review is approved — proceeding"
  cat <<EOF
{
  "outcome": "allowed",
  "reason": "$REASON",
  "actions_required": [],
  "warnings": ["Self-validation is stale (spec.md has changed since validation). Peer review covers the artifact."]
}
EOF
  exit 0
fi

# Gate rule 6: Stale without approved review — blocked
if [ "$SPEC_IS_STALE" = "true" ]; then
  REASON="Spec content has changed since validation (hash mismatch)"
  ACTIONS='["Re-run /speckit.spec-validate.validate with current spec.md"]'
  cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": $ACTIONS,
  "warnings": []
}
EOF
  exit 1
fi

# Gate rule 7: Review pending
if [ "$REVIEW_STATUS" = "pending" ]; then
  REASON="Peer review is pending — waiting for reviewer"
  ACTIONS='["Wait for reviewer to run /speckit.spec-validate.review", "Or use maintainer override"]'
  cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": $ACTIONS,
  "warnings": []
}
EOF
  exit 1
fi

# Gate rule 8: Timeout with self-approval — allowed with warning
if [ "$REVIEW_STATUS" = "timed-out" ]; then
  # Check if timeout self-approval was used
  if command -v python3 >/dev/null 2>&1; then
    TIMEOUT_USED=$(echo "$STATE" | python3 -c "import sys,json; d=json.load(sys.stdin); print(str(d.get('timeout_self_approval',{}).get('used',False)).lower())" 2>/dev/null)
  else
    TIMEOUT_USED="false"
  fi

  if [ "$TIMEOUT_USED" = "true" ]; then
    REASON="Review timed out — author self-approved with recorded reason"
    cat <<EOF
{
  "outcome": "allowed-with-warning",
  "reason": "$REASON",
  "actions_required": [],
  "warnings": ["Review timed out after 24h SLA. Author self-approved."]
}
EOF
    exit 0
  else
    REASON="Review timed out but no self-approval reason recorded"
    ACTIONS='["Record a self-approval reason to proceed"]'
    cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": $ACTIONS,
  "warnings": []
}
EOF
    exit 1
  fi
fi

# Gate rule 9: Override
if command -v python3 >/dev/null 2>&1; then
  OVERRIDE_USED=$(echo "$STATE" | python3 -c "import sys,json; print(str(json.load(sys.stdin).get('override',{}).get('used',False)).lower())" 2>/dev/null)
else
  OVERRIDE_USED="false"
fi

if [ "$OVERRIDE_USED" = "true" ]; then
  REASON="Maintainer override approved with recorded reason"
  cat <<EOF
{
  "outcome": "allowed",
  "reason": "$REASON",
  "actions_required": [],
  "warnings": ["Gate was overridden by maintainer"]
}
EOF
  exit 0
fi

# Gate rule 10: Validated + no review needed or review approved — allowed
if [ "$SPEC_VALIDATION" = "passed" ] && { [ "$REVIEW_STATUS" = "not-required" ] || [ "$REVIEW_STATUS" = "approved" ]; }; then
  REASON="Spec validated and review requirements met"
  cat <<EOF
{
  "outcome": "allowed",
  "reason": "$REASON",
  "actions_required": [],
  "warnings": []
}
EOF
  exit 0
fi

# Fallback: blocked
REASON="Gate check did not match any allow condition"
cat <<EOF
{
  "outcome": "blocked",
  "reason": "$REASON",
  "actions_required": ["Run /speckit.spec-validate.status to check current state"],
  "warnings": []
}
EOF
exit 1
