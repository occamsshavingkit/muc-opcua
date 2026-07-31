#!/usr/bin/env python3
"""Validation tests for resolved profile manifests."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import sys

_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(_ROOT / "scripts" / "profile_manifest"))

import graph_deps  # noqa: E402
import model  # noqa: E402


def _resolved_manifest() -> dict[str, object]:
    manifest_path = _ROOT / "profiles" / "opcua-profile-manifest.yaml"
    graph_path = _ROOT / "profiles" / "opcua-profile-graph.json"
    manifest = model.load_manifest(str(manifest_path))
    with graph_path.open(encoding="utf-8") as graph_file:
        graph = json.load(graph_file)
    graph_deps.resolve_into(manifest, graph)
    assert model.validate_manifest(manifest) == []
    return manifest


def _cu_item(manifest: dict[str, object]) -> dict[str, object]:
    items = manifest["items"]
    assert isinstance(items, list)
    item = next(item for item in items if item.get("id") == "opc_cu_2936")
    assert isinstance(item, dict)
    return item


def test_validate_manifest_rejects_non_list_semantic_depends_on() -> None:
    # Given a graph-resolved manifest with a malformed semantic prerequisite field.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)["semantic_depends_on"] = "MUC_OPCUA_CU_BASE_INFO_BASE_TYPES"

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the malformed field is identified on the mutated item.
    detail = "\n".join(errors)
    assert "opc_cu_2936" in detail
    assert "semantic_depends_on" in detail
    assert "list" in detail.lower()


def test_validate_manifest_rejects_unknown_semantic_dependency_symbol() -> None:
    # Given a graph-resolved manifest with an unknown semantic prerequisite symbol.
    manifest = copy.deepcopy(_resolved_manifest())
    unknown_symbol = "MUC_OPCUA_CU_DOES_NOT_EXIST"
    _cu_item(manifest)["semantic_depends_on"] = [unknown_symbol]

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the unknown symbol is identified on the mutated item.
    detail = "\n".join(errors)
    assert "opc_cu_2936" in detail
    assert "semantic_depends_on" in detail
    assert unknown_symbol in detail


def test_load_manifest_rejects_recursive_satisfied_by(tmp_path: Path) -> None:
    # Given a strict-JSON manifest with a nested satisfied_by middleman.
    manifest_path = tmp_path / "manifest.json"
    manifest_path.write_text(
        json.dumps({"items": [{"metadata": {"satisfied_by": "service_read"}}]}),
        encoding="utf-8",
    )

    # When the manifest is loaded.
    try:
        model.load_manifest(str(manifest_path))
    except ValueError as exc:
        detail = str(exc)
    else:
        raise AssertionError("load_manifest accepted a nested satisfied_by field")

    # Then the forbidden field and its location are reported.
    assert "satisfied_by" in detail
    assert "items[0].metadata.satisfied_by" in detail


def test_validate_manifest_rejects_duplicate_canonical_cu_id() -> None:
    # Given an otherwise valid CU alias with a unique item id and Kconfig symbol.
    manifest = copy.deepcopy(_resolved_manifest())
    duplicate = copy.deepcopy(_cu_item(manifest))
    duplicate["id"] = "opc_cu_2936_alias"
    duplicate["kconfig_symbol"] = "MUC_OPCUA_CU_ATTRIBUTE_WRITE_STATUSCODE_ALIAS"
    items = manifest["items"]
    assert isinstance(items, list)
    items.append(duplicate)
    facet_containment = manifest["facet_containment"]
    assert isinstance(facet_containment, dict)
    facet_containment["opc_facet_1322"].append(duplicate["id"])

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then both owners and the duplicated canonical CU id are reported.
    detail = "\n".join(errors)
    assert "2936" in detail
    assert "opc_cu_2936" in detail
    assert "opc_cu_2936_alias" in detail
