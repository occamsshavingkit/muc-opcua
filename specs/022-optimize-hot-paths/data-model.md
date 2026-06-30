# Data Model: Optimize Hot Paths

## Benchmark Baseline

Represents the authoritative pre-optimization measurement set.

**Fields**
- `schema`: Report schema identifier.
- `generated_at_utc`: Time of capture.
- `git_commit`: Commit measured.
- `build_type`: Build configuration.
- `host`: Host fingerprint including CPU model, kernel, isolation mask, governor.
- `scheduling`: Benchmark CPU, realtime priority, sudo, isolation, and shielding
  settings.
- `thresholds`: Throughput and resource comparison gates.
- `results`: Collection of benchmark result rows.

**Validation Rules**
- Must be captured in the controlled lab mode before behavior changes begin.
- Must include all expected profile/workload rows.
- Must record isolated CPU 11, realtime priority 80, and passwordless sudo usage
  for authoritative comparisons.

## Benchmark Result

Represents one workload measurement for one profile.

**Fields**
- `profile`: Profile name.
- `scenario`: Workload scenario.
- `nodes`: Address-space size for the workload.
- `batch`: Operation batch size.
- `ops_per_sec`: Operations per second.
- `items_per_sec`: Items per second where applicable.
- `normalized_ops_per_sec`: Throughput normalized against same-process calibration.
- `text`, `data`, `bss`: Library resource measurements.
- `cpu_affinity_applied`, `realtime_priority_applied`, `sudo_requested`: Scheduling
  evidence.

**Validation Rules**
- `supported` rows are the only rows used for throughput acceptance.
- Missing current rows fail comparison.
- Resource fields must be present for every supported row.
- Batch size must remain within the current dispatch limit.

## Hot-Path Workload

Represents a repeatable exercise of an implemented protocol path.

**Fields**
- `profile`.
- `scenario`.
- `nodes`.
- `batch`.
- `normative_scope`: OPC UA sections governing behavior.
- `resource_risk`: Whether implementation may affect ROM, RAM, stack, heap, or
  throughput.

**Relationships**
- Belongs to one benchmark baseline.
- Produces one benchmark result per run.
- Maps to one protocol behavior contract.

## Resource Budget

Represents acceptance limits for embedded headroom.

**Fields**
- `max_rom_growth_ratio`: 1.03.
- `max_static_ram_growth_ratio`: 1.05.
- `heap_use`: Must remain none for hot paths.
- `stack_growth`: Must be measured or justified if changed.
- `static_tables`: Must be absent or net-benefit justified.

**Validation Rules**
- Evaluated per profile, not globally.
- A change that passes a larger profile but fails a smaller profile is incomplete.

## Protocol Behavior Contract

Represents the observable OPC UA behavior that must not change.

**Fields**
- `normative_sections`: Exact OPC UA sections.
- `positive_cases`: Existing valid request/response behaviors.
- `negative_cases`: Malformed input, unsupported behavior, and StatusCode cases.
- `compatibility_claim`: Existing profile/service/encoding/transport claim.

**State Transitions**
- `Baseline`: Existing behavior captured by tests and fixtures.
- `Candidate`: Optimization under review with protective tests.
- `Accepted`: Candidate passes conformance, negative-path, resource, and benchmark
  gates.
- `Rejected`: Candidate changes behavior or fails resource/throughput gates.
