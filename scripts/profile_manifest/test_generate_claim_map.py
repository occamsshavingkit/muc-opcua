from __future__ import annotations

import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from generate import generate_claim_map  # noqa: E402  # pylint: disable=wrong-import-position


class ClaimMapGenerationTest(unittest.TestCase):
    def test_equal_facet_and_cu_names_are_not_duplicated(self) -> None:
        # Given one claimed item whose facet and CU labels are identical.
        manifest = {
            "items": [{
                "id": "subscription_standard",
                "implementation_state": "claimed",
                "opc_reference": {
                    "spec": "OPC-10000-7",
                    "section": "5.3.4",
                    "facet": "Subscription Standard",
                    "cu_name": "Subscription Standard",
                },
                "profile_defaults": {
                    "nano": False,
                    "micro": False,
                    "embedded": False,
                    "standard": True,
                    "full": True,
                },
                "backing_tests": ["test_subscription_standard"],
            }],
        }

        # When the claim map is generated.
        claim_map = generate_claim_map(manifest)

        # Then its row contains one canonical label rather than facet: CU duplication.
        self.assertIn(
            "| Subscription Standard | OPC-10000-7 §5.3.4 | standard, full | "
            "test_subscription_standard |",
            claim_map,
        )
        self.assertNotIn("Subscription Standard: Subscription Standard", claim_map)


if __name__ == "__main__":
    unittest.main()
