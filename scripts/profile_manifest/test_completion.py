#!/usr/bin/env python3
"""Fixture tests for profile_manifest.completion counting (spec 073).

These verify the completion arithmetic — closure denominator, required/optional
split from cu_optional, status resolution through reconciliation links, and N/A
exclusion — on a small known manifest, independent of the real data.
"""

from __future__ import annotations

import importlib.util
import pathlib
import unittest

_PATH = pathlib.Path(__file__).with_name("completion.py")
_SPEC = importlib.util.spec_from_file_location("profile_manifest_completion", _PATH)
assert _SPEC is not None and _SPEC.loader is not None
completion = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(completion)


def _cu(cid, *, state="unimplemented", optional=False, cu_id=None, satisfied_by=None, na=None):
    item = {
        "id": cid,
        "kind": "conformance_unit",
        "implementation_state": state,
        "opc_display_name": cid,
        "cu_optional": optional,
        "opc_reference": {"cu_id": cu_id if cu_id is not None else (cid[7:] if cid.startswith("opc_cu_") else None)},
    }
    if satisfied_by is not None:
        item["satisfied_by"] = satisfied_by
    if na is not None:
        item["not_applicable"] = na  # {profile_key: reason}
    return item


class TestCompletion(unittest.TestCase):
    def _fixture(self):
        # Closure for profile "p" = CUs 100(req),101(req),102(opt),103(opt,N/A),104(req)
        manifest = {
            "items": [
                _cu("opc_cu_100", state="claimed", optional=False),  # implemented directly
                _cu("opc_cu_101", optional=False, satisfied_by="alias_x"),  # implemented via alias
                _cu("opc_cu_102", state="implemented", optional=True),  # optional implemented
                _cu("opc_cu_103", optional=True, na={"p": "mooted by profile constraint"}),  # optional N/A
                _cu("opc_cu_104", optional=False),  # genuine required gap
                _cu("alias_x", state="claimed", optional=False, cu_id="9999"),
            ]
        }
        snapshot = {"relationships": {"transitive_cu_closure": {"55": [100, 101, 102, 103, 104]}}}
        profiles = {"p": {"key": "p", "opc_id": "55", "display": "P"}}
        return manifest, snapshot, profiles

    def test_required_optional_counts_with_alias_and_na(self):
        manifest, snapshot, profiles = self._fixture()
        r = completion.compute_profile_completion(manifest, snapshot, profiles["p"])
        # required in closure: 100,101,104 (103 is optional; not required). 100 direct, 101 via alias -> 2/3.
        self.assertEqual((r["required_implemented"], r["required_total"]), (2, 3))
        # optional in closure: 102,103. 103 is N/A -> excluded. 102 implemented -> 1/1.
        self.assertEqual((r["optional_implemented"], r["optional_total"]), (1, 1))
        # N/A reported separately
        self.assertEqual([c["id"] for c in r["not_applicable"]], ["opc_cu_103"])
        # required gap surfaced
        self.assertIn("opc_cu_104", [c["id"] for c in r["required_gaps"]])

    def test_closure_cu_missing_from_manifest_is_flagged(self):
        manifest, snapshot, profiles = self._fixture()
        snapshot["relationships"]["transitive_cu_closure"]["55"].append(200)  # not in manifest
        r = completion.compute_profile_completion(manifest, snapshot, profiles["p"])
        self.assertIn("200", [str(x) for x in r["missing_from_manifest"]])

    def test_alias_resolution_requires_implemented_alias(self):
        manifest, snapshot, profiles = self._fixture()
        # break the alias's status -> 101 no longer counts
        for it in manifest["items"]:
            if it["id"] == "alias_x":
                it["implementation_state"] = "unimplemented"
        r = completion.compute_profile_completion(manifest, snapshot, profiles["p"])
        self.assertEqual((r["required_implemented"], r["required_total"]), (1, 3))

    def test_shared_cu_id_aggregation(self):
        # Canonical placeholder (unimplemented) + impl entry share cu_id 300;
        # the cu_id must count implemented via aggregation, no satisfied_by needed.
        manifest = {"items": [
            _cu("opc_cu_300", state="unimplemented", optional=False),
            _cu("opc_cu_core_thing", state="claimed", optional=False, cu_id="300"),
        ]}
        snapshot = {"relationships": {"transitive_cu_closure": {"55": [300]}}}
        r = completion.compute_profile_completion(manifest, snapshot, {"key": "p", "opc_id": "55"})
        self.assertEqual((r["required_implemented"], r["required_total"]), (1, 1))

    def test_catalog_completion_uses_catalog_optionality(self):
        # Optionality comes from the catalog map, not manifest cu_optional;
        # status is capability (manifest, via cu_id/aggregate/satisfied_by).
        manifest = {"items": [
            _cu("opc_cu_700", state="claimed"),          # implemented
            _cu("opc_cu_701", state="unimplemented"),    # not implemented
            _cu("opc_cu_x", state="claimed", cu_id="702"),  # implements cu 702 via shared cu_id
        ]}
        opt = {"700": False, "701": True, "702": False, "703": True}  # 703 not in manifest
        r = completion.compute_catalog_completion(manifest, ["700", "701", "702", "703"], opt)
        # required: 700 (impl), 702 (impl via alias) -> 2/2
        self.assertEqual((r["required_implemented"], r["required_total"]), (2, 2))
        # optional: 701 (not impl), 703 (not in manifest) -> 0/2
        self.assertEqual((r["optional_implemented"], r["optional_total"]), (0, 2))

    def test_facet_completion_uses_included_membership(self):
        manifest = {"items": [
            _cu("opc_cu_400", state="claimed", optional=False),
            _cu("opc_cu_401", state="unimplemented", optional=True),
        ]}
        snapshot = {"relationships": {
            "transitive_cu_closure": {},
            "included_conformance_units": {"1322": [400, 401]},
        }}
        r = completion.compute_profile_completion(
            manifest, snapshot, {"key": None, "opc_id": "1322", "rel": "included_conformance_units"})
        self.assertEqual((r["required_implemented"], r["required_total"]), (1, 1))
        self.assertEqual((r["optional_implemented"], r["optional_total"]), (0, 1))


if __name__ == "__main__":
    unittest.main()
