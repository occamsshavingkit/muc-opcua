# Interop Validation (asyncua)

Validates micro-opcua against a real, standards-compliant OPC UA client instead
of self-authored fixtures. This operationalises **FR-026**: adopt/adapt the
async-opcua interop suite once the minimal connect/discover/browse/read path
works (it now does).

## Harness

- `tests/interop/interop_smoke.py` — an [`asyncua`](https://github.com/FreeOpcUa/opcua-asyncio)
  client that connects, reads `ns=1;i=1000` (MyVar1 = 42), and browses the
  Objects folder (`i=85`) expecting MyVar1 as a child.
- `tests/interop/run_interop.sh` — starts the host `minimal_server`, waits for
  the port, runs the smoke test, and tears the server down.

The client's `connect()` exercises HEL/ACK, OpenSecureChannel, GetEndpoints,
CreateSession and ActivateSession; the reads/browse exercise the Attribute and
View service sets. So a single run covers the whole Nano-profile surface.

## Running

```sh
cmake -S . -B build/host -DMICRO_OPCUA_BUILD_EXAMPLES=ON -DMICRO_OPCUA_PLATFORM=host
cmake --build build/host --target minimal_server
pip install asyncua            # or use the devcontainer, which installs it
tests/interop/run_interop.sh
```

Expected output:

```
  read  ns=1;i=1000 (MyVar1) = 42   OK
  browse i=85 children = ['ns=1;i=1000']   OK
INTEROP PASS
```

CI runs this on every push (the `interop` job installs asyncua in a venv); the
devcontainer installs asyncua so Codespaces can run it too.

## Findings

| Finding | Status |
|---------|--------|
| asyncua completes connect → GetEndpoints → CreateSession → ActivateSession against the wired server | ✅ passes |
| `Read` of MyVar1 returns Int32 42 | ✅ passes |
| **Browse `HierarchicalReferences` (i=33) with `includeSubtypes=true` returned no children** — the Organizes (i=35) reference is a *subtype* and was filtered out by exact match | ✅ fixed: `mu_browse_process` now resolves the standard ReferenceType subtype hierarchy |

The browse subtype gap is exactly the kind of issue self-authored fixtures
missed (the earlier dispatch/browse tests browsed with "any" reference type) and
a real client surfaced immediately.

## async-opcua suite inventory (FR-026)

See `docs/conformance/async-opcua-inventory.md` for the upstream inventory. This
first harness adapts the **client-driven smoke** portion (asyncua). Still
deferred until needed: the broader codegen-tests, dotnet-tests, and an OPC
Foundation Compliance Test Tool run. No formal conformance claim is made on the
strength of this smoke test; see `docs/conformance/status.md` for the current
status and its evidence.
