#!/usr/bin/env bash
# read-approval-state.sh — Read approval state for a feature
# Usage: read-approval-state.sh <feature-name>
# Output: JSON to stdout
# Uses SPEC_VALIDATE_STATE_DIR env var if set, otherwise default path

set -e

FEATURE="${1:-}"

if [ -z "$FEATURE" ]; then
  echo "Error: No feature name provided. Usage: read-approval-state.sh <feature-name>" >&2
  exit 1
fi

# Determine state directory
if [ -n "$SPEC_VALIDATE_STATE_DIR" ]; then
  STATE_DIR="$SPEC_VALIDATE_STATE_DIR"
else
  # Find project root
  _dir="$(pwd)"
  while [ "$_dir" != "/" ]; do
    if [ -d "$_dir/.specify" ]; then
      break
    fi
    _dir="$(dirname "$_dir")"
  done
  STATE_DIR="$_dir/.specify/extensions/spec-validate/status"
fi

STATE_FILE="$STATE_DIR/${FEATURE}.json"

if [ ! -f "$STATE_FILE" ]; then
  # Return default state
  cat <<EOF
{
  "feature": "$FEATURE",
  "spec_hash": null,
  "tasks_hash": null,
  "spec_self_validation": "not-run",
  "tasks_self_validation": "not-run",
  "spec_validated_at": null,
  "tasks_validated_at": null,
  "spec_critical_count": 0,
  "spec_missed_items": [],
  "review_status": "not-required",
  "review_requested_at": null,
  "reviewer": null,
  "review_comments": [],
  "approval_status": "blocked",
  "warnings": [],
  "override": {"used": false, "reason": null, "by": null},
  "timeout_self_approval": {"used": false, "reason": null}
}
EOF
  exit 0
fi

# Validate JSON before outputting
if command -v python3 >/dev/null 2>&1; then
  if ! python3 -c "import json; json.load(open('$STATE_FILE'))" 2>/dev/null; then
    echo "Error: Malformed JSON in state file: $STATE_FILE" >&2
    exit 1
  fi
elif command -v jq >/dev/null 2>&1; then
  if ! jq empty "$STATE_FILE" 2>/dev/null; then
    echo "Error: Malformed JSON in state file: $STATE_FILE" >&2
    exit 1
  fi
fi

cat "$STATE_FILE"
