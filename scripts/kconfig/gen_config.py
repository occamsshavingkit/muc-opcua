#!/usr/bin/env python3
"""Resolve a Kconfig for a profile (+ optional overrides) and emit config.cmake [+ autoconf.h].

Symbol names in Kconfig are bare (e.g. DATA_ACCESS); the config prefix is MUC_OPCUA_, so
outputs use the existing C macro names verbatim:
  - config.cmake : `set(MUC_OPCUA_<SYM> ON|OFF)` for bool symbols and
                   `set(MUC_OPCUA_<SYM> <N>)` for int/hex symbols -- drives
                   src/CMakeLists.txt source selection AND its target_compile_definitions
                   (so the -D the compiler sees is byte-identical to the pre-Kconfig build).
                   Int values also become the public MU_MAX_* compile definitions that
                   capacities.h stage 3 consumes, so Kconfig / menuconfig / .config capacity
                   overrides reach the compiler.
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
# CONFIG_ is empty: the generated Kconfig mixes bare legacy/internal symbols
# (READ_CACHE, MAX_SESSIONS, ...) with fully-prefixed symbols
# (MUC_OPCUA_FACET_CORE_2017_SERVER, ...). With an empty prefix, defconfig /
# fragment / .config lines use the Kconfig symbol name verbatim, so both
# classes resolve correctly. config.cmake still emits the MUC_OPCUA_<SYM>
# CMake variable name (see below).
os.environ["CONFIG_"] = ""

import kconfiglib  # noqa: E402


def _server_profile_choice(kconf):
    return next(
        (
            choice
            for choice in kconf.choices
            if any(sym.name == "MUC_OPCUA_PROFILE_CUSTOM" for sym in choice.syms)
        ),
        None,
    )


def _profile_fragment_values(fragment, profile_names):
    values = {}
    if not fragment or not os.path.exists(fragment):
        return values

    with open(fragment, encoding="utf-8") as fragment_file:
        for raw_line in fragment_file:
            line = raw_line.strip()
            if "=" in line:
                name, value = line.split("=", 1)
                if name in profile_names:
                    values[name] = value == "y"
            elif line.startswith("# ") and line.endswith(" is not set"):
                name = line[2:-11]
                if name in profile_names:
                    values[name] = False
    return values


def main(argv):
    if len(argv) < 4:
        sys.stderr.write(__doc__ or "")
        return 2
    kconfig_path, defconfig, cmake_out = argv[1], argv[2], argv[3]
    autoconf_out = argv[4] if len(argv) > 4 and argv[4] else None
    fragment = argv[5] if len(argv) > 5 and argv[5] else None

    kconf = kconfiglib.Kconfig(kconfig_path, warn=True, warn_to_stderr=True)
    kconf.load_config(defconfig)                       # profile base
    profile_choice = _server_profile_choice(kconf)
    base_profile = profile_choice.selection if profile_choice is not None else None
    profile_names = {sym.name for sym in profile_choice.syms} if profile_choice else set()
    profile_overrides = _profile_fragment_values(fragment, profile_names)
    if fragment and os.path.exists(fragment):
        kconf.load_config(fragment, replace=False)     # user overrides on top

    if (
        profile_choice is not None
        and base_profile is not None
        and profile_overrides.get(base_profile.name) is False
        and not any(profile_overrides.values())
    ):
        kconf.syms["MUC_OPCUA_PROFILE_CUSTOM"].set_value(2)

    has_explicit_profile = profile_choice is not None and any(
        sym.user_value == 2 for sym in profile_choice.syms
    )
    effective_profile_count = (
        sum(sym.tri_value == 2 for sym in profile_choice.syms)
        if profile_choice is not None
        else 0
    )
    if not has_explicit_profile or effective_profile_count != 1:
        sys.stderr.write(
            "gen_config: exactly one profile is required; "
            "select exactly one server profile\n"
        )
        return 1

    with open(cmake_out, "w") as f:
        f.write("# Generated from Kconfig by scripts/kconfig/gen_config.py -- do not edit.\n")
        for sym in kconf.unique_defined_syms:
            # Symbols already carrying the MUC_OPCUA_ prefix (e.g. facets/profile
            # symbols) are emitted verbatim; bare legacy/internal symbols
            # (READ_CACHE, MAX_SESSIONS, ...) get the prefix added so the CMake
            # variable name is always MUC_OPCUA_<SYM>.
            cmake_name = sym.name if sym.name.startswith("MUC_OPCUA_") else "MUC_OPCUA_" + sym.name
            if sym.type == kconfiglib.BOOL:
                f.write("set(%s %s)\n" % (cmake_name, "ON" if sym.tri_value else "OFF"))
            elif sym.type in (kconfiglib.INT, kconfiglib.HEX):
                val = sym.str_value or "0"
                f.write("set(%s %s)\n" % (cmake_name, val))

    if autoconf_out:
        kconf.write_autoconf(autoconf_out)
        bare_boolean_names = {
            sym.name
            for sym in kconf.unique_defined_syms
            if sym.type == kconfiglib.BOOL and not sym.name.startswith("MUC_OPCUA_")
        }
        with open(autoconf_out, "r+") as f:
            autoconf = f.read()
            for name in bare_boolean_names:
                autoconf = autoconf.replace(
                    "#define %s 1" % name,
                    "#define MUC_OPCUA_%s 1" % name,
                )
            f.seek(0)
            f.write(autoconf)
            f.truncate()

    # Write the resolved .config next to config.cmake so `menuconfig` can seed/edit it.
    kconf.write_config(os.path.join(os.path.dirname(os.path.abspath(cmake_out)), ".config"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
