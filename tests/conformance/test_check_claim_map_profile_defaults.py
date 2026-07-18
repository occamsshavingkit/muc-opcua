#!/usr/bin/env python3
"""Unit-style coverage for in-scope CU profile-default claim-map guards."""

import unittest

import check_claim_map


class InScopeCuProfileDefaultsTest(unittest.TestCase):
    def test_accepts_expected_profile_defaults_for_in_scope_cus(self):
        rows = [
            (
                "opc_cu_2328 Discovery Get Endpoints",
                "OPC-10000-4 §5.5.4",
                {"nano", "micro", "embedded", "standard", "full"},
                ["test_discovery_endpoint"],
            ),
            (
                # Optional CU (see graph_deps.derive_profile_defaults / the
                # 2026-07-18 graph-driven Kconfig plan): default-off in
                # nano/micro/embedded/standard, on only in full.
                "opc_cu_2389 Attribute Write Values",
                "OPC-10000-4 §5.11.4",
                {"full"},
                ["test_write_service"],
            ),
        ]

        self.assertEqual(check_claim_map.find_profile_default_gaps(rows), [])

    def test_accepts_nano_default_cu_with_all_profiles_cell(self):
        row = check_claim_map._parse_row(
            "| opc_cu_2328 Discovery Get Endpoints | OPC-10000-4 §5.5.4 | "
            "all | test_discovery_endpoint |"
        )

        self.assertIsNotNone(row)
        self.assertEqual(check_claim_map.find_profile_default_gaps([row]), [])

    def test_rejects_optional_cu_claimed_for_nano(self):
        rows = [
            (
                "opc_cu_3192 Base Info Diagnostics",
                "OPC-10000-5 §6.3.3",
                {"nano", "micro", "embedded", "standard", "full"},
                ["test_diagnostics"],
            )
        ]

        gaps = check_claim_map.find_profile_default_gaps(rows)

        self.assertEqual(len(gaps), 1)
        self.assertIn("opc_cu_3192", gaps[0])
        self.assertIn("nano", gaps[0])

    def test_rejects_nano_default_cu_missing_from_nano(self):
        rows = [
            (
                "opc_cu_2352 Discovery Find Servers Self",
                "OPC-10000-4 §5.5.2",
                {"micro", "embedded", "standard", "full"},
                ["test_discovery_services"],
            )
        ]

        gaps = check_claim_map.find_profile_default_gaps(rows)

        self.assertEqual(len(gaps), 1)
        self.assertIn("opc_cu_2352", gaps[0])
        self.assertIn("nano", gaps[0])


if __name__ == "__main__":
    unittest.main()
