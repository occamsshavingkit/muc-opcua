#!/usr/bin/env bash
set -e
# Dummy script for size measurement
echo "Measuring size..."
# Assuming we have a built target
size build/minimal_server || true
