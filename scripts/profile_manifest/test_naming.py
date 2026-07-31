#!/usr/bin/env python3
"""Unit tests for the Kconfig symbol naming algorithm in generate.py."""

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from generate import (  # noqa: E402
    _emit_selectable,
    compute_kconfig_symbol,
    generate_kconfig,
)


def test_core_2017_server_facet_symbol():
    assert compute_kconfig_symbol("Core 2017 Server Facet", "facet") == (
        "MUC_OPCUA_FACET_CORE_2017_SERVER"
    )


def test_attribute_read_cu_symbol():
    assert compute_kconfig_symbol("Attribute Read", "conformance_unit") == (
        "MUC_OPCUA_CU_ATTRIBUTE_READ"
    )


def test_embedded_2017_ua_server_profile_symbol():
    assert compute_kconfig_symbol(
        "Embedded 2017 UA Server Profile", "profile"
    ) == "MUC_OPCUA_PROFILE_EMBEDDED_2017_UA_SERVER"


def test_emit_selectable_separates_facet_and_semantic_dependencies():
    # Given a conformance unit with graph facet membership and semantic prerequisites.
    item = {
        "kind": "conformance_unit",
        "kconfig_symbol": "CU_MAIN",
        "implementation_state": "implemented",
        "depends_on": ["FACET_A", "FACET_B"],
        "depends_on_op": "or",
        "semantic_depends_on": ["BASE_WRITE"],
    }

    # When the real selectable fragment is emitted without profile defaults.
    emitted = []
    _emit_selectable(emitted, item, {})

    # Then graph membership and semantic prerequisites form separate groups.
    depends_line = next(line for line in emitted if line.startswith("\tdepends on "))
    assert depends_line == "\tdepends on (FACET_A || FACET_B) && BASE_WRITE"


def test_generate_kconfig_emits_unimplemented_conformance_unit_as_comment():
    # Given a minimal manifest containing an unimplemented conformance unit.
    display_name = "Unmistakable Incomplete Conformance Unit"
    symbol = "MUC_OPCUA_CU_UNMISTAKABLE_INCOMPLETE_CONFORMANCE_UNIT"
    manifest = {
        "items": [
            {
                "id": "incomplete_cu",
                "kind": "conformance_unit",
                "opc_display_name": display_name,
                "kconfig_symbol": symbol,
                "implementation_state": "unimplemented",
            }
        ],
        "facet_containment": {},
    }

    # When the real Kconfig generator renders the manifest.
    generated = generate_kconfig(manifest)

    # Then the item stays visible but cannot be selected.
    assert f'comment "{display_name} (NOT IMPLEMENTED)"' in generated
    assert symbol not in generated
