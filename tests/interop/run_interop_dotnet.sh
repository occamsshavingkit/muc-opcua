#!/usr/bin/env bash
# Run the OPC Foundation .NET reference client interop check against the host
# minimal_server. Requires the .NET 8 SDK (provisioned in CI / the devcontainer).
#
# Usage: tests/interop/run_interop_dotnet.sh [path-to-server-binary]
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SERVER="${1:-$ROOT/build/host/examples/minimal_server}"
PROJ="$HERE/dotnet"

if [ ! -x "$SERVER" ]; then
    echo "Server binary not found: $SERVER" >&2
    echo "Build it: cmake -S . -B build/host -DMUC_OPCUA_BUILD_EXAMPLES=ON && cmake --build build/host --target minimal_server" >&2
    exit 2
fi

# Build the reference client up front (keeps NuGet/build noise out of the run).
dotnet build "$PROJ/interop.csproj" -c Release -v minimal || exit 1

"$SERVER" >/tmp/mu_interop_dotnet_server.log 2>&1 &
SRV=$!
trap 'kill "$SRV" 2>/dev/null' EXIT

# Wait (up to ~5s) for the server to accept on 4840 (bash /dev/tcp, no extra deps).
for _ in $(seq 1 50); do
    if (exec 3<>/dev/tcp/127.0.0.1/4840) 2>/dev/null; then
        exec 3>&- 3<&-
        break
    fi
    sleep 0.1
done

echo "Running OPC Foundation .NET reference-client interop against $SERVER ..."
if ! dotnet run -c Release --no-build --project "$PROJ" -- opc.tcp://localhost:4840; then
    echo "--- server log ---" >&2
    cat /tmp/mu_interop_dotnet_server.log >&2 || true
    exit 1
fi
