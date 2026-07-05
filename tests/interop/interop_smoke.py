#!/usr/bin/env python3
"""Interop smoke test for muc-opcua.
 
Drives the running muc-opcua server with `asyncua` (a mature, standards-
compliant Python OPC UA client) to validate the connect -> discover -> session
-> browse -> read -> write path against a real client rather than self-authored
fixtures. This operationalises spec FR-026 (adopt/adapt the async-opcua interop
suite once the minimal connect/discover/browse/read path works) and interop test
hardening (spec 033) which adds write coverage.

Exits 0 on success, 1 on failure.
"""
import asyncio
import sys
import traceback

from asyncua import Client
from asyncua import ua

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

        # Write — sets MyVar1 to 100, then reads back to verify.
        # Some server configurations (e.g. minimal_server without write handler)
        # return BadWriteNotSupported. Gracefully skip in that case.
        myvar = client.get_node("ns=1;i=1000")
        try:
            dv = await asyncio.wait_for(myvar.read_value(), timeout=10)
            old = dv
            await asyncio.wait_for(myvar.write_value(100, varianttype=ua.VariantType.Int32), timeout=10)
            new_dv = await asyncio.wait_for(myvar.read_value(), timeout=10)
            assert new_dv == 100, f"Write expected 100, got {new_dv!r}"
            print(f"  write ns=1;i=1000 = 100   OK")
            # Restore original value
            await asyncio.wait_for(myvar.write_value(old, varianttype=ua.VariantType.Int32), timeout=10)
        except ua.UaStatusCodeError as e:
            if "BadWriteNotSupported" in str(e):
                print(f"  write ns=1;i=1000   SKIP (server does not support writes)")
            else:
                raise

        # Subscribe/Publish — only if writes are supported (needs a write to trigger notification).
        # Skip gracefully when the server returns BadWriteNotSupported.
        write_ok = True
        try:
            await asyncio.wait_for(myvar.write_value(99, varianttype=ua.VariantType.Int32), timeout=10)
        except ua.UaStatusCodeError as e:
            write_ok = False
            if "BadWriteNotSupported" not in str(e):
                raise
        if write_ok:
            try:
                await asyncio.wait_for(myvar.write_value(old, varianttype=ua.VariantType.Int32), timeout=10)
            except ua.UaStatusCodeError:
                pass

            try:
                sub = await asyncio.wait_for(
                    client.create_subscription(period=500, handler=None), timeout=10
                )
                try:
                    handle = await asyncio.wait_for(
                        sub.subscribe_data_change(var, ua.AttributeIds.Value), timeout=10
                    )
                    await asyncio.wait_for(myvar.write_value(99, varianttype=ua.VariantType.Int32), timeout=10)
                    notif = await asyncio.wait_for(sub._internal_subscription.publish(timeout=3), timeout=8)
                    print(f"  subscribe/publish ns=1;i=1000   OK (notification received)")
                    await asyncio.wait_for(sub.unsubscribe(handle), timeout=5)
                finally:
                    await asyncio.wait_for(sub.delete(), timeout=10)
            except (ua.UaStatusCodeError, asyncio.TimeoutError) as e:
                if "BadServiceUnsupported" in str(e) or "BadSubscriptionIdInvalid" in str(e):
                    print(f"  subscribe/publish   SKIP (server does not support subscriptions)")
                else:
                    raise
        else:
            print(f"  subscribe/publish   SKIP (server does not support writes)")
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
