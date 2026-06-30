#!/usr/bin/env python3
import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "scripts" / "run_speed_bench.py"


REQUIRED_TOP_LEVEL_FIELDS = (
    "schema",
    "generated_at_utc",
    "git_commit",
    "build_type",
    "host",
    "scheduling",
    "thresholds",
    "results",
)

REQUIRED_HOST_FIELDS = (
    "hostname",
    "release",
    "machine",
    "cpu_model",
    "isolated_cpus",
    "kernel_cmdline",
    "benchmark_cpu_governor",
)

REQUIRED_RESULT_FIELDS = (
    "profile",
    "scenario",
    "nodes",
    "batch",
    "supported",
    "ops_per_sec",
    "items_per_sec",
    "normalized_ops_per_sec",
    "text",
    "data",
    "bss",
    "cpu_affinity_requested",
    "cpu_affinity_applied",
    "realtime_priority_requested",
    "realtime_priority_applied",
    "sudo_requested",
)

NEW_HOT_PATH_SCENARIOS = {
    "message-chunk-header": ((1, 1), (1, 32)),
    "binary-primitive": ((1, 1), (32, 32)),
    "uasc-response-framing": ((1, 1), (32, 32)),
}


def load_runner():
    if not SCRIPT.exists():
        raise AssertionError(f"missing benchmark runner: {SCRIPT}")
    spec = importlib.util.spec_from_file_location("run_speed_bench", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def valid_result(**overrides):
    result = {
        "profile": "micro",
        "scenario": "read-value",
        "nodes": 32,
        "batch": 16,
        "supported": True,
        "ops_per_sec": 1000.0,
        "items_per_sec": 16000.0,
        "normalized_ops_per_sec": 950.0,
        "text": 40000,
        "data": 2000,
        "bss": 10000,
        "cpu_affinity_requested": "11",
        "cpu_affinity_applied": True,
        "realtime_priority_requested": 80,
        "realtime_priority_applied": True,
        "sudo_requested": True,
    }
    result.update(overrides)
    return result


def valid_report(**overrides):
    report = {
        "schema": "micro-opcua-speed-benchmark-v1",
        "generated_at_utc": "2026-06-30T12:00:00+00:00",
        "git_commit": "0123456789abcdef",
        "build_type": "Release",
        "host": {
            "hostname": "bench-host",
            "system": "Linux",
            "release": "6.8.0",
            "machine": "x86_64",
            "processor": "x86_64",
            "python": "3.12.0",
            "cpu_count": 16,
            "cpu_model": "Test CPU",
            "isolated_cpus": "10-11",
            "kernel_cmdline": "quiet isolcpus=10-11 nohz_full=10-11",
            "benchmark_cpu_governor": "performance",
        },
        "scheduling": {
            "cpu": "11",
            "realtime_priority": 80,
            "require_affinity": True,
            "require_realtime": True,
            "shield_cpu": True,
            "use_sudo": True,
            "require_isolated_cpu": True,
        },
        "thresholds": {
            "min_ops_ratio": 0.95,
            "max_text_growth_ratio": 1.03,
            "max_ram_growth_ratio": 1.05,
        },
        "results": [valid_result()],
    }
    report.update(overrides)
    return report


class SpeedBenchmarkRunnerTests(unittest.TestCase):
    def test_default_matrix_covers_profiles_and_hot_paths(self):
        runner = load_runner()

        matrix = runner.default_matrix(["nano", "micro", "embedded", "full"])
        keys = {(entry.profile, entry.scenario, entry.nodes, entry.batch) for entry in matrix}

        self.assertLessEqual(max(entry.batch for entry in matrix), 32)

        for profile in ("nano", "micro", "embedded", "full"):
            self.assertIn((profile, "read-value", 1, 1), keys)
            self.assertIn((profile, "read-value", 32, 1), keys)
            self.assertIn((profile, "read-value", 32, 16), keys)
            self.assertIn((profile, "read-value", 128, 1), keys)
            self.assertIn((profile, "read-value", 128, 32), keys)
            self.assertIn((profile, "read-bad-node", 128, 16), keys)

        self.assertNotIn(("nano", "write-value", 32, 16), keys)
        self.assertNotIn(("nano", "subscription-idle-tick", 1, 1), keys)

        for profile in ("micro", "embedded", "full"):
            self.assertIn((profile, "write-value", 1, 1), keys)
            self.assertIn((profile, "write-value", 32, 16), keys)
            self.assertIn((profile, "write-value", 128, 32), keys)
            self.assertIn((profile, "write-bad-type", 128, 16), keys)
            self.assertIn((profile, "subscription-idle-tick", 1, 1), keys)
            self.assertIn((profile, "subscription-active-tick", 32, 8), keys)

    def test_default_matrix_covers_new_hot_path_benchmark_scenarios(self):
        runner = load_runner()

        profiles = ("nano", "micro", "embedded", "full")
        matrix = runner.default_matrix(profiles)
        keys = {(entry.profile, entry.scenario, entry.nodes, entry.batch) for entry in matrix}

        missing = []
        for profile in profiles:
            for scenario, shapes in NEW_HOT_PATH_SCENARIOS.items():
                for nodes, batch in shapes:
                    with self.subTest(profile=profile, scenario=scenario, nodes=nodes, batch=batch):
                        self.assertLessEqual(batch, 32)
                    key = (profile, scenario, nodes, batch)
                    if key not in keys:
                        missing.append(key)

        self.assertFalse(missing, f"default matrix is missing new hot-path rows: {missing}")

    def test_compare_results_flags_throughput_and_resource_regressions(self):
        runner = load_runner()
        baseline = {
            "results": [
                {
                    "profile": "micro",
                    "scenario": "read-value",
                    "nodes": 32,
                    "batch": 16,
                    "supported": True,
                    "ops_per_sec": 1000.0,
                    "text": 40000,
                    "data": 2000,
                    "bss": 10000,
                }
            ]
        }
        current = {
            "results": [
                {
                    "profile": "micro",
                    "scenario": "read-value",
                    "nodes": 32,
                    "batch": 16,
                    "supported": True,
                    "ops_per_sec": 930.0,
                    "text": 42050,
                    "data": 2000,
                    "bss": 10000,
                }
            ]
        }

        failures = runner.compare_results(
            baseline,
            current,
            min_ops_ratio=0.95,
            max_text_growth_ratio=1.03,
            max_ram_growth_ratio=1.05,
        )

        self.assertEqual(2, len(failures))
        self.assertTrue(any("throughput" in failure for failure in failures))
        self.assertTrue(any("text" in failure for failure in failures))

    def test_benchmark_command_pins_to_requested_cpu(self):
        runner = load_runner()

        command = runner.benchmark_command(
            pathlib.Path("/tmp/hotpath_benchmark"),
            "read-value",
            nodes=32,
            batch=16,
            iterations=20000,
            warmup=2000,
            min_ms=200,
            cpu="11",
        )

        self.assertEqual("taskset", command[0])
        self.assertEqual(["-c", "11"], command[1:3])
        self.assertIn("/tmp/hotpath_benchmark", command)
        self.assertIn("--scenario", command)
        self.assertIn("read-value", command)

    def test_benchmark_command_can_request_realtime_priority(self):
        runner = load_runner()

        command = runner.benchmark_command(
            pathlib.Path("/tmp/hotpath_benchmark"),
            "read-value",
            nodes=1,
            batch=1,
            iterations=20000,
            warmup=2000,
            min_ms=200,
            cpu="11",
            realtime_priority=80,
        )

        self.assertEqual(["chrt", "-f", "80", "taskset", "-c", "11"], command[:6])

    def test_benchmark_command_can_use_passwordless_sudo(self):
        runner = load_runner()

        command = runner.benchmark_command(
            pathlib.Path("/tmp/hotpath_benchmark"),
            "read-value",
            nodes=1,
            batch=1,
            iterations=20000,
            warmup=2000,
            min_ms=200,
            cpu="11",
            realtime_priority=80,
            use_sudo=True,
        )

        self.assertEqual(["sudo", "-n", "chrt", "-f", "80", "taskset", "-c", "11"], command[:8])

    def test_validate_report_requires_top_level_contract_fields(self):
        runner = load_runner()

        for field in REQUIRED_TOP_LEVEL_FIELDS:
            with self.subTest(field=field):
                report = valid_report()
                del report[field]

                failures = runner.validate_report_contract(report)

                self.assertTrue(any(field in failure for failure in failures), failures)

    def test_validate_report_requires_authoritative_scheduling_criteria(self):
        runner = load_runner()
        cases = (
            ("cpu", "10"),
            ("realtime_priority", 79),
            ("use_sudo", False),
            ("require_affinity", False),
            ("require_realtime", False),
            ("require_isolated_cpu", False),
        )

        for field, value in cases:
            with self.subTest(field=field):
                report = valid_report()
                report["scheduling"][field] = value

                failures = runner.validate_authoritative_report(report)

                self.assertTrue(any("scheduling" in failure and field in failure for failure in failures), failures)

    def test_validate_report_requires_host_fingerprint_for_authoritative_run(self):
        runner = load_runner()

        for field in REQUIRED_HOST_FIELDS:
            with self.subTest(field=field):
                host = dict(valid_report()["host"])
                del host[field]
                report = valid_report(host=host)

                failures = runner.validate_report_contract(report)

                self.assertTrue(any("host" in failure and field in failure for failure in failures), failures)

    def test_validate_report_requires_host_isolated_cpu_to_include_benchmark_cpu(self):
        runner = load_runner()
        report = valid_report(host={**valid_report()["host"], "isolated_cpus": "0-10,12-15"})

        failures = runner.validate_authoritative_report(report)

        self.assertTrue(any("host.isolated_cpus" in failure and "11" in failure for failure in failures), failures)

    def test_compare_results_prefers_normalized_throughput_metric(self):
        runner = load_runner()
        baseline = {"results": [valid_result(ops_per_sec=1000.0, normalized_ops_per_sec=1000.0)]}
        current = {"results": [valid_result(ops_per_sec=2000.0, normalized_ops_per_sec=940.0)]}

        failures = runner.compare_results(
            baseline,
            current,
            min_ops_ratio=0.95,
            max_text_growth_ratio=1.03,
            max_ram_growth_ratio=1.05,
        )

        self.assertTrue(any("normalized_ops_per_sec" in failure for failure in failures), failures)

    def test_compare_results_flags_missing_and_now_unsupported_rows(self):
        runner = load_runner()
        baseline = {
            "results": [
                valid_result(profile="micro", scenario="read-value", nodes=32, batch=16),
                valid_result(profile="micro", scenario="write-value", nodes=32, batch=16),
            ]
        }
        current = {"results": [valid_result(profile="micro", scenario="write-value", nodes=32, batch=16, supported=False)]}

        failures = runner.compare_results(
            baseline,
            current,
            min_ops_ratio=0.95,
            max_text_growth_ratio=1.03,
            max_ram_growth_ratio=1.05,
        )

        self.assertTrue(any("missing current result" in failure for failure in failures), failures)
        self.assertTrue(any("current result is unsupported" in failure for failure in failures), failures)

    def test_validate_report_requires_supported_result_shape(self):
        runner = load_runner()

        for field in REQUIRED_RESULT_FIELDS:
            with self.subTest(field=field):
                result = valid_result()
                del result[field]
                report = valid_report(results=[result])

                failures = runner.validate_report_contract(report)

                self.assertTrue(any("results[0]" in failure and field in failure for failure in failures), failures)

    def test_validate_authoritative_report_requires_supported_rows_to_show_scheduling_applied(self):
        runner = load_runner()
        cases = (
            "cpu_affinity_applied",
            "realtime_priority_applied",
            "sudo_requested",
        )

        for field in cases:
            with self.subTest(field=field):
                report = valid_report(results=[valid_result(**{field: False})])

                failures = runner.validate_authoritative_report(report)

                self.assertTrue(any("results[0]" in failure and field in failure for failure in failures), failures)


if __name__ == "__main__":
    unittest.main()
