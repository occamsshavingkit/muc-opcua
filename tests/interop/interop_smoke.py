#!/usr/bin/env python3
"""Interop smoke test for muc-opcua.

Drives the running muc-opcua server with `asyncua` (a mature, standards-
compliant Python OPC UA client) to validate the connect -> discover -> session
-> browse -> read path against a real client rather than self-authored fixtures.
This operationalises spec FR-026 (adopt/adapt the async-opcua interop suite once
the minimal connect/discover/browse/read path works).

Exits 0 on success, 1 on failure.
"""
import asyncio
import sys
import traceback

from asyncua import Client

URL = sys.argv[1] if len(sys.argv) > 1 else "opc.tcp://localhost:4840"


async def main() -> None:
    client = Client(URL)
    # client.connect() exercises HEL/ACK, OpenSecureChannel, GetEndpoints,
    # CreateSession and ActivateSession.
    await asyncio.wait_for(client.connect(), timeout=15)
    try:
        var = client.get_node("ns=1;i=1000")
        value = await asyncio.wait_for(var.read_value(), timeout=10)
        assert value == 42, f"MyVar1 expected 42, got {value!r}"
        print(f"  read  ns=1;i=1000 (MyVar1) = {value}   OK")

        objects = client.get_node("i=85")
        children = await asyncio.wait_for(objects.get_children(), timeout=10)
        ids = [c.nodeid.to_string() for c in children]
        assert any("i=1000" in i for i in ids), f"MyVar1 not found under Objects: {ids}"
        print(f"  browse i=85 children = {ids}   OK")
    finally:
        await client.disconnect()


if __name__ == "__main__":
    try:
        asyncio.run(main())
        print("INTEROP PASS")
    except Exception as exc:  # noqa: BLE001 - smoke test reports any failure
        print("INTEROP FAIL:", repr(exc))
        traceback.print_exc()
        sys.exit(1)
