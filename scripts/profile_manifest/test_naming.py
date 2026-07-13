#!/usr/bin/env python3
"""Unit tests for the Kconfig symbol naming algorithm in generate.py."""

import os
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
if _HERE not in sys.path:
    sys.path.insert(0, _HERE)

from generate import compute_kconfig_symbol  # noqa: E402


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
