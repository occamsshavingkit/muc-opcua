#!/usr/bin/env python3
"""Validation tests for resolved profile manifests."""

from __future__ import annotations

import copy
import json
from pathlib import Path
import sys

import pytest

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


def test_validate_manifest_rejects_non_mapping_project_profile_defaults() -> None:
    # Given a graph-resolved manifest with a non-mapping project overlay.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)["project_profile_defaults"] = ["standard"]

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the malformed overlay type is rejected.
    detail = "\n".join(errors)
    assert "opc_cu_2936" in detail
    assert "project_profile_defaults" in detail
    assert "object" in detail


def test_validate_manifest_rejects_unknown_project_profile_default() -> None:
    # Given a graph-resolved manifest with an unknown project profile key.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)["project_profile_defaults"] = {"enterprise": True}

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the unknown key is rejected.
    detail = "\n".join(errors)
    assert "opc_cu_2936" in detail
    assert "project_profile_defaults" in detail
    assert "enterprise" in detail


@pytest.mark.parametrize("invalid_value", [False, 0, 1, "true", None])
def test_validate_manifest_rejects_project_profile_default_values_other_than_true(
    invalid_value: bool | int | str | None,
) -> None:
    # Given a graph-resolved manifest with a subtractive or non-boolean overlay value.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)["project_profile_defaults"] = {"standard": invalid_value}

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then only the exact boolean value true is accepted.
    detail = "\n".join(errors)
    assert "opc_cu_2936" in detail
    assert "project_profile_defaults" in detail
    assert "standard" in detail
    assert "true" in detail.lower()


@pytest.mark.parametrize(
    ("field_name", "invalid_value", "expected_fragment"),
    [
        ("required_for_profile", ["standard"], "object"),
        ("required_for_profile", {"enterprise": True}, "enterprise"),
        ("project_required_for_profile", ["standard"], "object"),
        ("project_required_for_profile", {"enterprise": True}, "enterprise"),
    ],
)
def test_validate_manifest_rejects_malformed_sparse_profile_requirement_maps(
    field_name: str,
    invalid_value: list[str] | dict[str, bool],
    expected_fragment: str,
) -> None:
    # Given a resolved manifest with a malformed graph or project requirement map.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)[field_name] = invalid_value

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the sparse true-only requirement contract identifies the field and error.
    detail = "\n".join(errors)
    assert field_name in detail
    assert expected_fragment in detail.lower()


@pytest.mark.parametrize(
    "field_name", ["required_for_profile", "project_required_for_profile"]
)
@pytest.mark.parametrize("invalid_value", [False, 0, 1, "true", None])
def test_validate_manifest_rejects_profile_requirement_values_other_than_true(
    field_name: str,
    invalid_value: bool | int | str | None,
) -> None:
    # Given a sparse requirement map containing a non-true value.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)[field_name] = {"standard": invalid_value}

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the exact-true requirement contract identifies the field and profile.
    detail = "\n".join(errors)
    assert field_name in detail
    assert "standard" in detail
    assert "true" in detail.lower()


@pytest.mark.parametrize(
    "field_name", ["required_for_profile", "project_required_for_profile"]
)
def test_validate_manifest_accepts_exact_true_profile_requirement(
    field_name: str,
) -> None:
    # Given a sparse requirement map containing the exact boolean value true.
    manifest = copy.deepcopy(_resolved_manifest())
    _cu_item(manifest)[field_name] = {"standard": True}

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the requirement value is accepted.
    assert errors == []


def test_validate_manifest_rejects_non_list_advertised_profile_markers() -> None:
    # Given an otherwise valid manifest with a non-list marker collection.
    manifest = copy.deepcopy(_resolved_manifest())
    manifest["advertised_profile_markers"] = {"id": "INVALID"}

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the marker collection shape is rejected.
    detail = "\n".join(errors)
    assert "advertised_profile_markers" in detail
    assert "list" in detail.lower()


def test_validate_manifest_rejects_legacy_marker_condition_field() -> None:
    # Given a marker using the removed default_if compatibility field.
    manifest = copy.deepcopy(_resolved_manifest())
    markers = manifest["advertised_profile_markers"]
    assert isinstance(markers, list)
    marker = markers[0]
    assert isinstance(marker, dict)
    marker["default_if"] = ["MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER"]

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the unknown compatibility field is rejected.
    detail = "\n".join(errors)
    assert "advertised_profile_markers[0]" in detail
    assert "default_if" in detail


@pytest.mark.parametrize(
    ("field_name", "invalid_value", "expected_fragment"),
    [
        ("id", "", "non-empty string"),
        ("id", "INVALID MARKER", "valid kconfig symbol"),
        ("required_profile", "", "non-empty string"),
        ("required_profile", "enterprise", "unknown profile"),
        ("note", 1, "string"),
    ],
)
def test_validate_manifest_rejects_invalid_advertised_profile_marker_fields(
    field_name: str,
    invalid_value: str | int,
    expected_fragment: str,
) -> None:
    # Given a marker with one malformed strict-schema field.
    manifest = copy.deepcopy(_resolved_manifest())
    markers = manifest["advertised_profile_markers"]
    assert isinstance(markers, list)
    marker = markers[0]
    assert isinstance(marker, dict)
    marker[field_name] = invalid_value

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then the field-specific marker error is reported.
    detail = "\n".join(errors)
    assert "advertised_profile_markers[0]" in detail
    assert field_name in detail
    assert expected_fragment in detail.lower()


@pytest.mark.parametrize("owner", ["item", "capacity"])
def test_validate_manifest_rejects_marker_kconfig_symbol_collision(
    owner: str,
) -> None:
    # Given a marker identifier that is already emitted by another manifest entry.
    manifest = copy.deepcopy(_resolved_manifest())
    markers = manifest["advertised_profile_markers"]
    assert isinstance(markers, list)
    marker = markers[0]
    assert isinstance(marker, dict)

    entries = manifest["items" if owner == "item" else "capacities"]
    assert isinstance(entries, list)
    conflicting_entry = entries[0]
    assert isinstance(conflicting_entry, dict)
    marker["id"] = conflicting_entry["kconfig_symbol"]

    # When the manifest is validated.
    errors = model.validate_manifest(manifest)

    # Then generation is stopped before duplicate Kconfig declarations are emitted.
    detail = "\n".join(errors)
    assert "advertised_profile_markers[0]" in detail
    assert "kconfig symbol" in detail.lower()
    assert owner in detail.lower()


def test_load_manifest_rejects_recursive_satisfied_by(tmp_path: Path) -> None:
    # Given a strict-JSON manifest with a nested satisfied_by middleman.
    manifest_path = tmp_path / "manifest.json"
    manifest_path.write_text(
        json.dumps({"items": [{"metadata": {"satisfied_by": "service_read"}}]}),
        encoding="utf-8",
    )

    # When the manifest is loaded.
    detail = ""
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
