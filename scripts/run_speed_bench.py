#!/usr/bin/env python3
"""Build and run the in-project micro-opcua hot-path speed benchmark."""

from __future__ import annotations

import argparse
import dataclasses
import datetime as _dt
import json
import os
import pathlib
import platform
import socket
import subprocess
import sys
from typing import Any


ROOT = pathlib.Path(__file__).resolve().parents[1]
DEFAULT_PROFILES = ("nano", "micro", "embedded", "full")
WRITE_PROFILES = frozenset(("micro", "embedded", "full"))
SUBSCRIPTION_PROFILES = frozenset(("micro", "embedded", "full"))
REQUIRED_REPORT_FIELDS = (
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
REQUIRED_THRESHOLD_FIELDS = (
    "min_ops_ratio",
    "max_text_growth_ratio",
    "max_ram_growth_ratio",
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


@dataclasses.dataclass(frozen=True)
class Workload:
    profile: str
    scenario: str
    nodes: int
    batch: int


def default_matrix(profiles: list[str] | tuple[str, ...]) -> list[Workload]:
    workloads: list[Workload] = []
    for profile in profiles:
        workloads.extend(
            [
                Workload(profile, "read-value", 1, 1),
                Workload(profile, "read-value", 32, 1),
                Workload(profile, "read-value", 32, 16),
                Workload(profile, "read-value", 128, 1),
                Workload(profile, "read-value", 128, 32),
                Workload(profile, "read-bad-node", 128, 16),
                Workload(profile, "message-chunk-header", 1, 1),
                Workload(profile, "message-chunk-header", 1, 32),
                Workload(profile, "binary-primitive", 1, 1),
                Workload(profile, "binary-primitive", 32, 32),
                Workload(profile, "uasc-response-framing", 1, 1),
                Workload(profile, "uasc-response-framing", 32, 32),
            ]
        )
        if profile in WRITE_PROFILES:
            workloads.extend(
                [
                    Workload(profile, "write-value", 1, 1),
                    Workload(profile, "write-value", 32, 16),
                    Workload(profile, "write-value", 128, 32),
                    Workload(profile, "write-bad-type", 128, 16),
                ]
            )
        if profile in SUBSCRIPTION_PROFILES:
            workloads.extend(
                [
                    Workload(profile, "subscription-idle-tick", 1, 1),
                    Workload(profile, "subscription-active-tick", 32, 8),
                ]
            )
    return workloads


def result_key(result: dict[str, Any]) -> tuple[str, str, int, int]:
    return (
        str(result["profile"]),
        str(result["scenario"]),
        int(result["nodes"]),
        int(result["batch"]),
    )


def _require_mapping(value: Any, path: str, failures: list[str]) -> dict[str, Any] | None:
    if not isinstance(value, dict):
        failures.append(f"{path}: expected object")
        return None
    return value


def _require_fields(mapping: dict[str, Any], fields: tuple[str, ...], path: str, failures: list[str]) -> None:
    for field in fields:
        if field not in mapping:
            failures.append(f"{path}.{field}: missing required field")


def validate_report_contract(report: dict[str, Any]) -> list[str]:
    failures: list[str] = []
    report_map = _require_mapping(report, "report", failures)
    if report_map is None:
        return failures

    for field in REQUIRED_REPORT_FIELDS:
        if field not in report_map:
            failures.append(f"{field}: missing required top-level field")

    host = report_map.get("host")
    if host is not None:
        host_map = _require_mapping(host, "host", failures)
        if host_map is not None:
            _require_fields(host_map, REQUIRED_HOST_FIELDS, "host", failures)

    thresholds = report_map.get("thresholds")
    if thresholds is not None:
        thresholds_map = _require_mapping(thresholds, "thresholds", failures)
        if thresholds_map is not None:
            _require_fields(thresholds_map, REQUIRED_THRESHOLD_FIELDS, "thresholds", failures)

    results = report_map.get("results")
    if results is not None:
        if not isinstance(results, list):
            failures.append("results: expected list")
        else:
            for index, result in enumerate(results):
                path = f"results[{index}]"
                result_map = _require_mapping(result, path, failures)
                if result_map is None:
                    continue
                if result_map.get("supported", True):
                    _require_fields(result_map, REQUIRED_RESULT_FIELDS, path, failures)

    return failures


def _cpu_set_contains(cpu_set: Any, cpu: int) -> bool:
    try:
        return cpu in parse_cpu_set(str(cpu_set))
    except ValueError:
        return False


def validate_authoritative_report(report: dict[str, Any]) -> list[str]:
    failures = validate_report_contract(report)
    if not isinstance(report, dict):
        return failures

    scheduling = report.get("scheduling")
    if isinstance(scheduling, dict):
        expected = {
            "cpu": "11",
            "realtime_priority": 80,
            "use_sudo": True,
            "require_affinity": True,
            "require_realtime": True,
            "require_isolated_cpu": True,
        }
        for field, value in expected.items():
            if scheduling.get(field) != value:
                failures.append(f"scheduling.{field}: expected {value!r}, got {scheduling.get(field)!r}")
        if "shield_cpu" in scheduling and scheduling.get("shield_cpu") is not True:
            failures.append(f"scheduling.shield_cpu: expected True, got {scheduling.get('shield_cpu')!r}")
    elif scheduling is not None:
        failures.append("scheduling: expected object")

    host = report.get("host")
    if isinstance(host, dict):
        isolated = host.get("isolated_cpus")
        if not _cpu_set_contains(isolated, 11):
            failures.append(f"host.isolated_cpus: expected isolated CPU set to include 11, got {isolated!r}")

    results = report.get("results")
    if isinstance(results, list):
        for index, result in enumerate(results):
            if not isinstance(result, dict) or not result.get("supported", True):
                continue
            for field in ("cpu_affinity_applied", "realtime_priority_applied", "sudo_requested"):
                if result.get(field) is not True:
                    failures.append(f"results[{index}].{field}: expected True for authoritative result")

    return failures


def compare_results(
    baseline: dict[str, Any],
    current: dict[str, Any],
    *,
    min_ops_ratio: float,
    max_text_growth_ratio: float,
    max_ram_growth_ratio: float,
) -> list[str]:
    failures: list[str] = []
    current_by_key = {result_key(item): item for item in current.get("results", [])}

    for base in baseline.get("results", []):
        if not base.get("supported", True):
            continue
        key = result_key(base)
        cur = current_by_key.get(key)
        label = f"{key[0]} {key[1]} nodes={key[2]} batch={key[3]}"
        if cur is None:
            failures.append(f"{label}: missing current result")
            continue
        if not cur.get("supported", True):
            failures.append(f"{label}: current result is unsupported")
            continue

        metric = "normalized_ops_per_sec"
        if metric not in base or metric not in cur:
            metric = "ops_per_sec"
        base_ops = float(base.get(metric, 0.0))
        cur_ops = float(cur.get(metric, 0.0))
        if base_ops > 0.0 and cur_ops < base_ops * min_ops_ratio:
            failures.append(
                f"{label}: throughput metric {metric}={cur_ops:.6f} is below "
                f"{min_ops_ratio:.3f}x baseline {base_ops:.6f}"
            )

        base_text = int(base.get("text", 0))
        cur_text = int(cur.get("text", 0))
        if base_text > 0 and cur_text > int(base_text * max_text_growth_ratio):
            failures.append(
                f"{label}: text {cur_text} bytes exceeds "
                f"{max_text_growth_ratio:.3f}x baseline {base_text} bytes"
            )

        base_ram = int(base.get("data", 0)) + int(base.get("bss", 0))
        cur_ram = int(cur.get("data", 0)) + int(cur.get("bss", 0))
        if base_ram > 0 and cur_ram > int(base_ram * max_ram_growth_ratio):
            failures.append(
                f"{label}: RAM {cur_ram} bytes exceeds "
                f"{max_ram_growth_ratio:.3f}x baseline {base_ram} bytes"
            )

    return failures


def run_command(cmd: list[str], *, cwd: pathlib.Path = ROOT) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, cwd=cwd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)


def benchmark_command(
    binary: pathlib.Path,
    scenario: str,
    *,
    nodes: int,
    batch: int,
    iterations: int,
    warmup: int,
    min_ms: int,
    cpu: str | None,
    realtime_priority: int = 0,
    use_sudo: bool = False,
) -> list[str]:
    command = [
        str(binary),
        "--scenario",
        scenario,
        "--nodes",
        str(nodes),
        "--batch",
        str(batch),
        "--iterations",
        str(iterations),
        "--warmup",
        str(warmup),
        "--min-ms",
        str(min_ms),
    ]
    if cpu and cpu.lower() != "none":
        command = ["taskset", "-c", cpu] + command
    if realtime_priority > 0:
        command = ["chrt", "-f", str(realtime_priority)] + command
    if use_sudo:
        command = ["sudo", "-n"] + command
    return command


def parse_cpu_set(spec: str) -> set[int]:
    cpus: set[int] = set()
    for part in spec.split(","):
        part = part.strip()
        if not part:
            continue
        if "-" in part:
            start_text, end_text = part.split("-", 1)
            start = int(start_text)
            end = int(end_text)
            if end < start:
                raise ValueError(f"invalid CPU range: {part}")
            cpus.update(range(start, end + 1))
        else:
            cpus.add(int(part))
    return cpus


def format_cpu_set(cpus: set[int]) -> str:
    return ",".join(str(cpu) for cpu in sorted(cpus))


def online_cpus() -> set[int]:
    online = pathlib.Path("/sys/devices/system/cpu/online")
    if online.exists():
        return parse_cpu_set(online.read_text(encoding="utf-8").strip())
    count = os.cpu_count() or 1
    return set(range(count))


def isolated_cpus() -> set[int]:
    isolated = pathlib.Path("/sys/devices/system/cpu/isolated")
    if isolated.exists():
        text = isolated.read_text(encoding="utf-8").strip()
        if text:
            return parse_cpu_set(text)
    return set()


def read_text_file(path: str) -> str:
    try:
        return pathlib.Path(path).read_text(encoding="utf-8").strip()
    except OSError:
        return ""


def validate_isolated_cpu(cpu: str | None) -> str:
    if not cpu or cpu.lower() == "none":
        return "no benchmark CPU selected"
    target = parse_cpu_set(cpu)
    isolated = isolated_cpus()
    missing = target - isolated
    if missing:
        return (
            f"CPU set {format_cpu_set(target)} is not fully isolated; "
            f"/sys/devices/system/cpu/isolated reports {format_cpu_set(isolated) or '<empty>'}"
        )
    return ""


def run_affinity_command(cpu_set: str, tid: int, use_sudo: bool) -> subprocess.CompletedProcess[str]:
    cmd = ["taskset", "-pc", cpu_set, str(tid)]
    if use_sudo:
        cmd = ["sudo", "-n"] + cmd
    return subprocess.run(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True)


def shield_cpu(cpu: str | None, *, use_sudo: bool = False) -> dict[str, Any]:
    if not cpu or cpu.lower() == "none":
        return {"requested": False}

    target = parse_cpu_set(cpu)
    available = online_cpus()
    replacement = available - target
    summary: dict[str, Any] = {
        "requested": True,
        "target": format_cpu_set(target),
        "available": format_cpu_set(available),
        "replacement": format_cpu_set(replacement),
        "threads_seen": 0,
        "threads_moved": 0,
        "threads_skipped": 0,
        "errors": 0,
        "use_sudo": use_sudo,
        "sample_errors": [],
    }
    if not replacement:
        summary["error"] = "no replacement CPUs available"
        return summary

    current_pid = os.getpid()
    for task_dir in pathlib.Path("/proc").glob("[0-9]*/task/[0-9]*"):
        try:
            tid = int(task_dir.name)
            pid = int(task_dir.parent.parent.name)
        except ValueError:
            continue
        if pid == current_pid:
            continue
        summary["threads_seen"] += 1
        try:
            affinity = os.sched_getaffinity(tid)
            if not (affinity & target):
                summary["threads_skipped"] += 1
                continue
            new_affinity = set(affinity) - target
            if not new_affinity:
                new_affinity = replacement
            if use_sudo:
                run_affinity_command(format_cpu_set(new_affinity), tid, use_sudo=True)
            else:
                os.sched_setaffinity(tid, new_affinity)
            summary["threads_moved"] += 1
        except (PermissionError, ProcessLookupError, OSError, subprocess.CalledProcessError) as exc:
            summary["errors"] += 1
            sample_errors = summary["sample_errors"]
            if isinstance(sample_errors, list) and len(sample_errors) < 5:
                if isinstance(exc, subprocess.CalledProcessError):
                    text = (exc.stderr or exc.stdout or str(exc)).strip()
                else:
                    text = str(exc)
                sample_errors.append(f"tid {tid}: {text}")
    return summary


def parse_profiles(value: str) -> list[str]:
    profiles = [part.strip() for part in value.split(",") if part.strip()]
    invalid = [profile for profile in profiles if profile not in DEFAULT_PROFILES]
    if invalid:
        raise argparse.ArgumentTypeError(f"unsupported profile(s): {', '.join(invalid)}")
    return profiles


def configure_profile(profile: str, build_root: pathlib.Path, build_type: str) -> pathlib.Path:
    build_dir = build_root / profile
    cmd = [
        os.environ.get("CMAKE", "cmake"),
        "-S",
        str(ROOT),
        "-B",
        str(build_dir),
        f"-DCMAKE_BUILD_TYPE={build_type}",
        "-DMICRO_OPCUA_BUILD_BENCHMARKS=ON",
        "-DMICRO_OPCUA_PLATFORM=host",
        f"-DMICRO_OPCUA_PROFILE={profile}",
    ]
    run_command(cmd)
    run_command([os.environ.get("CMAKE", "cmake"), "--build", str(build_dir), "--target", "hotpath_benchmark"])
    return build_dir


def parse_size_output(output: str) -> dict[str, int]:
    lines = [line.split() for line in output.splitlines() if line.strip()]
    for parts in reversed(lines):
        if len(parts) >= 4 and all(part.lstrip("-").isdigit() for part in parts[:4]):
            return {
                "text": int(parts[0]),
                "data": int(parts[1]),
                "bss": int(parts[2]),
                "dec": int(parts[3]),
            }
    return {"text": 0, "data": 0, "bss": 0, "dec": 0}


def measure_size(path: pathlib.Path) -> dict[str, int]:
    if not path.exists():
        return {"text": 0, "data": 0, "bss": 0, "dec": 0}
    cmd = [os.environ.get("SIZE", "size")]
    if path.suffix == ".a":
        cmd.append("-t")
    cmd.append(str(path))
    return parse_size_output(run_command(cmd).stdout)


def run_workload(
    workload: Workload,
    build_dir: pathlib.Path,
    *,
    iterations: int,
    warmup: int,
    min_ms: int,
    cpu: str | None,
    realtime_priority: int,
    require_realtime: bool,
    require_affinity: bool,
    use_sudo: bool,
) -> dict[str, Any]:
    binary = build_dir / "tests" / "benchmark" / "hotpath_benchmark"
    cmd = benchmark_command(
        binary,
        workload.scenario,
        nodes=workload.nodes,
        batch=workload.batch,
        iterations=iterations,
        warmup=warmup,
        min_ms=min_ms,
        cpu=cpu,
        realtime_priority=realtime_priority,
        use_sudo=use_sudo,
    )
    realtime_applied = realtime_priority > 0
    affinity_applied = bool(cpu and cpu.lower() != "none")
    scheduling_error = ""
    try:
        completed = run_command(cmd)
    except subprocess.CalledProcessError as exc:
        if realtime_priority > 0 and not require_realtime:
            scheduling_error = (exc.stderr or exc.stdout or str(exc)).strip()
            realtime_applied = False
            cmd = benchmark_command(
                binary,
                workload.scenario,
                nodes=workload.nodes,
                batch=workload.batch,
                iterations=iterations,
                warmup=warmup,
                min_ms=min_ms,
                cpu=cpu,
                realtime_priority=0,
                use_sudo=use_sudo,
            )
            try:
                completed = run_command(cmd)
            except subprocess.CalledProcessError:
                if require_affinity:
                    raise
                affinity_applied = False
                cmd = benchmark_command(
                    binary,
                    workload.scenario,
                    nodes=workload.nodes,
                    batch=workload.batch,
                    iterations=iterations,
                    warmup=warmup,
                    min_ms=min_ms,
                    cpu=None,
                    realtime_priority=0,
                    use_sudo=use_sudo,
                )
                completed = run_command(cmd)
        elif affinity_applied and not require_affinity:
            affinity_applied = False
            scheduling_error = (exc.stderr or exc.stdout or str(exc)).strip()
            cmd = benchmark_command(
                binary,
                workload.scenario,
                nodes=workload.nodes,
                batch=workload.batch,
                iterations=iterations,
                warmup=warmup,
                min_ms=min_ms,
                cpu=None,
                realtime_priority=0,
                use_sudo=use_sudo,
            )
            completed = run_command(cmd)
        else:
            raise

    result = json.loads(completed.stdout)
    result["profile"] = workload.profile
    result["build_dir"] = str(build_dir)
    result["cpu_affinity_requested"] = cpu
    result["cpu_affinity_applied"] = affinity_applied
    result["realtime_priority_requested"] = realtime_priority
    result["realtime_priority_applied"] = realtime_applied
    result["sudo_requested"] = use_sudo
    if scheduling_error:
        result["scheduling_error"] = scheduling_error
    library_size = measure_size(build_dir / "src" / "libmicro_opcua.a")
    result.update(library_size)
    binary_size = measure_size(binary)
    result["binary_text"] = binary_size["text"]
    result["binary_data"] = binary_size["data"]
    result["binary_bss"] = binary_size["bss"]
    result["binary_dec"] = binary_size["dec"]
    return result


def git_commit() -> str:
    try:
        return run_command(["git", "rev-parse", "HEAD"]).stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        return "unknown"


def build_report(results: list[dict[str, Any]], args: argparse.Namespace) -> dict[str, Any]:
    cpuinfo = read_text_file("/proc/cpuinfo")
    model_name = ""
    for line in cpuinfo.splitlines():
        if line.startswith("model name"):
            model_name = line.split(":", 1)[1].strip()
            break
    return {
        "schema": "micro-opcua-speed-benchmark-v1",
        "generated_at_utc": _dt.datetime.now(_dt.timezone.utc).isoformat(),
        "git_commit": git_commit(),
        "build_type": args.build_type,
        "iterations": args.iterations,
        "warmup": args.warmup,
        "min_ms": args.min_ms,
        "host": {
            "hostname": socket.gethostname(),
            "system": platform.system(),
            "release": platform.release(),
            "machine": platform.machine(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "cpu_count": os.cpu_count(),
            "cpu_model": model_name,
            "isolated_cpus": format_cpu_set(isolated_cpus()),
            "kernel_cmdline": read_text_file("/proc/cmdline"),
            "benchmark_cpu_governor": read_text_file(f"/sys/devices/system/cpu/cpu{args.cpu}/cpufreq/scaling_governor")
            if args.cpu.isdigit()
            else "",
        },
        "thresholds": {
            "min_ops_ratio": args.min_ops_ratio,
            "max_text_growth_ratio": args.max_text_growth_ratio,
            "max_ram_growth_ratio": args.max_ram_growth_ratio,
        },
        "scheduling": {
            "cpu": args.cpu,
            "realtime_priority": args.realtime_priority,
            "require_affinity": args.require_affinity,
            "require_realtime": args.require_realtime,
            "shield_cpu": args.shield_cpu,
            "use_sudo": args.use_sudo,
            "require_isolated_cpu": args.require_isolated_cpu,
        },
        "shielding": args.shielding_summary,
        "results": results,
    }


def write_json(path: pathlib.Path, report: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def load_json(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--profiles", type=parse_profiles, default=list(DEFAULT_PROFILES))
    parser.add_argument("--build-root", type=pathlib.Path, default=ROOT / "build" / "speed-bench")
    parser.add_argument("--build-type", default="Release")
    parser.add_argument("--iterations", type=int, default=20000)
    parser.add_argument("--warmup", type=int, default=2000)
    parser.add_argument("--min-ms", type=int, default=200)
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "docs" / "benchmarks" / "speed-current.json")
    parser.add_argument("--baseline", type=pathlib.Path, default=ROOT / "docs" / "benchmarks" / "speed-baseline.json")
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--compare", action="store_true")
    parser.add_argument("--matrix", action="store_true", help="Print the workload matrix and exit")
    parser.add_argument("--skip-build", action="store_true", help="Reuse existing build directories")
    parser.add_argument("--cpu", default="11", help="CPU set for benchmark execution; use 'none' to disable")
    parser.add_argument("--shield-cpu", action="store_true", help="Best-effort move other user-accessible threads off --cpu")
    parser.add_argument("--use-sudo", action="store_true", help="Use passwordless sudo (-n) for chrt/taskset operations")
    parser.add_argument("--realtime-priority", type=int, default=80, help="SCHED_FIFO priority to try; 0 disables")
    parser.add_argument("--require-affinity", action="store_true", help="Fail instead of falling back when taskset fails")
    parser.add_argument("--require-realtime", action="store_true", help="Fail instead of falling back when chrt fails")
    parser.add_argument("--require-isolated-cpu", action="store_true", help="Fail unless --cpu is listed by Linux as isolated")
    parser.add_argument("--min-ops-ratio", type=float, default=0.85)
    parser.add_argument("--max-text-growth-ratio", type=float, default=1.03)
    parser.add_argument("--max-ram-growth-ratio", type=float, default=1.05)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    matrix = default_matrix(args.profiles)

    if args.matrix:
        for workload in matrix:
            print(
                f"{workload.profile:8s} {workload.scenario:26s} "
                f"nodes={workload.nodes:<3d} batch={workload.batch}"
            )
        return 0

    if args.require_isolated_cpu:
        isolation_error = validate_isolated_cpu(args.cpu)
        if isolation_error:
            print(isolation_error, file=sys.stderr)
            return 2

    args.shielding_summary = shield_cpu(args.cpu, use_sudo=args.use_sudo) if args.shield_cpu else {"requested": False}

    build_dirs: dict[str, pathlib.Path] = {}
    for profile in args.profiles:
        build_dir = args.build_root / profile
        if not args.skip_build:
            build_dir = configure_profile(profile, args.build_root, args.build_type)
        build_dirs[profile] = build_dir

    results = [
        run_workload(
            workload,
            build_dirs[workload.profile],
            iterations=args.iterations,
            warmup=args.warmup,
            min_ms=args.min_ms,
            cpu=args.cpu,
            realtime_priority=args.realtime_priority,
            require_realtime=args.require_realtime,
            require_affinity=args.require_affinity,
            use_sudo=args.use_sudo,
        )
        for workload in matrix
    ]
    report = build_report(results, args)

    output = args.baseline if args.update_baseline else args.output
    write_json(output, report)
    print(f"wrote {output}")

    if args.compare:
        if not args.baseline.exists():
            print(f"baseline not found: {args.baseline}", file=sys.stderr)
            return 2
        failures = compare_results(
            load_json(args.baseline),
            report,
            min_ops_ratio=args.min_ops_ratio,
            max_text_growth_ratio=args.max_text_growth_ratio,
            max_ram_growth_ratio=args.max_ram_growth_ratio,
        )
        if failures:
            print("benchmark comparison failed:", file=sys.stderr)
            for failure in failures:
                print(f"  - {failure}", file=sys.stderr)
            return 1
        print("benchmark comparison passed")

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
