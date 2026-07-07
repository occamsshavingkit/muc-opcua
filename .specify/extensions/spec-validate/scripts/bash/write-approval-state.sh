#!/usr/bin/env bash
# write-approval-state.sh — Write approval state for a feature
# Usage: write-approval-state.sh <feature-name> <json-string>
# Uses SPEC_VALIDATE_STATE_DIR env var if set, otherwise default path

set -e

FEATURE="${1:-}"
JSON_DATA="${2:-}"

if [ -z "$FEATURE" ]; then
  echo "Error: No feature name provided. Usage: write-approval-state.sh <feature-name> <json-string>" >&2
  exit 1
fi

if [ -z "$JSON_DATA" ]; then
  echo "Error: No JSON data provided. Usage: write-approval-state.sh <feature-name> <json-string>" >&2
  exit 1
fi

# Validate JSON
if command -v python3 >/dev/null 2>&1; then
  if ! echo "$JSON_DATA" | python3 -c "import sys,json; d=json.load(sys.stdin); assert 'spec_self_validation' in d and 'approval_status' in d" 2>/dev/null; then
    echo "Error: Invalid JSON or missing required fields (spec_self_validation, approval_status)" >&2
    exit 1
  fi
elif command -v jq >/dev/null 2>&1; then
  if ! echo "$JSON_DATA" | jq -e '.spec_self_validation and .approval_status' >/dev/null 2>&1; then
    echo "Error: Invalid JSON or missing required fields (spec_self_validation, approval_status)" >&2
    exit 1
  fi
fi

# Determine state directory
if [ -n "$SPEC_VALIDATE_STATE_DIR" ]; then
  STATE_DIR="$SPEC_VALIDATE_STATE_DIR"
else
  _dir="$(pwd)"
  while [ "$_dir" != "/" ]; do
    if [ -d "$_dir/.specify" ]; then
      break
    fi
    _dir="$(dirname "$_dir")"
  done
  STATE_DIR="$_dir/.specify/extensions/spec-validate/status"
fi

# Create directory if needed
mkdir -p "$STATE_DIR"

STATE_FILE="$STATE_DIR/${FEATURE}.json"

# Write JSON, pretty-print if python3 available
if command -v python3 >/dev/null 2>&1; then
  echo "$JSON_DATA" | python3 -c "import sys,json; json.dump(json.load(sys.stdin),open('$STATE_FILE','w'),indent=2)" 2>/dev/null
else
  echo "$JSON_DATA" > "$STATE_FILE"
fi

echo "State written to $STATE_FILE" >&2
