#!/usr/bin/env python3
"""Generate autoconf.h + config.cmake from a Kconfig + .config (kconfiglib).

Symbol names in Kconfig are bare (e.g. DATA_ACCESS); we set the config prefix to
MUC_OPCUA_ so the outputs use the existing C macro names verbatim:
  - autoconf.h : `#define MUC_OPCUA_<SYM> 1` for y, UNDEFINED for n (#ifdef-safe).
  - config.cmake: `set(MUC_OPCUA_<SYM> ON|OFF)` for CMake source selection.

Usage:
  gen_config.py <Kconfig> <config-in> <autoconf.h-out> <config.cmake-out>

The build is expected to have produced <config-in> already (from a profile defconfig
merged with any user fragments) via kconfiglib's defconfig/alldefconfig tools, which use
the same CONFIG_=MUC_OPCUA_ prefix.
"""
import os
import sys

# kconfiglib reads the symbol prefix from the CONFIG_ env var; set it before import use.
os.environ.setdefault("CONFIG_", "MUC_OPCUA_")

import kconfiglib  # noqa: E402


def main(argv):
    if len(argv) != 5:
        sys.stderr.write(__doc__)
        return 2
    kconfig_path, config_in, autoconf_out, cmake_out = argv[1:]

    kconf = kconfiglib.Kconfig(kconfig_path, warn=True, warn_to_stderr=True)
    kconf.load_config(config_in)

    # C header (kconfiglib emits `#define MUC_OPCUA_X 1` / undefined for n).
    kconf.write_autoconf(autoconf_out)

    # CMake fragment for per-feature source selection.
    with open(cmake_out, "w") as f:
        f.write("# Generated from Kconfig by scripts/kconfig/gen_config.py -- do not edit.\n")
        for sym in kconf.unique_defined_syms:
            if sym.type == kconfiglib.BOOL:
                f.write("set(MUC_OPCUA_%s %s)\n" % (sym.name, "ON" if sym.tri_value else "OFF"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
