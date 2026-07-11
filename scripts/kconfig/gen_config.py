#!/usr/bin/env python3
"""Resolve a Kconfig for a profile (+ optional overrides) and emit config.cmake [+ autoconf.h].

Symbol names in Kconfig are bare (e.g. DATA_ACCESS); the config prefix is MUC_OPCUA_, so
outputs use the existing C macro names verbatim:
  - config.cmake : `set(MUC_OPCUA_<SYM> ON|OFF)` -- drives src/CMakeLists.txt source
                   selection AND its target_compile_definitions (so the -D the compiler
                   sees is byte-identical to the pre-Kconfig build).
  - autoconf.h   : `#define MUC_OPCUA_<SYM> 1` for y / UNDEFINED for n (#ifdef-safe).
                   Emitted for completeness / non-CMake consumers; not force-included.

Usage:
  gen_config.py <Kconfig> <defconfig> <config.cmake-out> [autoconf.h-out] [overrides-fragment]

<defconfig> selects the profile (a configs/<profile>.defconfig). <overrides-fragment>, if
given, is merged ON TOP (legacy -DMUC_OPCUA_X=ON/OFF translated to Kconfig assignments);
Kconfig's `depends on` still vetoes any override that violates a dependency.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))  # vendored kconfiglib.py
os.environ.setdefault("CONFIG_", "MUC_OPCUA_")

import kconfiglib  # noqa: E402


def main(argv):
    if len(argv) < 4:
        sys.stderr.write(__doc__)
        return 2
    kconfig_path, defconfig, cmake_out = argv[1], argv[2], argv[3]
    autoconf_out = argv[4] if len(argv) > 4 and argv[4] else None
    fragment = argv[5] if len(argv) > 5 and argv[5] else None

    kconf = kconfiglib.Kconfig(kconfig_path, warn=True, warn_to_stderr=True)
    kconf.load_config(defconfig)                       # profile base
    if fragment and os.path.exists(fragment):
        kconf.load_config(fragment, replace=False)     # user overrides on top

    with open(cmake_out, "w") as f:
        f.write("# Generated from Kconfig by scripts/kconfig/gen_config.py -- do not edit.\n")
        for sym in kconf.unique_defined_syms:
            if sym.type == kconfiglib.BOOL:
                f.write("set(MUC_OPCUA_%s %s)\n" % (sym.name, "ON" if sym.tri_value else "OFF"))

    if autoconf_out:
        kconf.write_autoconf(autoconf_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
