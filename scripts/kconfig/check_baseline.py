#!/usr/bin/env python3
"""Acid test: the Kconfig-resolved feature set per profile == the CMake-resolved set.

For each profile it configures the current CMake build (the baseline), extracts the
resolved MUC_OPCUA_* feature defines from compile_commands.json, and compares them to the
symbols the Kconfig tree turns on for that profile's defconfig. The migration is only
correct when these match for all six profiles.

Excluded from the comparison (not Kconfig-owned): ALLOW_HEAP and MDNS_DISCOVERY are
CMake-derived (memory-model / platform); PROFILE_CUSTOM is a behavior-neutral Kconfig
choice artifact (no C consumer -- capacities.h reads only PROFILE_{MICRO,EMBEDDED,STANDARD,FULL}).

Usage: check_baseline.py <repo-root> <kconfig-python>   (kconfig-python = interpreter with kconfiglib)
Exit 0 if all profiles match, 1 otherwise.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

PROFILES = ["nano", "micro", "embedded", "standard", "full", "custom"]
EXCLUDE = {"MUC_OPCUA_ALLOW_HEAP", "MUC_OPCUA_MDNS_DISCOVERY", "MUC_OPCUA_PROFILE_CUSTOM"}


def cmake_flags(root, profile, builddir):
    subprocess.run(
        ["cmake", "-S", root, "-B", builddir, f"-DMUC_OPCUA_PROFILE={profile}",
         "-DMUC_OPCUA_BUILD_TESTS=OFF"],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    cc = json.load(open(os.path.join(builddir, "compile_commands.json")))
    e = next(x for x in cc if "/src/core/server/init.c" in x["file"].replace("\\", "/"))
    cmd = e.get("command") or " ".join(e.get("arguments", []))
    flags = set(re.findall(r"MUC_OPCUA_[A-Z0-9_]+", cmd))
    # drop platform/backend noise + the ALLOW_HEAP name (it is emitted =0/=1 regardless)
    return {f for f in flags if not f.startswith(("MUC_OPCUA_HAVE_", "MUC_OPCUA_IS_"))} - EXCLUDE


def kconfig_flags(root, py, profile):
    out = subprocess.check_output([py, "-c", f"""
import os; os.environ['CONFIG_']='MUC_OPCUA_'
import kconfiglib
k=kconfiglib.Kconfig('{root}/Kconfig', warn=False)
k.load_config('{root}/configs/{profile}.defconfig')
print('\\n'.join('MUC_OPCUA_'+s.name for s in k.unique_defined_syms
                 if s.type==kconfiglib.BOOL and s.tri_value==2))
"""], text=True)
    return set(out.split()) - EXCLUDE


def main(argv):
    root, py = argv[1], argv[2]
    ok = True
    for p in PROFILES:
        with tempfile.TemporaryDirectory() as bd:
            base = cmake_flags(root, p, bd)
        kc = kconfig_flags(root, py, p)
        if base == kc:
            print(f"{p}: MATCH ({len(kc)} flags)")
        else:
            ok = False
            print(f"{p}: MISMATCH")
            print("  cmake-only :", sorted(base - kc))
            print("  kconfig-only:", sorted(kc - base))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
