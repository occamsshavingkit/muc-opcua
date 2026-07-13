import os
import sys
import unittest

_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
_SCRIPTS = os.path.join(_ROOT, "scripts")
if _SCRIPTS not in sys.path:
    sys.path.insert(0, _SCRIPTS)

from profile_manifest import import_opc_profiles as importer


def profile(opc_id, name, uri, status):
    return {
        "id": opc_id,
        "name": name,
        "profileUri": uri,
        "releaseStatus": status,
        "description": name,
    }


class ImportOpcProfilesTests(unittest.TestCase):
    def test_release_status_name_maps_active_and_superseded(self):
        self.assertEqual(importer._release_status_name(3), "active")
        self.assertEqual(importer._release_status_name(4), "superseded")
        self.assertEqual(importer._release_status_name(99), "unknown_99")
        self.assertEqual(importer._release_status_name(None), "unknown")

    def test_is_server_record_filters_client_records(self):
        server = profile(
            2266,
            "Nano Embedded Device 2025 Server Profile",
            "http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2025",
            3,
        )
        client = profile(
            1,
            "Some Client Facet",
            "http://opcfoundation.org/UA-Profile/Client/SomeClientFacet",
            3,
        )
        self.assertTrue(importer._is_server_record(server))
        self.assertFalse(importer._is_server_record(client))

    def test_select_canonical_server_profiles_prefers_active_2025(self):
        records = [
            profile(
                1330,
                "Nano Embedded Device 2022 Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2022",
                4,
            ),
            profile(
                2255,
                "Micro Embedded Device 2022 Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/MicroEmbeddedDevice2022",
                4,
            ),
            profile(
                1332,
                "Embedded 2022 UA Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2022",
                4,
            ),
            profile(
                1333,
                "Standard 2022 UA Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/StandardUA2022",
                4,
            ),
            profile(
                2266,
                "Nano Embedded Device 2025 Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2025",
                3,
            ),
            profile(
                2267,
                "Micro Embedded Device 2025 Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/MicroEmbeddedDevice2025",
                3,
            ),
            profile(
                2268,
                "Embedded 2025 UA Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2025",
                3,
            ),
            profile(
                2269,
                "Standard 2025 UA Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/StandardUA2022",
                3,
            ),
        ]

        selected = importer._select_canonical_server_profiles(records)

        self.assertEqual(selected["nano"]["id"], 2266)
        self.assertEqual(selected["micro"]["id"], 2267)
        self.assertEqual(selected["embedded"]["id"], 2268)
        self.assertEqual(selected["standard"]["id"], 2269)

    def test_merge_normalized_snapshot_updates_profiles_without_promoting_claims(self):
        snapshot = {
            "canonical_profiles": {
                "standard": {
                    "opc_id": 2269,
                    "name": "Standard 2025 UA Server Profile",
                    "display_name": "Standard 2025 UA Server Profile",
                    "opc_profile_uri": "http://opcfoundation.org/UA-Profile/Server/StandardUA2022",
                    "release_status": 3,
                    "release_status_name": "active",
                }
            },
            "profiles": [],
            "facets": [
                {
                    "opc_id": 9100,
                    "name": "New Test Facet",
                    "release_status": 3,
                    "release_status_name": "active",
                    "description": "contains the new CU",
                }
            ],
            "conformance_units": [
                {
                    "opc_id": 9001,
                    "name": "Brand New CU",
                    "release_status": 3,
                    "release_status_name": "active",
                    "description": "not implemented here",
                }
            ],
            "relationships": {
                "included_profiles": {"2269": [9100]},
                "included_conformance_units": {"9100": [9001]},
                "transitive_cu_closure": {"2269": [9001]},
            },
            "minimum_profile_defaults": {"9001": "standard"},
        }
        manifest = {
            "schema_version": "1",
            "profiles": {
                "nano": {"id": "PROFILE_NANO", "display": "nano", "selectable": True},
                "micro": {"id": "PROFILE_MICRO", "display": "micro", "selectable": True},
                "embedded": {"id": "PROFILE_EMBEDDED", "display": "embedded", "selectable": True},
                "standard": {"id": "PROFILE_STANDARD", "display": "standard", "selectable": True},
                "full": {"id": "PROFILE_FULL", "display": "full", "selectable": True},
                "custom": {"id": "PROFILE_CUSTOM", "display": "custom", "selectable": True},
            },
            "items": [],
            "capacities": [],
        }

        merged, counts = importer._merge_normalized_snapshot_into_manifest(snapshot, manifest)

        self.assertEqual(
            merged["profiles"]["standard"]["opc_display_name"],
            "Standard 2025 UA Server Profile",
        )
        self.assertEqual(merged["profiles"]["standard"]["release_status_name"], "active")
        imported = [item for item in merged["items"] if item["id"] == "opc_cu_9001"]
        self.assertEqual(imported[0]["implementation_state"], "unimplemented")
        self.assertEqual(
            merged["facet_containment"]["opc_facet_9100"],
            ["opc_cu_9001"],
        )
        self.assertFalse(imported[0]["profile_defaults"]["nano"])
        self.assertFalse(imported[0]["profile_defaults"]["embedded"])
        self.assertTrue(imported[0]["profile_defaults"]["standard"])
        self.assertTrue(imported[0]["profile_defaults"]["full"])
        self.assertEqual(counts["conformance_units_added"], 1)

    def test_ambiguous_reachable_cu_under_two_valid_facets_is_not_imported(self):
        snapshot = {
            "canonical_profiles": {},
            "profiles": [],
            "facets": [
                {
                    "opc_id": 9100,
                    "name": "Facet A",
                    "release_status": 3,
                    "release_status_name": "active",
                    "description": "facet a",
                },
                {
                    "opc_id": 9101,
                    "name": "Facet B",
                    "release_status": 3,
                    "release_status_name": "active",
                    "description": "facet b",
                },
            ],
            "conformance_units": [
                {
                    "opc_id": 9001,
                    "name": "Ambiguous CU",
                    "release_status": 3,
                    "release_status_name": "active",
                    "description": "appears under two valid facets",
                }
            ],
            "relationships": {
                "included_profiles": {},
                "included_conformance_units": {
                    "9100": [9001],
                    "9101": [9001],
                },
                "transitive_cu_closure": {"9100": [9001], "9101": [9001]},
            },
            "minimum_profile_defaults": {"9001": "nano"},
        }
        manifest = {
            "schema_version": "1",
            "profiles": {
                "nano": {"id": "PROFILE_NANO", "display": "nano", "selectable": True},
                "micro": {"id": "PROFILE_MICRO", "display": "micro", "selectable": True},
                "embedded": {"id": "PROFILE_EMBEDDED", "display": "embedded", "selectable": True},
                "standard": {"id": "PROFILE_STANDARD", "display": "standard", "selectable": True},
                "full": {"id": "PROFILE_FULL", "display": "full", "selectable": True},
                "custom": {"id": "PROFILE_CUSTOM", "display": "custom", "selectable": True},
            },
            "items": [],
            "capacities": [],
        }

        merged, counts = importer._merge_normalized_snapshot_into_manifest(snapshot, manifest)

        cu_items = [item for item in merged["items"] if item["id"] == "opc_cu_9001"]
        self.assertEqual(cu_items, [])
        self.assertNotIn(
            "opc_cu_9001",
            merged.get("facet_containment", {}).get("opc_facet_9100", []),
        )
        self.assertNotIn(
            "opc_cu_9001",
            merged.get("facet_containment", {}).get("opc_facet_9101", []),
        )
        self.assertEqual(counts["conformance_units_added"], 0)

    def test_normalize_api_snapshot_records_relationships(self):
        profile_rows = [
            profile(
                2266,
                "Nano Embedded Device 2025 Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2025",
                3,
            ),
            profile(
                2267,
                "Micro Embedded Device 2025 Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/MicroEmbeddedDevice2025",
                3,
            ),
            profile(
                2268,
                "Embedded 2025 UA Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/EmbeddedUA2025",
                3,
            ),
            profile(
                2269,
                "Standard 2025 UA Server Profile",
                "http://opcfoundation.org/UA-Profile/Server/StandardUA2022",
                3,
            ),
            profile(
                1322,
                "Core 2022 Server Facet",
                "http://opcfoundation.org/UA-Profile/Server/Core2022Facet",
                3,
            ),
        ]
        cu_rows = [{"id": 100, "name": "Attribute Read", "releaseStatus": 3}]
        included_profiles = {"2266": [profile_rows[4]]}
        included_cus = {"1322": [cu_rows[0]]}

        normalized = importer._normalize_api_snapshot(
            profile_rows,
            cu_rows,
            included_profiles,
            included_cus,
        )

        self.assertEqual(normalized["canonical_profiles"]["nano"]["opc_id"], 2266)
        self.assertEqual(
            normalized["relationships"]["included_profiles"]["2266"],
            [1322],
        )
        self.assertEqual(
            normalized["relationships"]["included_conformance_units"]["1322"],
            [100],
        )
        self.assertEqual(normalized["facets"][0]["opc_id"], 1322)
        self.assertIn(100, normalized["relationships"]["transitive_cu_closure"]["2266"])
        self.assertEqual(normalized["minimum_profile_defaults"]["100"], "nano")


if __name__ == "__main__":
    unittest.main()
