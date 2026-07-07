#!/usr/bin/env bash
# compute-hash.sh — Compute SHA-256 hash of a file
# Usage: compute-hash.sh <file-path>
# Output: sha256:<hex-hash> to stdout
# Exit 1 with error message for missing/unreadable file

set -e

FILE_PATH="${1:-}"

if [ -z "$FILE_PATH" ]; then
  echo "Error: No file path provided. Usage: compute-hash.sh <file-path>" >&2
  exit 1
fi

if [ ! -f "$FILE_PATH" ]; then
  echo "Error: File not found or does not exist: $FILE_PATH" >&2
  exit 1
fi

if [ ! -r "$FILE_PATH" ]; then
  echo "Error: File is not readable: $FILE_PATH" >&2
  exit 1
fi

# Auto-detect hash command: sha256sum (Linux) or shasum (macOS)
if command -v sha256sum >/dev/null 2>&1; then
  HASH=$(sha256sum "$FILE_PATH" | awk '{print $1}')
elif command -v shasum >/dev/null 2>&1; then
  HASH=$(shasum -a 256 "$FILE_PATH" | awk '{print $1}')
else
  echo "Error: Neither sha256sum nor shasum found on this system" >&2
  exit 1
fi

echo "sha256:${HASH}"
