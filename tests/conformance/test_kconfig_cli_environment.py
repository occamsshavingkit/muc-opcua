#!/usr/bin/env python3
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
KCONFIG = ROOT / "Kconfig"
GEN_CONFIG = ROOT / "scripts" / "kconfig" / "gen_config.py"
SAVEDEFCONFIG = ROOT / "scripts" / "kconfig" / "savedefconfig.py"
NANO_DEFCONFIG = ROOT / "configs" / "nano.defconfig"
NANO_PROFILE = "MUC_OPCUA_PROFILE_NANO_EMBEDDED_DEVICE_2025_SERVER"
WRITE_VALUES = "MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES"
CUSTOM_PROFILE = "MUC_OPCUA_PROFILE_CUSTOM"
FULL_PROFILE = "MUC_OPCUA_PROFILE_FULL_EVERYTHING_ENABLED_GENEROUS_CAPACITIES"


class KconfigCliEnvironmentTest(unittest.TestCase):
    def test_gen_config_rejects_empty_config_without_profile(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            empty_config = output_directory / "empty.config"
            cmake_output = output_directory / "config.cmake"
            empty_config.write_text("", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    str(GEN_CONFIG),
                    str(KCONFIG),
                    str(empty_config),
                    str(cmake_output),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "exactly one profile is required",
                result.stdout + result.stderr,
            )

    def test_gen_config_accepts_explicit_custom_profile(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            config = output_directory / "custom.config"
            cmake_output = output_directory / "config.cmake"
            config.write_text(f"{CUSTOM_PROFILE}=y\n", encoding="utf-8")

            result = subprocess.run(
                [
                    sys.executable,
                    str(GEN_CONFIG),
                    str(KCONFIG),
                    str(config),
                    str(cmake_output),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn(f"set({CUSTOM_PROFILE} ON)", cmake_output.read_text(encoding="utf-8"))

    def test_cmake_rejects_saved_config_without_profile(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            build_directory = output_directory / "build"
            saved_config = output_directory / "saved.config"
            saved_config.write_text("", encoding="utf-8")

            result = subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(ROOT),
                    "-B",
                    str(build_directory),
                    f"-DMUC_OPCUA_KCONFIG_CONFIG={saved_config}",
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "exactly one profile is required",
                result.stdout + result.stderr,
            )

    def test_cmake_implicit_profile_uses_custom_seed(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = Path(temporary_directory) / "build"
            result = subprocess.run(
                ["cmake", "-S", str(ROOT), "-B", str(build_directory)],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            cache = (build_directory / "CMakeCache.txt").read_text(encoding="utf-8")
            generated = (build_directory / "muc_opcua_config.cmake").read_text(
                encoding="utf-8"
            )
            self.assertIn("MUC_OPCUA_PROFILE:STRING=custom", cache)
            self.assertIn(f"set({CUSTOM_PROFILE} ON)", generated)

    def test_cmake_implicit_profile_promotes_examples_to_full(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = Path(temporary_directory) / "build"
            result = subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(ROOT),
                    "-B",
                    str(build_directory),
                    "-DMUC_OPCUA_BUILD_EXAMPLES=ON",
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            cache = (build_directory / "CMakeCache.txt").read_text(encoding="utf-8")
            generated = (build_directory / "muc_opcua_config.cmake").read_text(
                encoding="utf-8"
            )
            self.assertIn("MUC_OPCUA_PROFILE:STRING=full", cache)
            self.assertIn(f"set({FULL_PROFILE} ON)", generated)

    def test_cmake_rejects_explicit_crypto_request_when_dependency_vetoes_it(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            build_directory = Path(temporary_directory) / "build"
            result = subprocess.run(
                [
                    "cmake",
                    "-S",
                    str(ROOT),
                    "-B",
                    str(build_directory),
                    "-DMUC_OPCUA_PROFILE=custom",
                    "-DMUC_OPCUA_FACET_CORE_2022_SERVER=OFF",
                    "-DMUC_OPCUA_SECURE_CHANNEL_CRYPTO=ON",
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertNotEqual(result.returncode, 0)
            self.assertIn(
                "MUC_OPCUA_SECURE_CHANNEL_CRYPTO was explicitly requested ON",
                result.stdout + result.stderr,
            )

    def test_gen_config_ignores_ambient_config_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            cmake_output = output_directory / "config.cmake"
            environment = os.environ.copy()
            environment["CONFIG_"] = "AMBIENT_"

            result = subprocess.run(
                [
                    sys.executable,
                    str(GEN_CONFIG),
                    str(KCONFIG),
                    str(NANO_DEFCONFIG),
                    str(cmake_output),
                ],
                cwd=ROOT,
                env=environment,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            generated_outputs = "\n".join(
                path.read_text(encoding="utf-8")
                for path in output_directory.iterdir()
                if path.is_file()
            )
            self.assertIn(f"set({NANO_PROFILE} ON)", generated_outputs)
            self.assertNotIn("AMBIENT_", generated_outputs)

    def test_gen_config_autoconf_uses_public_macro_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            cmake_output = output_directory / "config.cmake"
            autoconf_output = output_directory / "autoconf.h"
            overrides = output_directory / "overrides.config"
            overrides.write_text("READ_CACHE=y\n", encoding="utf-8")

            # Given: bare project feature symbols enabled through profile and override.
            result = subprocess.run(
                [
                    sys.executable,
                    str(GEN_CONFIG),
                    str(KCONFIG),
                    str(ROOT / "configs" / "micro.defconfig"),
                    str(cmake_output),
                    str(autoconf_output),
                    str(overrides),
                ],
                cwd=ROOT,
                capture_output=True,
                text=True,
                check=False,
            )

            # When: gen_config.py emits the requested autoconf header.
            self.assertEqual(result.returncode, 0, result.stderr)
            generated_header = autoconf_output.read_text(encoding="utf-8")

            # Then: the public macro is prefixed and the bare macro is absent.
            self.assertIn(
                "#define MUC_OPCUA_SECURE_CHANNEL_CRYPTO 1",
                generated_header,
            )
            self.assertIn("#define MUC_OPCUA_READ_CACHE 1", generated_header)
            self.assertNotIn(
                "#define SECURE_CHANNEL_CRYPTO 1",
                generated_header,
            )
            self.assertNotIn("#define READ_CACHE 1", generated_header)

    def test_savedefconfig_emits_canonical_symbols_with_ambient_prefix(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            output_directory = Path(temporary_directory)
            config_input = output_directory / ".config"
            defconfig_output = output_directory / "defconfig"
            config_input.write_text(
                f"{NANO_PROFILE}=y\n{WRITE_VALUES}=y\n",
                encoding="utf-8",
            )
            environment = os.environ.copy()
            environment["CONFIG_"] = "AMBIENT_"

            result = subprocess.run(
                [
                    sys.executable,
                    str(SAVEDEFCONFIG),
                    str(KCONFIG),
                    str(config_input),
                    str(defconfig_output),
                ],
                cwd=ROOT,
                env=environment,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(result.returncode, 0, result.stderr)
            generated_defconfig = defconfig_output.read_text(encoding="utf-8")
            self.assertIn(f"{WRITE_VALUES}=y", generated_defconfig)
            self.assertNotIn("AMBIENT_", generated_defconfig)


if __name__ == "__main__":
    unittest.main()
