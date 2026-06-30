# Contract: Benchmark Report Evidence

This contract documents the report fields required for optimization evidence. It
does not define a new runtime API.

## Required Top-Level Fields

- `schema`: Must identify the speed benchmark report schema.
- `generated_at_utc`: Must be an ISO-8601 UTC timestamp.
- `git_commit`: Must identify the commit measured.
- `build_type`: Must identify the build configuration.
- `host`: Must contain hostname, kernel release, machine, CPU model, isolated CPU
  set, kernel command line, and benchmark CPU governor when available.
- `scheduling`: Must contain CPU selection, realtime priority, affinity/realtime
  requirement flags, isolation requirement flag, sudo flag, and shielding flag.
- `thresholds`: Must contain minimum throughput ratio, maximum ROM growth ratio,
  and maximum static RAM growth ratio.
- `results`: Must contain benchmark result rows for all supported workload/profile
  combinations.

## Required Result Fields

- `profile`
- `scenario`
- `nodes`
- `batch`
- `supported`
- `ops_per_sec`
- `items_per_sec`
- `normalized_ops_per_sec`
- `text`
- `data`
- `bss`
- `cpu_affinity_requested`
- `cpu_affinity_applied`
- `realtime_priority_requested`
- `realtime_priority_applied`
- `sudo_requested`

## Authoritative Lab Criteria

A report is authoritative for this feature only when:

- `scheduling.cpu` is `11`.
- `scheduling.realtime_priority` is `80`.
- `scheduling.use_sudo` is `true`.
- `scheduling.require_affinity` is `true`.
- `scheduling.require_realtime` is `true`.
- `scheduling.require_isolated_cpu` is `true`.
- `host.isolated_cpus` includes `11`.
- Every supported result has `cpu_affinity_applied`, `realtime_priority_applied`,
  and `sudo_requested` set to `true`.

## Acceptance Rules

- A missing supported workload row fails comparison.
- A current unsupported row fails if the baseline row is supported.
- Normalized throughput is the primary speed metric when present.
- ROM and static RAM gates are evaluated per profile.
- A report that does not meet authoritative lab criteria may be used for local
  exploration but not for final optimization acceptance.
