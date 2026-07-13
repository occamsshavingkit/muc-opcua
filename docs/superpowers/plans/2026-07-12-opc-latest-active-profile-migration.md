# OPC Latest Active Profile Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. Worker agents must not commit in this dirty worktree; the controller owns verification and commits with explicit pathspecs.

**Goal:** Migrate the canonical OPC profile manifest from 2017-era seed data to the latest active OPC UA 1.05 server profile surface, using active 2025 named profiles and keeping superseded 2022 profiles non-canonical.

**Architecture:** Extend the profile import pipeline so it normalizes raw OPC API catalog data plus relationship endpoints into a deterministic checked-in snapshot. Merge that normalized snapshot into `profiles/opcua-profile-manifest.yaml` without promoting claim state, then regenerate Kconfig and generated docs from the migrated manifest. The generator keeps the existing graph-derived Kconfig semantics: profile selection seeds defaults only, all Profile/Facet/CU menus stay globally editable, and `full` remains a project-internal convenience profile.

**Tech Stack:** Python 3 stdlib, JSON manifest stored as `.yaml`, vendored `kconfiglib`, CMake, Kconfig, bash, C11.

## Global Constraints

- Raw downloaded OPC API dumps and scrape playbooks must remain ignored and must not be committed.
- Canonical named profiles must track active 2025 server profiles where available.
- Superseded 2022 profiles must remain historical metadata and must not seed default selections.
- Existing claimed items may remain claimed only when backed by current implementation and tests.
- Newly imported API items must be `unimplemented` or `deferred` unless explicitly mapped to tested code.
- Claimed/implemented CUs must retain exact `opc_reference.spec` and `opc_reference.section` values.
- Profile selection remains a default preset only; it must not hide unrelated Profile/Facet/CU menus.
- Generated `.config`, `muc_opcua_config.cmake`, and `autoconf.h` must expose selected 2025 profile/facet/CU/capacity symbols.
- Validation must include `validate.py --all`, `test_profile_gating.sh`, standard/full CTest, and `git diff --check`.
- CLI modes must be mutually exclusive: `--dry-run`, `--write`, `--write-normalized-snapshot PATH`, and `--fetch-relationships`. Write/fetch modes must not be combined with `--dry-run`.
- Raw API ignore policy must be checked before any task fetches or writes ignored raw API files.

---

## Execution Amendment

The current branch has extensive staged and unstaged feature work. To avoid mixing unrelated work into task commits, implementer subagents must not run `git add` or `git commit`. Treat each task's commit step as a controller-only checkpoint: the controller reviews status, stages explicit pathspecs, and commits only intended migration files when safe.

## Task 0: Establish Baseline and Ignore Guard

**Files:**
- Read: `.gitignore`
- Read: `git status --short`

**Interfaces:**
- Consumes: existing `.gitignore` raw API ignore rule.
- Produces: verified raw API ignore guard before relationship fetch tasks.

- [ ] **Step 1: Record worktree state**

Run: `git status --short`

Expected: dirty worktree is visible and no worker-owned commit is attempted.

- [ ] **Step 2: Verify raw API path is ignored**

Run:

```bash
git check-ignore -v profiles/opcua-1.05-api/relationships/includedprofiles-2269.json
```

Expected: output references `.gitignore` entry `profiles/opcua-1.05-api/`.

- [ ] **Step 3: Apply no-worker-commit mode**

For every later task, skip task-local `git add` and `git commit` commands inside subagents. The controller will perform explicit pathspec commits after reviews.

## File Structure

- Modify `scripts/profile_manifest/import_opc_profiles.py`: add active/superseded classification, relationship normalization, deterministic normalized snapshot writing, and manifest merge for active 2025 named profiles.
- Create `scripts/profile_manifest/test_import_opc_profiles.py`: stdlib unit tests for profile selection, release-status mapping, relationship traversal, and claim-state preservation.
- Modify `scripts/profile_manifest/model.py`: validate optional OPC API metadata fields and reject selectable superseded canonical profiles.
- Modify `scripts/profile_manifest/validate.py`: add generated-output checks for 2025 profile symbols and absence of selectable 2022 profile-choice defaults.
- Modify `scripts/profile_manifest/generate.py`: only if needed to render superseded imported records as comments and preserve 2025 display names/URIs exactly.
- Modify `profiles/opcua-profile-manifest.yaml`: migrate canonical profile entries, item metadata, profile defaults, and imported unimplemented records.
- Create or update `profiles/opcua-profile-normalized-snapshot.json`: checked-in deterministic normalized subset used by the import/merge tool.
- Regenerate `Kconfig`, `configs/*.defconfig`, `include/muc_opcua/capacities.h`, `tests/conformance/claim_test_map.md`, `docs/conformance/opc-profile-roadmap.md`, and `docs/build-and-gating.md`.

## Task 1: Add Importer Unit Tests for Active Profile Selection

**Files:**
- Create: `scripts/profile_manifest/test_import_opc_profiles.py`
- Modify later: `scripts/profile_manifest/import_opc_profiles.py`

**Interfaces:**
- Consumes: no new production functions yet; tests define required signatures.
- Produces expected functions:
  - `_release_status_name(status: int | None) -> str`
  - `_is_server_record(record: dict) -> bool`
  - `_select_canonical_server_profiles(records: list[dict]) -> dict[str, dict]`

**OPC grounding:** OPC-10000-7 §4.3 defines Profiles as named aggregations; §4.7 defines year-based profile versioning. OPC API `releaseStatus` is OPC Foundation API metadata, not a normative OPC UA section.

- [ ] **Step 1: Write the failing tests**

Create `scripts/profile_manifest/test_import_opc_profiles.py` with this content:

```python
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


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run tests and confirm they fail**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles -v`

Expected: failure or error mentioning missing `_release_status_name`, `_is_server_record`, or `_select_canonical_server_profiles`.

- [ ] **Step 3: Implement the minimal importer helpers**

In `scripts/profile_manifest/import_opc_profiles.py`, add these functions after `_snapshot_id()`:

```python
def _release_status_name(status: int | None) -> str:
    if status == 3:
        return "active"
    if status == 4:
        return "superseded"
    if status is None:
        return "unknown"
    return "unknown_" + str(status)


def _is_server_record(record: dict) -> bool:
    uri = record.get("profileUri") or record.get("opc_uri") or ""
    return isinstance(uri, str) and "/Server/" in uri


def _select_canonical_server_profiles(records: list[dict]) -> dict[str, dict]:
    patterns = {
        "nano": (2266, "Nano Embedded Device 2025 Server Profile"),
        "micro": (2267, "Micro Embedded Device 2025 Server Profile"),
        "embedded": (2268, "Embedded 2025 UA Server Profile"),
        "standard": (2269, "Standard 2025 UA Server Profile"),
    }
    selected: dict[str, dict] = {}
    active_server_records = [
        r for r in records
        if _is_server_record(r) and r.get("releaseStatus") == 3
    ]
    for key, (expected_id, name) in patterns.items():
        match = next(
            (
                r for r in active_server_records
                if r.get("id") == expected_id and r.get("name") == name
            ),
            None,
        )
        if match is None:
            raise ValueError(
                "missing active canonical server profile: "
                + name
                + " (id "
                + str(expected_id)
                + ")"
            )
        selected[key] = match
    return selected
```

- [ ] **Step 4: Run tests and confirm they pass**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles -v`

Expected: all tests pass.

- [ ] **Step 5: Commit**

Controller checkpoint only. Worker agents must not run this command. The controller may stage only these two files after review:

```bash
git add scripts/profile_manifest/import_opc_profiles.py scripts/profile_manifest/test_import_opc_profiles.py
```

## Task 2: Normalize API Catalog and Relationship Data

**Files:**
- Modify: `scripts/profile_manifest/import_opc_profiles.py`
- Modify: `scripts/profile_manifest/test_import_opc_profiles.py`

**Interfaces:**
- Consumes: `_select_canonical_server_profiles(records: list[dict]) -> dict[str, dict]`
- Produces:
  - `_normalize_api_snapshot(profile_rows: list[dict], cu_rows: list[dict], included_profiles: dict[str, list[dict]], included_cus: dict[str, list[dict]]) -> dict`
  - JSON object with keys `metadata`, `canonical_profiles`, `profiles`, `facets`, `superseded_profiles`, `conformance_units`, `relationships`, and `minimum_profile_defaults`.

**OPC grounding:** OPC-10000-7 §4.2 defines ConformanceUnits as Profile building blocks; §4.3 states Profiles aggregate ConformanceUnits and other Profiles; §4.5 states included Profiles are mandatory for conformance.

- [ ] **Step 1: Add failing normalization test**

Append this test method to `ImportOpcProfilesTests`:

```python
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
```

- [ ] **Step 2: Run the new test and confirm it fails**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles.ImportOpcProfilesTests.test_normalize_api_snapshot_records_relationships -v`

Expected: error for missing `_normalize_api_snapshot`.

- [ ] **Step 3: Implement normalization helpers**

Add these helpers to `scripts/profile_manifest/import_opc_profiles.py`:

```python
def _normalize_profile_row(row: dict) -> dict:
    return {
        "opc_id": row.get("id"),
        "name": row.get("name"),
        "display_name": row.get("name"),
        "opc_profile_uri": row.get("profileUri"),
        "release_status": row.get("releaseStatus"),
        "release_status_name": _release_status_name(row.get("releaseStatus")),
        "description": row.get("description") or "",
        "last_update_time": row.get("lastUpdateTime"),
        "source_metadata": {
            "profile_group_id": row.get("profileGroupId"),
            "source_endpoint": "profile/?pg=171&all=1",
        },
    }


def _normalize_cu_row(row: dict) -> dict:
    return {
        "opc_id": row.get("id"),
        "name": row.get("name"),
        "release_status": row.get("releaseStatus"),
        "release_status_name": _release_status_name(row.get("releaseStatus")),
        "description": row.get("description") or "",
        "source_metadata": {
            "profile_group_id": row.get("profileGroupId"),
            "source_endpoint": "conformanceunit/?pg=171&all=1",
        },
    }


def _ids(rows: list[dict]) -> list[int]:
    values = [row.get("id") for row in rows]
    return sorted(v for v in values if isinstance(v, int))


def _normalize_api_snapshot(
    profile_rows: list[dict],
    cu_rows: list[dict],
    included_profiles: dict[str, list[dict]],
    included_cus: dict[str, list[dict]],
) -> dict:
    canonical = _select_canonical_server_profiles(profile_rows)
    server_records = [row for row in profile_rows if _is_server_record(row)]
    facet_records = [
        row for row in server_records
        if isinstance(row.get("profileUri"), str) and "Facet" in row.get("profileUri")
    ]
    profile_records = [row for row in server_records if row not in facet_records]
    normalized = {
        "snapshot_version": "2",
        "metadata": {
            "profile_group_id": 171,
            "source": "https://profiles.opcfoundation.org/api/",
            "note": "Normalized subset of OPC UA 1.05 API data for deterministic Kconfig generation.",
        },
        "canonical_profiles": {
            key: _normalize_profile_row(row)
            for key, row in sorted(canonical.items())
        },
        "profiles": [
            _normalize_profile_row(row)
            for row in sorted(profile_records, key=lambda r: (r.get("id") or 0))
            if row.get("releaseStatus") == 3
        ],
        "facets": [
            _normalize_profile_row(row)
            for row in sorted(facet_records, key=lambda r: (r.get("id") or 0))
            if row.get("releaseStatus") == 3
        ],
        "superseded_profiles": [
            _normalize_profile_row(row)
            for row in sorted(profile_records, key=lambda r: (r.get("id") or 0))
            if row.get("releaseStatus") == 4
        ],
        "conformance_units": [_normalize_cu_row(row) for row in sorted(cu_rows, key=lambda r: (r.get("id") or 0))],
        "relationships": {
            "included_profiles": {
                str(key): _ids(rows)
                for key, rows in sorted(included_profiles.items())
            },
            "included_conformance_units": {
                str(key): _ids(rows)
                for key, rows in sorted(included_cus.items())
            },
            "transitive_profile_closure": {},
            "transitive_cu_closure": {},
        },
        "minimum_profile_defaults": {},
    }
    _populate_relationship_closures(normalized)
    return normalized
```

Add this closure helper before `_normalize_api_snapshot()`:

```python
def _populate_relationship_closures(snapshot: dict) -> None:
    included_profiles = snapshot["relationships"]["included_profiles"]
    included_cus = snapshot["relationships"]["included_conformance_units"]

    def profile_closure(profile_id: int, seen: set[int] | None = None) -> list[int]:
        if seen is None:
            seen = set()
        if profile_id in seen:
            return []
        seen.add(profile_id)
        result: list[int] = []
        for child in included_profiles.get(str(profile_id), []):
            result.append(child)
            result.extend(profile_closure(child, seen))
        return sorted(set(result))

    def cu_closure(profile_id: int) -> list[int]:
        profile_ids = [profile_id] + profile_closure(profile_id)
        result: set[int] = set()
        for pid in profile_ids:
            result.update(included_cus.get(str(pid), []))
        return sorted(result)

    canonical = snapshot.get("canonical_profiles") or {}
    for profile in canonical.values():
        opc_id = profile.get("opc_id")
        if not isinstance(opc_id, int):
            continue
        snapshot["relationships"]["transitive_profile_closure"][str(opc_id)] = profile_closure(opc_id)
        snapshot["relationships"]["transitive_cu_closure"][str(opc_id)] = cu_closure(opc_id)

    order = ("nano", "micro", "embedded", "standard")
    for key in order:
        profile = canonical.get(key) or {}
        opc_id = profile.get("opc_id")
        if not isinstance(opc_id, int):
            continue
        for cu_id in snapshot["relationships"]["transitive_cu_closure"].get(str(opc_id), []):
            snapshot["minimum_profile_defaults"].setdefault(str(cu_id), key)
```

- [ ] **Step 4: Rework CLI modes and add normalized snapshot mode**

In `_parse_args()`, make `--snapshot` and `--manifest` optional parser arguments, keep `--profile-catalog`, `--cu-catalog`, and `--relationship-dir` as optional parser arguments, and put all modes in the existing required mutually-exclusive group:

```python
    parser.add_argument(
        "--snapshot",
        help="Path to the pinned OPC snapshot JSON file for manifest merge modes.",
    )
    parser.add_argument(
        "--manifest",
        help="Path to the manifest .yaml/.json file for manifest merge modes.",
    )
    parser.add_argument(
        "--profile-catalog",
        help="Raw OPC API profile catalog JSON to normalize.",
    )
    parser.add_argument(
        "--cu-catalog",
        help="Raw OPC API conformance-unit catalog JSON to normalize.",
    )
    parser.add_argument(
        "--relationship-dir",
        help="Directory containing includedprofiles-<id>.json and includedconformanceunits-<id>.json files.",
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--dry-run",
        action="store_true",
        help="Read the snapshot and print a summary without modifying the manifest.",
    )
    mode.add_argument(
        "--write",
        action="store_true",
        help="Merge snapshot OPC facts into the manifest file in place.",
    )
    mode.add_argument(
        "--write-normalized-snapshot",
        help="Path to write deterministic normalized snapshot JSON.",
    )
```

Then add mode-specific validation helpers and branch in `main()` before any access to `args.snapshot` or `args.manifest`:

```python
def _require_args(args: argparse.Namespace, names: tuple[str, ...], mode: str) -> bool:
    missing = [name for name in names if not getattr(args, name)]
    if missing:
        print(mode + ": FAIL")
        print("  missing required option(s): " + ", ".join("--" + name.replace("_", "-") for name in missing))
        return False
    return True


def _write_normalized_snapshot(args: argparse.Namespace) -> int:
    if not _require_args(
        args,
        ("profile_catalog", "cu_catalog", "relationship_dir", "write_normalized_snapshot"),
        "normalized snapshot",
    ):
        return 1
    profile_catalog = _load_snapshot(args.profile_catalog)
    cu_catalog = _load_snapshot(args.cu_catalog)
    profile_rows = profile_catalog.get("result") or []
    cu_rows = cu_catalog.get("result") or []
    included_profiles, included_cus = _load_relationship_dir(args.relationship_dir)
    normalized = _normalize_api_snapshot(profile_rows, cu_rows, included_profiles, included_cus)
    with open(args.write_normalized_snapshot, "w", encoding="utf-8") as fh:
        json.dump(normalized, fh, indent=2, sort_keys=True)
        fh.write("\n")
    return 0


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(sys.argv[1:] if argv is None else argv)

    if args.write_normalized_snapshot:
        return _write_normalized_snapshot(args)

    if not _require_args(args, ("snapshot", "manifest"), "manifest merge"):
        return 1
```

Add this loader near `_load_snapshot()`:

```python
def _load_relationship_dir(path: str | None) -> tuple[dict[str, list[dict]], dict[str, list[dict]]]:
    if not path:
        return {}, {}
    included_profiles: dict[str, list[dict]] = {}
    included_cus: dict[str, list[dict]] = {}
    for name in sorted(os.listdir(path)):
        full_path = os.path.join(path, name)
        if not os.path.isfile(full_path):
            continue
        payload = _load_snapshot(full_path)
        rows = payload.get("result") or []
        if name.startswith("includedprofiles-") and name.endswith(".json"):
            included_profiles[name[len("includedprofiles-"):-len(".json")]] = rows
        elif name.startswith("includedconformanceunits-") and name.endswith(".json"):
            included_cus[name[len("includedconformanceunits-"):-len(".json")]] = rows
    return included_profiles, included_cus
```

- [ ] **Step 5: Run tests**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles -v`

Expected: all tests pass.

- [ ] **Step 6: Do not produce the project snapshot yet**

Task 2 intentionally stops after adding normalization code and tests. Task 3 fetches ignored relationship evidence and writes `profiles/opcua-profile-normalized-snapshot.json` after the relationship directory exists.

- [ ] **Step 7: Controller checkpoint**

Worker agents must not commit. The controller may stage only these paths after review:

```bash
git add scripts/profile_manifest/import_opc_profiles.py scripts/profile_manifest/test_import_opc_profiles.py
```

## Task 3: Fetch Missing Relationship Evidence Without Committing Raw Dumps

**Files:**
- Modify: `scripts/profile_manifest/import_opc_profiles.py`
- Modify: `.gitignore` only if relationship raw files are not already ignored by `profiles/opcua-1.05-api/`
- Create: `profiles/opcua-profile-normalized-snapshot.json`

**Interfaces:**
- Consumes: `_load_snapshot(path: str) -> dict`
- Produces: `fetch_relationships(profile_ids: list[int], output_dir: str) -> None`

**OPC grounding:** Relationship endpoints are OPC Foundation API evidence for OPC-10000-7 §4.3 profile aggregation and §4.5 included-profile conformance semantics.

- [ ] **Step 1: Add relationship fetch helper**

Add these imports near the top:

```python
import urllib.request
```

Add this function near `_load_relationship_dir()`:

```python
def fetch_relationships(profile_ids: list[int], output_dir: str) -> None:
    os.makedirs(output_dir, exist_ok=True)
    base = "https://profiles.opcfoundation.org/api/profile"
    endpoints = (
        ("includedprofiles", "includedprofiles"),
        ("includedconformanceunits", "includedconformanceunits"),
    )
    for profile_id in sorted(set(profile_ids)):
        for endpoint, prefix in endpoints:
            url = f"{base}/{endpoint}/{profile_id}/?pg=171&all=1"
            target = os.path.join(output_dir, f"{prefix}-{profile_id}.json")
            with urllib.request.urlopen(url, timeout=30) as response:
                payload = response.read().decode("utf-8")
            parsed = json.loads(payload)
            with open(target, "w", encoding="utf-8") as fh:
                json.dump(parsed, fh, indent=2, sort_keys=True)
                fh.write("\n")
```

- [ ] **Step 2: Add fetch mode to the existing CLI mode group**

Add `--fetch-relationships` to the same required mutually-exclusive mode group that contains `--dry-run`, `--write`, and `--write-normalized-snapshot`:

```python
    mode.add_argument(
        "--fetch-relationships",
        action="store_true",
        help="Fetch included profile/CU relationship JSON for server profiles into --relationship-dir.",
    )
```

Add a branch in `main()` before normalized snapshot writing:

```python
    if args.fetch_relationships:
        if not _require_args(args, ("profile_catalog", "relationship_dir"), "relationship fetch"):
            return 1
        profile_catalog = _load_snapshot(args.profile_catalog)
        profile_rows = profile_catalog.get("result") or []
        server_ids = [row.get("id") for row in profile_rows if _is_server_record(row)]
        fetch_relationships([v for v in server_ids if isinstance(v, int)], args.relationship_dir)
        return 0
```

- [ ] **Step 3: Verify raw relationship files are ignored**

Run:

```bash
git check-ignore -v profiles/opcua-1.05-api/relationships/includedprofiles-2269.json
```

Expected: output references `.gitignore` entry `profiles/opcua-1.05-api/`.

- [ ] **Step 4: Fetch relationships into ignored directory**

Run:

```bash
python3 scripts/profile_manifest/import_opc_profiles.py \
  --profile-catalog profiles/opcua-1.05-api/profile-catalog.pg171.json \
  --relationship-dir profiles/opcua-1.05-api/relationships \
  --fetch-relationships
```

Expected: ignored files are created under `profiles/opcua-1.05-api/relationships/`.

- [ ] **Step 5: Generate normalized snapshot after relationship fetch**

Run:

```bash
python3 scripts/profile_manifest/import_opc_profiles.py \
  --profile-catalog profiles/opcua-1.05-api/profile-catalog.pg171.json \
  --cu-catalog profiles/opcua-1.05-api/conformance-unit-catalog.pg171.json \
  --relationship-dir profiles/opcua-1.05-api/relationships \
  --write-normalized-snapshot profiles/opcua-profile-normalized-snapshot.json
```

Expected: normalized snapshot includes `canonical_profiles.standard.opc_id` equal to `2269`, plus non-empty `relationships.included_profiles` and `relationships.included_conformance_units`.

- [ ] **Step 6: Run tests**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles -v`

Expected: all tests pass.

- [ ] **Step 7: Controller checkpoint**

Worker agents must not commit. The controller may stage only these paths after review:

```bash
git add scripts/profile_manifest/import_opc_profiles.py profiles/opcua-profile-normalized-snapshot.json
```

## Task 4: Merge Active 2025 Profiles Into the Manifest

**Files:**
- Modify: `scripts/profile_manifest/import_opc_profiles.py`
- Modify: `profiles/opcua-profile-manifest.yaml`
- Modify: `scripts/profile_manifest/test_import_opc_profiles.py`

**Interfaces:**
- Consumes: normalized snapshot keys from Task 2.
- Produces: `_merge_normalized_snapshot_into_manifest(snapshot: dict, manifest: dict) -> tuple[dict, dict[str, int]]`

**OPC grounding:** OPC-10000-7 §4.2 governs Facets/ConformanceUnits as conformance building blocks; §4.3 governs Profile aggregation; §4.5 governs mandatory included Profiles and CUs for profile conformance.

- [ ] **Step 1: Add failing merge test**

Append this test method:

```python
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
```

- [ ] **Step 2: Run test and confirm failure**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles.ImportOpcProfilesTests.test_merge_normalized_snapshot_updates_profiles_without_promoting_claims -v`

Expected: error for missing `_merge_normalized_snapshot_into_manifest`.

- [ ] **Step 3: Implement manifest merge function**

Add this function after `_merge_snapshot_into_manifest()`:

```python
def _merge_normalized_snapshot_into_manifest(
    snapshot: dict,
    manifest: dict,
) -> tuple[dict, dict[str, int]]:
    counts = {
        "profiles_updated": 0,
        "facets_added": 0,
        "conformance_units_added": 0,
        "claimed_items_preserved": 0,
        "implemented_items_preserved": 0,
        "unimplemented_items_imported": 0,
    }
    new_manifest = json.loads(json.dumps(manifest))

    metadata = new_manifest.setdefault("metadata", {})
    metadata["superseded_profiles"] = snapshot.get("superseded_profiles", [])

    profiles = new_manifest.setdefault("profiles", {})
    for key, profile_data in (snapshot.get("canonical_profiles") or {}).items():
        if key not in profiles:
            continue
        target = profiles[key]
        target["opc_display_name"] = profile_data.get("display_name") or profile_data.get("name")
        target["opc_profile_uri"] = profile_data.get("opc_profile_uri")
        target["opc_id"] = profile_data.get("opc_id")
        target["release_status"] = profile_data.get("release_status")
        target["release_status_name"] = profile_data.get("release_status_name")
        counts["profiles_updated"] += 1

    items = new_manifest.setdefault("items", [])
    facet_containment = new_manifest.setdefault("facet_containment", {})
    existing_cu_ids = set()
    existing_item_ids = {item.get("id") for item in items if isinstance(item.get("id"), str)}
    for item in items:
        if item.get("implementation_state") == "claimed":
            counts["claimed_items_preserved"] += 1
        if item.get("implementation_state") == "implemented":
            counts["implemented_items_preserved"] += 1
        opc_ref = item.get("opc_reference") if isinstance(item.get("opc_reference"), dict) else {}
        cu_id = opc_ref.get("cu_id")
        if cu_id is not None:
            existing_cu_ids.add(str(cu_id))

    minimum_defaults = snapshot.get("minimum_profile_defaults") or {}
    profile_order = ("nano", "micro", "embedded", "standard", "full")
    relationships = snapshot.get("relationships") or {}
    included_cus = relationships.get("included_conformance_units") or {}
    transitive_cu_closure = relationships.get("transitive_cu_closure") or {}
    reachable_cu_ids = {
        str(cu_id)
        for cu_ids in transitive_cu_closure.values()
        for cu_id in cu_ids
    }
    facet_ids = {
        str(facet.get("opc_id"))
        for facet in snapshot.get("facets") or []
        if facet.get("opc_id") is not None
    }
    cu_to_facet: dict[str, str] = {}
    for facet_id, cu_ids in sorted(included_cus.items()):
        if str(facet_id) not in facet_ids:
            continue
        for cu_id in cu_ids:
            cu_to_facet.setdefault(str(cu_id), str(facet_id))

    def defaults_for_cu(cu_id: int) -> dict[str, bool]:
        minimum = minimum_defaults.get(str(cu_id))
        defaults = {pk: False for pk in _DEFAULT_PROFILES}
        if minimum in profile_order:
            start = profile_order.index(minimum)
            for pk in profile_order[start:]:
                defaults[pk] = True
        return defaults

    for facet in snapshot.get("facets") or []:
        opc_id = facet.get("opc_id")
        if opc_id is None:
            continue
        facet_id = "opc_facet_" + str(opc_id)
        if facet_id not in existing_item_ids:
            items.append({
                "id": facet_id,
                "kind": "facet",
                "opc_display_name": facet.get("name"),
                "kconfig_symbol": None,
                "implementation_state": "unimplemented",
                "depends_on": [],
                "profile_defaults": {pk: False for pk in _DEFAULT_PROFILES},
                "backing_tests": [],
                "opc_reference": {
                    "spec": "OPC-10000-7",
                    "section": "4.2",
                    "profile_id": str(opc_id),
                    "profile_uri": None,
                    "source_url": "https://profiles.opcfoundation.org",
                },
                "source_metadata": facet.get("source_metadata", {}),
                "notes": facet.get("description") or "Imported from OPC API as unimplemented.",
            })
            existing_item_ids.add(facet_id)
            counts["facets_added"] += 1

    for cu in snapshot.get("conformance_units") or []:
        opc_id = cu.get("opc_id")
        if opc_id is None or str(opc_id) in existing_cu_ids:
            continue
        if str(opc_id) not in reachable_cu_ids or str(opc_id) not in cu_to_facet:
            continue
        item_id = "opc_cu_" + str(opc_id)
        if any(item.get("id") == item_id for item in items):
            continue
        items.append({
            "id": item_id,
            "kind": "conformance_unit",
            "opc_display_name": cu.get("name"),
            "kconfig_symbol": None,
            "implementation_state": "unimplemented",
            "depends_on": [],
            "profile_defaults": defaults_for_cu(opc_id),
            "backing_tests": [],
            "opc_reference": {
                "spec": None,
                "section": None,
                "cu_id": str(opc_id),
                "cu_name": cu.get("name"),
                "source_url": "https://profiles.opcfoundation.org",
            },
            "source_metadata": cu.get("source_metadata", {}),
            "notes": cu.get("description") or "Imported from OPC API as unimplemented.",
        })
        counts["unimplemented_items_imported"] += 1
        existing_item_ids.add(item_id)
        counts["conformance_units_added"] += 1
        facet_item_id = "opc_facet_" + cu_to_facet[str(opc_id)]
        contained = facet_containment.setdefault(facet_item_id, [])
        if item_id not in contained:
            contained.append(item_id)
        contained.sort()

    return new_manifest, counts
```

Also replace `_print_summary(counts)` with this tolerant shared printer so legacy and normalized merge summaries cannot raise `KeyError`:

```python
def _print_summary(counts: dict[str, int]) -> None:
    print("OPC snapshot import summary")
    print(f"  profiles in snapshot:          {counts.get('profiles_in_snapshot', 0)}")
    print(f"  profiles matched in manifest:   {counts.get('profiles_matched', 0)}")
    print(f"  profiles updated:               {counts.get('profiles_updated', 0)}")
    print(f"  facets in snapshot:             {counts.get('facets_in_snapshot', 0)}")
    print(f"  facets matched (preserved):     {counts.get('facets_matched', 0)}")
    print(f"  facets added (unimplemented):   {counts.get('facets_added', 0)}")
    print(f"  CUs in snapshot:                {counts.get('conformance_units_in_snapshot', 0)}")
    print(f"  CUs matched (preserved):        {counts.get('conformance_units_matched', 0)}")
    print(f"  CUs added (unimplemented):      {counts.get('conformance_units_added', 0)}")
    print(f"  claimed items preserved:        {counts.get('claimed_items_preserved', 0)}")
    print(f"  implemented items preserved:    {counts.get('implemented_items_preserved', 0)}")
    print(f"  unimplemented items imported:   {counts.get('unimplemented_items_imported', 0)}")
```

- [ ] **Step 4: Wire merge mode to normalized snapshots**

In `main()`, when `--snapshot` points to a snapshot with `snapshot_version` equal to `2`, call `_merge_normalized_snapshot_into_manifest()` instead of `_merge_snapshot_into_manifest()`.

Use this branch after loading `snapshot` and `manifest`:

```python
    if snapshot.get("snapshot_version") == "2":
        new_manifest, counts = _merge_normalized_snapshot_into_manifest(snapshot, manifest)
    else:
        new_manifest, counts = _merge_snapshot_into_manifest(snapshot, manifest)
```

- [ ] **Step 5: Run tests**

Run: `python3 -m unittest scripts.profile_manifest.test_import_opc_profiles -v`

Expected: all tests pass.

- [ ] **Step 6: Apply merge to manifest**

Run:

```bash
python3 scripts/profile_manifest/import_opc_profiles.py \
  --snapshot profiles/opcua-profile-normalized-snapshot.json \
  --manifest profiles/opcua-profile-manifest.yaml \
  --write
```

Expected: output reports `profiles_updated` at least `4`; manifest profile entries use 2025 display names.

- [ ] **Step 7: Validate manifest only**

Run: `python3 scripts/profile_manifest/validate.py --manifest-only`

Expected: `manifest: OK`.

- [ ] **Step 8: Controller checkpoint**

Worker agents must not commit. The controller may stage only these paths after review:

```bash
git add scripts/profile_manifest/import_opc_profiles.py scripts/profile_manifest/test_import_opc_profiles.py profiles/opcua-profile-manifest.yaml
```

## Task 5: Validate Superseded Profile and Generated 2025 Surface Rules

**Files:**
- Modify: `scripts/profile_manifest/model.py`
- Modify: `scripts/profile_manifest/validate.py`

**Interfaces:**
- Consumes manifest profile fields `release_status_name`, `opc_display_name`, and `selectable`.
- Produces validation checks:
  - active canonical profile entries exist for `nano`, `micro`, `embedded`, `standard`.
  - canonical profile choice symbols contain `2025`.
  - superseded `2022` profile symbols are not selectable profile-choice defaults.

**OPC grounding:** OPC-10000-7 §4.3 defines named Profiles, and §4.7 defines versioned profile names. OPC API release status remains OPC Foundation API metadata used to choose active records.

- [ ] **Step 1: Add model validation for release metadata**

In `scripts/profile_manifest/model.py`, inside the profile loop after `selectable` is checked, add:

```python
        release_status_name = profile.get("release_status_name")
        if release_status_name is not None and release_status_name not in ("active", "superseded", "unknown"):
            _err(
                errors,
                f"profile '{profile_key}': release_status_name '{release_status_name}' is invalid",
            )
        if profile_key in ("nano", "micro", "embedded", "standard"):
            if profile.get("release_status_name") != "active":
                _err(errors, f"profile '{profile_key}': canonical named profile must be active")
            display = profile.get("opc_display_name")
            if not isinstance(display, str) or "2025" not in display:
                _err(errors, f"profile '{profile_key}': canonical named profile must use 2025 display name")
```

- [ ] **Step 2: Add generated Kconfig validation**

In `scripts/profile_manifest/validate.py`, add a function near other generated checks:

```python
def _check_latest_active_profile_surface(errors: list[str]) -> None:
    kconfig_path = os.path.join(_REPO_ROOT, "Kconfig")
    config_dir = os.path.join(_REPO_ROOT, "configs")
    with open(kconfig_path, "r", encoding="utf-8") as fh:
        kconfig = fh.read()
    required = (
        "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER",
        "MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER",
        "MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER",
        "MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER",
    )
    for sym in required:
        if sym not in kconfig:
            errors.append("generated Kconfig missing active profile symbol " + sym)
    forbidden_choice = (
        "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2022_SERVER",
        "MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2022_SERVER",
        "MUC_OPCUA_PROFILE_EMBEDDED_2022_UA_SERVER",
        "MUC_OPCUA_PROFILE_STANDARD_2022_UA_SERVER",
    )
    for sym in forbidden_choice:
        if ("config " + sym) in kconfig:
            errors.append("superseded 2022 profile must not be selectable: " + sym)
    defconfigs = {
        "nano": required[0],
        "micro": required[1],
        "embedded": required[2],
        "standard": required[3],
    }
    for name, sym in defconfigs.items():
        path = os.path.join(config_dir, name + ".defconfig")
        with open(path, "r", encoding="utf-8") as fh:
            text = fh.read()
        if sym + "=y" not in text:
            errors.append(path + " does not select " + sym)
```

Call `_check_latest_active_profile_surface(errors)` from the generated validation path after Kconfig exists.

- [ ] **Step 3: Run validation and confirm current generated files fail until regeneration**

Run: `python3 scripts/profile_manifest/validate.py --all`

Expected before regeneration: validation may fail for missing 2025 generated symbols. If it already passes because artifacts were regenerated by Task 4, continue.

- [ ] **Step 4: Controller checkpoint**

Worker agents must not commit. The controller may stage only these paths after review:

```bash
git add scripts/profile_manifest/model.py scripts/profile_manifest/validate.py
```

## Task 6: Regenerate Kconfig Artifacts From Migrated Manifest

**Files:**
- Modify: `Kconfig`
- Modify: `configs/nano.defconfig`
- Modify: `configs/micro.defconfig`
- Modify: `configs/embedded.defconfig`
- Modify: `configs/standard.defconfig`
- Modify: `configs/full.defconfig`
- Modify: `include/muc_opcua/capacities.h`
- Modify: `tests/conformance/claim_test_map.md`
- Modify: `docs/conformance/opc-profile-roadmap.md`
- Modify: `docs/build-and-gating.md`

**Interfaces:**
- Consumes: migrated `profiles/opcua-profile-manifest.yaml`.
- Produces: generated files using active 2025 profile symbols.

**OPC grounding:** Generated defaults implement OPC-10000-7 §4.3 profile aggregation and §4.5 profile conformance as project Kconfig presets; selections remain editable and do not assert external certification.

- [ ] **Step 1: Regenerate all generated artifacts**

Run:

```bash
python3 scripts/profile_manifest/generate.py \
  --manifest profiles/opcua-profile-manifest.yaml \
  --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
```

Expected: command exits 0 and updates generated files.

- [ ] **Step 2: Check expected profile symbols**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
checks = {
    "configs/nano.defconfig": "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER=y",
    "configs/micro.defconfig": "MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER=y",
    "configs/embedded.defconfig": "MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER=y",
    "configs/standard.defconfig": "MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER=y",
}
for path, needle in checks.items():
    text = Path(path).read_text()
    if needle not in text:
        raise SystemExit(f"missing {needle} in {path}")
print("active profile defconfigs: OK")
PY
```

Expected: `active profile defconfigs: OK`.

- [ ] **Step 3: Check superseded profile symbols are not selectable**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
kconfig = Path("Kconfig").read_text()
for sym in (
    "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2022_SERVER",
    "MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2022_SERVER",
    "MUC_OPCUA_PROFILE_EMBEDDED_2022_UA_SERVER",
    "MUC_OPCUA_PROFILE_STANDARD_2022_UA_SERVER",
):
    if f"config {sym}" in kconfig:
        raise SystemExit(f"superseded selectable profile found: {sym}")
print("superseded profile choice: OK")
PY
```

Expected: `superseded profile choice: OK`.

- [ ] **Step 4: Run manifest/generated validation**

Run: `python3 scripts/profile_manifest/validate.py --all`

Expected: `manifest: OK`.

- [ ] **Step 5: Controller checkpoint**

Worker agents must not commit. The controller may stage only these paths after review:

```bash
git add Kconfig \
  configs/nano.defconfig \
  configs/micro.defconfig \
  configs/embedded.defconfig \
  configs/standard.defconfig \
  configs/full.defconfig \
  include/muc_opcua/capacities.h \
  tests/conformance/claim_test_map.md \
  docs/conformance/opc-profile-roadmap.md \
  docs/build-and-gating.md
```

## Task 7: Update Profile Gating Tests for 2025 Canonical Defaults

**Files:**
- Modify: `scripts/test_profile_gating.sh`

**Interfaces:**
- Consumes: generated 2025 profile symbols.
- Produces: profile-gating scenarios that verify active 2025 symbols, superseded 2022 absence, cross-profile activation, and capacity propagation.

**OPC grounding:** Tests verify project behavior for OPC-10000-7 §4.3 aggregation, §4.5 included-profile conformance, and §4.7 versioned profile names.

- [ ] **Step 1: Update expected profile symbols in gating script**

In `scripts/test_profile_gating.sh`, replace 2017/2022 profile-symbol expectations with:

```bash
PROFILE_NANO="MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER"
PROFILE_MICRO="MUC_OPCUA_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER"
PROFILE_EMBEDDED="MUC_OPCUA_PROFILE_EMBEDDED_2025_UA_SERVER"
PROFILE_STANDARD="MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER"
INTERN_NANO="MUC_OPCUA_INTERN_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER"
INTERN_MICRO="MUC_OPCUA_INTERN_PROFILE_MICRO_EMBEDDED_DEVICE_2025_SERVER"
INTERN_EMBEDDED="MUC_OPCUA_INTERN_PROFILE_EMBEDDED_2025_UA_SERVER"
INTERN_STANDARD="MUC_OPCUA_INTERN_PROFILE_STANDARD_2025_UA_SERVER"
```

- [ ] **Step 2: Add superseded absence check**

Add this scenario near the profile cascade checks:

```bash
section "Superseded 2022 profiles are not canonical choices"
if grep -q "config MUC_OPCUA_PROFILE_STANDARD_2022_UA_SERVER" "$ROOT_DIR/Kconfig"; then
  fail "superseded Standard 2022 profile is selectable"
else
  pass "superseded Standard 2022 profile is not selectable"
fi
```

- [ ] **Step 3: Run profile gating tests**

Run: `bash scripts/test_profile_gating.sh`

Expected: all checks pass, with one explicit PASS for superseded Standard 2022 profile not selectable.

- [ ] **Step 4: Controller checkpoint**

Worker agents must not commit. The controller may stage only this path after review:

```bash
git add scripts/test_profile_gating.sh
```

## Task 8: End-to-End Build Verification

**Files:**
- No source edits expected.
- Build outputs under `build/standard-latest-active` and `build/full-latest-active` are not committed.

**Interfaces:**
- Consumes: regenerated Kconfig, CMake integration, and profile-gating tests.
- Produces: verified standard/full build and test evidence.

**OPC grounding:** Build verification checks the generated surfaces that encode OPC-10000-7 §4.3, §4.5, and §4.7 profile semantics as build-time presets.

- [ ] **Step 1: Configure standard profile**

Run:

```bash
cmake -S . -B build/standard-latest-active \
  -DMUC_OPCUA_PROFILE=standard \
  -DMUC_OPCUA_BUILD_TESTS=ON
```

Expected: configure exits 0.

- [ ] **Step 2: Build and test standard profile**

Run:

```bash
cmake --build build/standard-latest-active && \
ctest --test-dir build/standard-latest-active --output-on-failure
```

Expected: CTest reports 100% tests passed.

- [ ] **Step 3: Verify standard generated surfaces**

Run:

```bash
grep -q 'MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER=y' build/standard-latest-active/.config
grep -q 'MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER' build/standard-latest-active/muc_opcua_config.cmake
grep -q 'MUC_OPCUA_PROFILE_STANDARD_2025_UA_SERVER' build/standard-latest-active/include/muc_opcua/autoconf.h
```

Expected: all `grep` commands exit 0.

- [ ] **Step 4: Configure full profile**

Run:

```bash
cmake -S . -B build/full-latest-active \
  -DMUC_OPCUA_PROFILE=full \
  -DMUC_OPCUA_BUILD_TESTS=ON
```

Expected: configure exits 0.

- [ ] **Step 5: Build and test full profile**

Run:

```bash
cmake --build build/full-latest-active && \
ctest --test-dir build/full-latest-active --output-on-failure
```

Expected: CTest reports 100% tests passed.

- [ ] **Step 6: Run final validation commands**

Run:

```bash
python3 scripts/profile_manifest/validate.py --all
bash scripts/test_profile_gating.sh
git diff --check
```

Expected: validation passes, profile gating passes, and `git diff --check` has no output.

- [ ] **Step 7: Controller checkpoint for verification-only docs if any generated docs changed**

Worker agents must not commit. If verification updates docs or generated reports, the controller may stage only these paths after review:

```bash
git add docs/conformance/opc-profile-roadmap.md \
  docs/build-and-gating.md \
  tests/conformance/claim_test_map.md
```

If no files changed, do not create an empty commit.

## Self-Review

Spec coverage:

- Active 2025 profile selection is covered by Tasks 1, 4, 5, 6, and 7.
- Superseded 2022 handling is covered by Tasks 1, 5, 6, and 7.
- Relationship endpoint normalization is covered by Tasks 2 and 3.
- Claim honesty is covered by Task 4 and model validation in Task 5.
- Generated `.config`, CMake, and `autoconf.h` surfaces are covered by Task 8.
- Raw API dump non-commit policy is covered by Task 3.

Placeholder scan: red-flag wording is absent. Each code-changing step names paths, code snippets, commands, and expected output.

Type consistency: importer function names are introduced in Task 1 and reused consistently in later tasks. Snapshot keys are introduced in Task 2 and consumed by Task 4.
