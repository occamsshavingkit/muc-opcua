#!/usr/bin/env bash
# Run the asyncua interop smoke test against the host minimal_server example.
#
# Usage: tests/interop/run_interop.sh [path-to-server-binary]
# Requires: a built host minimal_server, python3 with `asyncua` installed.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SERVER="${1:-$ROOT/build/host/examples/minimal_server}"

if [ ! -x "$SERVER" ]; then
    echo "Server binary not found: $SERVER" >&2
    echo "Build it first:" >&2
    echo "  cmake -S . -B build/host -DMICRO_OPCUA_BUILD_EXAMPLES=ON -DMICRO_OPCUA_PLATFORM=host" >&2
    echo "  cmake --build build/host --target minimal_server" >&2
    exit 2
fi

"$SERVER" >/tmp/mu_interop_server.log 2>&1 &
SRV=$!
trap 'kill "$SRV" 2>/dev/null' EXIT

# Wait (up to ~5s) for the server to accept connections on 4840.
for _ in $(seq 1 50); do
    if python3 -c "import socket,sys; s=socket.socket(); s.settimeout(0.2); sys.exit(0 if s.connect_ex(('127.0.0.1',4840))==0 else 1)" 2>/dev/null; then
        break
    fi
    sleep 0.1
done

echo "Running interop smoke test against $SERVER ..."
python3 "$HERE/interop_smoke.py" "opc.tcp://localhost:4840"
