#!/usr/bin/env python3
"""Feature 028 claim->test map enforcement.

For the profile a build targets, every claimed conformance unit whose row lists
that profile MUST have a backing test that is registered in that build. A claimed
unit with no profile-runnable backing test fails (nonzero exit), naming the unit.
This replaces the previous markdown-substring "traceability" with an enforced
"claimed conformance unit => a test that runs in that profile" link
(OPC-10000-7 sections 4.2 / 4.3).

Usage:
  check_claim_map.py --manifest <claim_test_map.md> --profile <p> --build-dir <dir>
"""
import argparse
import re
import shutil
import subprocess  # nosec B404 — this checker's job is to invoke ctest on the build
import sys

VALID_PROFILES = {"nano", "micro", "embedded", "standard", "full", "default", "custom"}
PROFILE_ALIASES = {"default": "full", "custom": "full"}
CLAIM_MAP_NAMED_PROFILES = {"nano", "micro", "embedded", "standard", "full"}


IN_SCOPE_CU_PROFILE_DEFAULTS = {
    "opc_cu_2317": {
        "claim": "View TranslateBrowsePath",
        "profiles": {"nano", "micro", "embedded", "standard", "full"},
    },
    "opc_cu_2328": {
        "claim": "Discovery Get Endpoints",
        "profiles": {"nano", "micro", "embedded", "standard", "full"},
    },
    "opc_cu_2352": {
        "claim": "Discovery Find Servers Self",
        "profiles": {"nano", "micro", "embedded", "standard", "full"},
    },
    "opc_cu_3530": {
        "claim": "View Basic 2",
        "profiles": {"nano", "micro", "embedded", "standard", "full"},
    },
    "opc_cu_2389": {
        "claim": "Attribute Write Values",
        "profiles": {"micro", "embedded", "standard", "full"},
    },
    "opc_cu_2400": {
        "claim": "Session Change User",
        "profiles": {"micro", "embedded", "standard", "full"},
    },
    "opc_cu_2936": {
        "claim": "Attribute Write StatusCode & Timestamp",
        "profiles": {"micro", "embedded", "standard", "full"},
    },
    "opc_cu_3147": {
        "claim": "Attribute Write Index",
        # IndexRange writes carry an array Value, which only decodes with heap.
        # The strictly no-heap MCU tiers (nano/micro/embedded) cannot support it,
        # so it is claimed only on the heap-capable standard/full tiers.
        "profiles": {"standard", "full"},
    },
    "opc_cu_3192": {
        "claim": "Base Info Diagnostics",
        "profiles": {"micro", "embedded", "standard", "full"},
    },
    "opc_cu_3983": {
        "claim": "Base Services Diagnostics",
        "profiles": {"micro", "embedded", "standard", "full"},
    },
}


def _parse_profiles(cell):
    """Profiles cell -> set of profile names ('all' expands; '' is never applicable)."""
    val = cell.strip().lower()
    if val == "all":
        return set(VALID_PROFILES)
    if val == "":
        return set()
    return {p.strip() for p in cell.split(",") if p.strip()}


def _parse_row(line):
    """Parse one markdown table line into (claim, section, profiles, backing) or None."""
    if not line.startswith("|"):
        return None
    cells = [c.strip() for c in line.strip().strip("|").split("|")]
    if len(cells) < 4:
        return None
    claim, section, profiles_cell, backing_cell = cells[0], cells[1], cells[2], cells[3]
    # Skip the header and separator rows.
    if claim.lower().startswith("claim") or set(claim) <= set("-: "):
        return None
    backing = [re.sub(r"[`\s]", "", t) for t in backing_cell.split(",") if t.strip()]
    return (claim, section, _parse_profiles(profiles_cell), backing)


def parse_manifest(path):
    """Return list of (claim, section, profiles:set, backing_tests:list) rows."""
    rows = []
    with open(path, encoding="utf-8") as fh:
        for line in fh:
            row = _parse_row(line.rstrip("\n"))
            if row is not None:
                rows.append(row)
    return rows


def registered_tests(build_dir):
    """Set of ctest-registered test names in the build."""
    ctest = shutil.which("ctest")
    if ctest is None:
        raise FileNotFoundError("ctest not found on PATH")
    # Fixed argv (no shell); ctest is resolved to a full path above and build_dir
    # comes from CMake, not user input — running ctest is this checker's whole job.
    out = subprocess.run(  # nosec B603  # nosemgrep
        [ctest, "-N", "--test-dir", build_dir],  # nosemgrep
        capture_output=True, text=True, check=True,
    ).stdout
    names = set()
    for line in out.splitlines():
        m = re.search(r"Test\s+#\d+:\s+(\S+)", line)
        if m:
            names.add(m.group(1))
    return names


def find_gaps(rows, profile, registered):
    """Return (gaps, applicable_count) for the given profile."""
    gaps = []
    applicable = 0
    for claim, section, profiles, backing in rows:
        if profile not in profiles:
            continue
        applicable += 1
        if not backing:
            gaps.append(f"  [{claim}] ({section}) — no backing test listed")
            continue
        missing = [t for t in backing if t not in registered]
        if missing:
            gaps.append(
                f"  [{claim}] ({section}) — backing test(s) not registered in "
                f"profile '{profile}': {', '.join(missing)}"
            )
    return gaps, applicable


def _matches_in_scope_cu(claim, cu_id, expected_claim):
    claim_lower = claim.lower()
    return cu_id in claim_lower or claim_lower == expected_claim.lower()


def _profile_default_profiles(profiles):
    return {p.lower() for p in profiles if p.lower() in CLAIM_MAP_NAMED_PROFILES}


def find_profile_default_gaps(rows):
    """Return SCN-001/CASE-010 quickstart path 4 profile-default gaps.

    T006 guard for the ten 071 in-scope CUs: when a generated claim-map row
    names one of these CU ids or exact claim names, its profile column must
    match the manifest-default expectation. This prevents later generated
    artifacts from silently claiming optional write/session/diagnostics CUs for
    nano or dropping nano-default Discovery/View CUs before profile gating runs.
    The claim-map docs define "all" as the named profiles only; default/custom
    aliases remain valid for CLI backing-test enforcement but are ignored here
    because parsed rows no longer distinguish alias expansion from explicit text.
    """
    gaps = []
    for claim, section, profiles, _backing in rows:
        for cu_id, expected in IN_SCOPE_CU_PROFILE_DEFAULTS.items():
            if not _matches_in_scope_cu(claim, cu_id, expected["claim"]):
                continue
            expected_profiles = expected["profiles"]
            actual_profiles = _profile_default_profiles(profiles)
            missing = sorted(expected_profiles - actual_profiles)
            unexpected = sorted(actual_profiles - expected_profiles)
            if missing or unexpected:
                details = []
                if missing:
                    details.append(f"missing profile default(s): {', '.join(missing)}")
                if unexpected:
                    details.append(f"unexpected profile claim(s): {', '.join(unexpected)}")
                gaps.append(
                    f"  [{cu_id}] {expected['claim']} ({section}) — "
                    + "; ".join(details)
                )
            break
    return gaps


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", required=True)
    ap.add_argument("--profile", required=True)
    ap.add_argument("--build-dir", required=True)
    args = ap.parse_args()

    if args.profile not in VALID_PROFILES:
        print(f"claim-map: unknown profile '{args.profile}'", file=sys.stderr)
        return 2

    profile = PROFILE_ALIASES.get(args.profile, args.profile)

    rows = parse_manifest(args.manifest)
    if not rows:
        print("claim-map: manifest has no claim rows (parse failure?)", file=sys.stderr)
        return 2

    registered = registered_tests(args.build_dir)
    gaps, applicable = find_gaps(rows, profile, registered)
    profile_default_gaps = find_profile_default_gaps(rows)

    if gaps or profile_default_gaps:
        if profile_default_gaps:
            print(
                "claim-map FAIL: in-scope CU profile defaults do not match "
                "SCN-001 / CASE-010 / quickstart path 4 expectations:",
                file=sys.stderr,
            )
            for g in profile_default_gaps:
                print(g, file=sys.stderr)
        if gaps:
            print(
                f"claim-map FAIL: {len(gaps)} claimed conformance unit(s) lack a "
                f"profile-runnable backing test in profile '{profile}':",
                file=sys.stderr,
            )
            for g in gaps:
                print(g, file=sys.stderr)
        print(
            "\nEach claim must have a test registered in this profile, and "
            "in-scope CU profile defaults must match the manifest expectation "
            "(OPC-10000-7 §4.2/§4.3).",
            file=sys.stderr,
        )
        return 1

    print(
        f"claim-map OK: {applicable} conformance unit(s) claimed for profile "
        f"'{args.profile}' each have a registered backing test."
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
