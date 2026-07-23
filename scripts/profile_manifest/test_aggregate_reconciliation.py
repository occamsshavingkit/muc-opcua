#!/usr/bin/env python3
"""Aggregate CU reconciliation, anchored on the C implementation.

The OPC Server catalog names every aggregate function twice -- once for the
subscription facet (``Aggregate Subscription - <Fn>``, facet 1582) and once for
the historical facet (``Aggregate - <Fn>``, facet 1708). This module checks that
the manifest claims exactly the ones the server actually computes.

Non-circularity matters here, so the set of implemented functions is derived
from two independent artifacts and intersected:

  1. ``MU_ID_AGGREGATETYPE_*`` constants in ``include/muc_opcua/opcua_ids.h``
     -- a constant alone only proves a NodeId was written down;
  2. those same constants appearing in the dispatch in
     ``subscription_aggregate.c`` -- which proves a client asking for that
     aggregate gets it handled.

The OPC cu_id for each function comes from the catalog fetched from the OPC
Foundation REST API, never from the manifest. So the chain runs
C source -> OPC catalog -> manifest, and the manifest is only ever the thing
under test, never the source of truth.

Both directions are asserted. Claiming an aggregate the code does not compute
is the more dangerous failure, so it gets its own test.
"""

from __future__ import annotations

import collections
import importlib.util
import json
import pathlib
import re
import unittest

_HERE = pathlib.Path(__file__).resolve().parent
_REPO = _HERE.parents[1]

_IDS_H = _REPO / "include" / "muc_opcua" / "opcua_ids.h"
_DISPATCH_C = (
    _REPO
    / "src"
    / "cu"
    / "core_2022_server"
    / "subscription"
    / "standard"
    / "subscription_aggregate.c"
)
_CATALOG = _REPO / "profiles" / "opcua-server-conformance.json"
_MANIFEST = _REPO / "profiles" / "opcua-profile-manifest.yaml"

_CONST_RE = re.compile(r"MU_ID_AGGREGATETYPE_([A-Z0-9_]+)")
# OPC catalog display names use an EN DASH (U+2013), not a hyphen.
_CU_NAME_RE = re.compile(r"^(Aggregate(?: Subscription)?) – (.+)$")


def _load(name: str):
    path = _HERE / (name + ".py")
    spec = importlib.util.spec_from_file_location("profile_manifest_" + name, path)
    assert spec is not None and spec.loader is not None
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


completion = _load("completion")


def _opc_function_name(const_suffix: str) -> str:
    """``TIME_AVERAGE_2`` -> ``TimeAverage2`` (the OPC spelling).

    A mechanical transform rather than a hand-written lookup table: a table
    would just be this file restating the mapping it is supposed to check.
    """
    return "".join(part.capitalize() for part in const_suffix.split("_"))


def _implemented_functions() -> set[str]:
    """Aggregate functions the server actually computes (declared AND dispatched)."""
    declared = set(_CONST_RE.findall(_IDS_H.read_text(encoding="utf-8")))
    dispatched = set(_CONST_RE.findall(_DISPATCH_C.read_text(encoding="utf-8")))
    return {_opc_function_name(c) for c in declared & dispatched}


def _catalog_aggregate_cus() -> dict[str, list[str]]:
    """OPC function name -> cu_ids naming it, from the OPC Foundation catalog."""
    names = json.loads(_CATALOG.read_text(encoding="utf-8"))["conformance_unit_names"]
    out: dict[str, list[str]] = collections.defaultdict(list)
    for cid, nm in names.items():
        m = _CU_NAME_RE.match(nm or "")
        if m:
            out[m.group(2)].append(str(cid))
    return out


class AggregateCuReconciliationTest(unittest.TestCase):
    def setUp(self) -> None:
        self.implemented = _implemented_functions()
        self.catalog = _catalog_aggregate_cus()
        manifest = completion._load_yaml(_MANIFEST)
        items = manifest["items"]
        self._groups = completion._entries_by_cu_id(items)
        self._by_id = completion._index_by_id(items)

    def _is_implemented(self, cu_id: str) -> bool:
        return completion._cu_id_implemented(
            cu_id, self._groups.get(cu_id, []), self._by_id
        )

    def test_extraction_finds_the_expected_shape_of_data(self) -> None:
        """Guard against a silently vacuous suite.

        If a rename broke either regex, every other test here would pass by
        iterating an empty set, so pin the extraction itself first.
        """
        self.assertEqual(
            len(self.implemented),
            24,
            "expected 24 declared-and-dispatched aggregate functions, got "
            + repr(sorted(self.implemented)),
        )
        self.assertIn("TimeAverage2", self.implemented)
        self.assertIn("DurationInStateZero", self.implemented)
        self.assertGreater(len(self.catalog), 30)

    def test_every_implemented_aggregate_has_a_catalog_cu(self) -> None:
        orphans = sorted(fn for fn in self.implemented if fn not in self.catalog)
        self.assertEqual(
            orphans,
            [],
            "aggregate functions dispatched in C but absent from the OPC "
            "catalog (a bad MU_ID_AGGREGATETYPE_* name?): " + repr(orphans),
        )

    def test_implemented_aggregates_are_reconciled_in_the_manifest(self) -> None:
        missing = sorted(
            (fn, cid)
            for fn in self.implemented
            for cid in self.catalog.get(fn, [])
            if not self._is_implemented(cid)
        )
        self.assertEqual(
            missing,
            [],
            str(len(missing))
            + " aggregate CU(s) are computed by subscription_aggregate.c but not "
            "reconciled in the manifest: " + repr(missing),
        )

    def test_unimplemented_aggregates_are_not_claimed(self) -> None:
        """The honesty direction: never claim an aggregate the code lacks.

        13 aggregate functions (the ActualTime/Bound/StandardDeviation/Variance
        family, NumberOfTransitions, Range2, DurationInStateNonZero) have no
        MU_ID_AGGREGATETYPE_* constant and no dispatch branch at all, so their
        CUs must stay uncounted.
        """
        overclaimed = sorted(
            (fn, cid)
            for fn, cids in self.catalog.items()
            if fn not in self.implemented
            for cid in cids
            if self._is_implemented(cid)
        )
        self.assertEqual(
            overclaimed,
            [],
            "aggregate CU(s) claimed in the manifest with no implementation in "
            "subscription_aggregate.c: " + repr(overclaimed),
        )


if __name__ == "__main__":
    unittest.main()
