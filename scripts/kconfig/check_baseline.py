#!/usr/bin/env python3
"""Check each profile's canonical Kconfig-to-compiler gate projection.

The canonical gate domain is derived independently from Kconfig symbols used in
``src/CMakeLists.txt`` implementation selectors.  The checker first verifies
that those selectors have matching compiler definitions, then checks each
profile's resolved compiler command.

For each profile, this script configures CMake, reads the compile command for
``src/core/server/init.c``, and compares enabled canonical compiler definitions
with enabled Kconfig BOOLs inside that derived domain.

Usage: check_baseline.py <repo-root> <kconfig-python>
Exit 0 if all six profile projections match, 1 otherwise.
"""

import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
from collections.abc import Iterable, Mapping
from typing import Final, TypeAlias, TypedDict

PROFILES: Final = ("nano", "micro", "embedded", "standard", "full", "custom")
SEMANTIC_PREFIXES: Final = (
    "MUC_OPCUA_PROFILE_",
    "MUC_OPCUA_INTERN_",
)
_CMAKE_DEFINITIONS_RE: Final = re.compile(
    r"target_compile_definitions\s*\(\s*muc_opcua\b(?P<body>.*?)\)", re.DOTALL
)
_CMAKE_SELECTORS_RE: Final = re.compile(
    r"(?:if|elseif)\s*\((?P<body>.*?)\)", re.DOTALL
)
_DEFINITION_RE: Final = re.compile(r"(?:-D)?(?P<name>MUC_OPCUA_[A-Z0-9_]+)(?:=(?P<value>[^\s]*))?")
_SYMBOL_RE: Final = re.compile(r"MUC_OPCUA_[A-Z0-9_]+")
_KCONFIG_STATE_SCRIPT: Final = """
import sys
import kconfiglib

kconfig = kconfiglib.Kconfig(sys.argv[1], warn=False)
kconfig.load_config(sys.argv[2])
for symbol in kconfig.unique_defined_syms:
    if symbol.type == kconfiglib.BOOL:
        print("bool\\t" + symbol.name)
        if symbol.tri_value == 2:
            print("enabled\\t" + symbol.name)
"""

JsonValue: TypeAlias = str | int | float | bool | None | list["JsonValue"] | dict[str, "JsonValue"]
DefinitionValue: TypeAlias = str | None


class GateProjection(TypedDict):
    """Differences between expected and actual canonical gates."""

    missing: set[str]
    unexpected: set[str]


def normalize_kconfig_symbol(symbol: str) -> str:
    if symbol.startswith("MUC_OPCUA_"):
        return symbol
    return "MUC_OPCUA_" + symbol


def _parse_definition_token(token: str) -> tuple[str, DefinitionValue] | None:
    match = _DEFINITION_RE.fullmatch(token)
    if match is None:
        return None
    return match.group("name"), match.group("value")


def _record_definition(definitions: dict[str, DefinitionValue], name: str, value: DefinitionValue) -> None:
    if name in definitions and definitions[name] != value:
        message = f"conflicting values for compiler definition {name}"
        raise ValueError(message)
    definitions[name] = value


def _parse_compiler_tokens(tokens: Iterable[str]) -> Mapping[str, DefinitionValue]:
    definitions: dict[str, DefinitionValue] = {}
    token_list = list(tokens)
    index = 0
    while index < len(token_list):
        token = token_list[index]
        payload: str | None = None
        if token == "-D" and index + 1 < len(token_list):
            index += 1
            payload = token_list[index]
        elif token.startswith("-D"):
            payload = token
        if payload is not None:
            parsed = _parse_definition_token(payload)
            if parsed is not None:
                name, value = parsed
                _record_definition(definitions, name, value)
        index += 1
    return definitions


def parse_compile_definitions(text: str) -> Mapping[str, DefinitionValue]:
    calls = list(_CMAKE_DEFINITIONS_RE.finditer(re.sub(r"#.*$", "", text, flags=re.MULTILINE)))
    if not calls:
        return _parse_compiler_tokens(shlex.split(text))

    definitions: dict[str, DefinitionValue] = {}
    for call in calls:
        for token in call.group("body").split():
            name = token.removeprefix("-D").partition("=")[0]
            if _DEFINITION_RE.fullmatch(name) is not None:
                definitions.setdefault(name, None)
    return definitions


def parse_cmake_gate_selectors(text: str) -> set[str]:
    selectors: set[str] = set()
    uncommented = re.sub(r"#.*$", "", text, flags=re.MULTILINE)
    for condition in _CMAKE_SELECTORS_RE.finditer(uncommented):
        selectors.update(_SYMBOL_RE.findall(condition.group("body")))
    return selectors


def compiler_gate_names(
    selector_names: set[str], kconfig_bools: set[str]
) -> set[str]:
    return {
        name
        for name in selector_names
        if name in kconfig_bools and not name.startswith(SEMANTIC_PREFIXES)
    }


def compiler_gate_inventory(
    selector_names: set[str], kconfig_bools: set[str]
) -> set[str]:
    return compiler_gate_names(selector_names, kconfig_bools)


def enabled_compiler_gates(
    definitions: Mapping[str, DefinitionValue], gate_domain: set[str]
) -> set[str]:
    enabled: set[str] = set()
    for name in gate_domain:
        if name not in definitions:
            continue
        value = definitions[name]
        if value not in (None, "1"):
            message = f"noncanonical value for compiler gate {name}: {value!r}"
            raise ValueError(message)
        enabled.add(name)
    return enabled


def compare_gate_projection(expected: set[str], actual: set[str]) -> GateProjection:
    return {"missing": expected - actual, "unexpected": actual - expected}


def validate_gate_inventory(inventory: set[str]) -> None:
    if inventory:
        return
    message = "global canonical compiler-gate inventory is empty"
    raise ValueError(message)


def kconfig_state(root: str, py: str, profile: str) -> tuple[set[str], set[str]]:
    env = os.environ.copy()
    env["CONFIG_"] = ""
    env["PYTHONPATH"] = str(Path(root) / "scripts" / "kconfig")
    env["PYTHONNOUSERSITE"] = "1"
    env.pop("KCONFIG_FUNCTIONS", None)
    output = subprocess.check_output(
        [
            py,
            "-c",
            _KCONFIG_STATE_SCRIPT,
            str(Path(root) / "Kconfig"),
            str(Path(root) / "configs" / f"{profile}.defconfig"),
        ],
        env=env,
        text=True,
    )
    all_bools: set[str] = set()
    enabled_bools: set[str] = set()
    for line in output.splitlines():
        category, separator, symbol = line.partition("\t")
        if not separator:
            message = f"invalid Kconfig state output: {line!r}"
            raise ValueError(message)
        normalized = normalize_kconfig_symbol(symbol)
        if category == "bool":
            all_bools.add(normalized)
            continue
        if category == "enabled":
            enabled_bools.add(normalized)
            continue
        message = f"unknown Kconfig state category: {category!r}"
        raise ValueError(message)
    return all_bools, enabled_bools


def kconfig_flags(root: str, py: str, profile: str) -> set[str]:
    return kconfig_state(root, py, profile)[1]


def _sentinel_compile_definitions(builddir: str) -> Mapping[str, DefinitionValue]:
    document: JsonValue = json.loads(
        (Path(builddir) / "compile_commands.json").read_text(encoding="utf-8")
    )
    if not isinstance(document, list):
        message = "compile_commands.json must contain a JSON array"
        raise ValueError(message)

    sentinel: dict[str, JsonValue] | None = None
    for entry in document:
        if not isinstance(entry, dict):
            continue
        file_value = entry.get("file")
        if isinstance(file_value, str) and "/src/core/server/init.c" in file_value.replace("\\", "/"):
            sentinel = entry
            break
    if sentinel is None:
        message = "compile_commands.json has no src/core/server/init.c command"
        raise ValueError(message)

    if "arguments" in sentinel:
        arguments = sentinel["arguments"]
        if not isinstance(arguments, list) or not all(
            isinstance(argument, str) for argument in arguments
        ):
            message = "sentinel compile command arguments must be strings"
            raise ValueError(message)
        definitions = _parse_compiler_tokens(
            argument for argument in arguments if isinstance(argument, str)
        )
    else:
        command = sentinel.get("command")
        if not isinstance(command, str):
            message = "sentinel compile command has neither arguments nor command"
            raise ValueError(message)
        definitions = parse_compile_definitions(command)
    if not definitions:
        message = "sentinel compile command has no recognized MUC_OPCUA definitions"
        raise ValueError(message)
    return definitions


def cmake_flags(root: str, profile: str, builddir: str, gate_domain: set[str] | None = None) -> set[str]:
    subprocess.run(
        [
            "cmake",
            "-S",
            root,
            "-B",
            builddir,
            f"-DMUC_OPCUA_PROFILE={profile}",
            "-DMUC_OPCUA_BUILD_TESTS=OFF",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    definitions = _sentinel_compile_definitions(builddir)
    domain = set(definitions) if gate_domain is None else gate_domain
    return enabled_compiler_gates(definitions, domain)


def main(argv: list[str]) -> int:
    root, py = argv[1], argv[2]
    cmake_source = (Path(root) / "src" / "CMakeLists.txt").read_text(encoding="utf-8")

    kconfig_bools, nano_enabled = kconfig_state(root, py, "nano")
    if not kconfig_bools:
        message = "global Kconfig BOOL inventory is empty"
        raise ValueError(message)
    selectors = parse_cmake_gate_selectors(cmake_source)
    gate_inventory = compiler_gate_names(selectors, kconfig_bools)
    validate_gate_inventory(gate_inventory)
    declared_gates = set(parse_compile_definitions(cmake_source)) & kconfig_bools
    declaration_projection = compare_gate_projection(gate_inventory, declared_gates)
    if declaration_projection["missing"] or declaration_projection["unexpected"]:
        print("compiler-gate declarations: MISMATCH")
        print("  declaration-missing:", sorted(declaration_projection["missing"]))
        print("  declaration-unexpected:", sorted(declaration_projection["unexpected"]))
        return 1

    matched = True
    for profile in PROFILES:
        enabled_bools = (
            nano_enabled
            if profile == "nano"
            else kconfig_state(root, py, profile)[1]
        )
        expected = enabled_bools & gate_inventory
        with tempfile.TemporaryDirectory() as builddir:
            actual = cmake_flags(root, profile, builddir, gate_inventory)
        projection = compare_gate_projection(expected, actual)
        if not projection["missing"] and not projection["unexpected"]:
            print(f"{profile}: MATCH ({len(expected)} canonical gates)")
            continue
        matched = False
        print(f"{profile}: MISMATCH (canonical compiler-gate projection)")
        print("  compiler-missing:", sorted(projection["missing"]))
        print("  compiler-unexpected:", sorted(projection["unexpected"]))
    return 0 if matched else 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
