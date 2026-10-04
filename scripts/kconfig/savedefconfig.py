#!/usr/bin/env python3
"""Write a minimal defconfig (only non-default assignments) from a .config.

Usage: savedefconfig.py <Kconfig> <.config-in> <defconfig-out>

Symbols are already fully MUC_OPCUA_-prefixed; the output is suitable to commit under configs/ or pass via
-DMUC_OPCUA_KCONFIG_CONFIG=.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
os.environ["CONFIG_"] = ""

import kconfiglib  # noqa: E402


def main(argv):
    if len(argv) != 4:
        sys.stderr.write(__doc__ or "")
        return 2
    kconfig_path, config_in, defconfig_out = argv[1:]
    kconf = kconfiglib.Kconfig(kconfig_path, warn=False)
    kconf.load_config(config_in)
    kconf.write_min_config(defconfig_out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
