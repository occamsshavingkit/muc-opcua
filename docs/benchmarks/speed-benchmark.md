# Speed Benchmark Baseline

`scripts/run_speed_bench.py` builds a host Release benchmark for each profile and
runs deterministic in-process service-dispatch workloads. It avoids localhost TCP
noise so changes in the binary codec, service dispatch, address-space lookup,
Write handling, and subscription tick path are easier to compare.

Each workload runs at least the requested iteration count and at least 200 ms by
default. The minimum duration keeps very fast paths from producing unstable
few-millisecond samples.

The project benchmark targets are lab-mode by default. They pin each benchmark
process to isolated CPU 11 and run it as `SCHED_FIFO` priority 80 via
`sudo -n chrt -f 80 taskset -c 11 ...`. They also require Linux to report CPU 11
as isolated, require affinity and realtime scheduling to succeed, and make a
best-effort `sudo -n taskset -pc ...` pass to move other threads off CPU 11 before
timing begins.

The intended lab host is PREEMPT_RT with CPU 11 isolated, for example with
`isolcpus=managed_irq,domain,11 nohz_full=11 rcu_nocbs=11 irqaffinity=0-10`.
Passwordless sudo must allow `sudo -n chrt ...` and `sudo -n taskset ...`;
otherwise `make speed-current`, `make speed-baseline`, and `make speed-compare`
fail instead of recording a noisy run.

## Commands

```sh
make speed-matrix
make speed-smoke
make speed-current
make speed-baseline
make speed-compare
```

`make speed-smoke` is the non-strict local check. It pins to CPU 11 but does not
request realtime scheduling or passwordless sudo.

The default baseline file is `docs/benchmarks/speed-baseline.json`. Current runs
write `docs/benchmarks/speed-current.json`.

## Evidence Status

Authoritative optimization evidence must come from the controlled lab path, not
from exploratory local timing. A report is authoritative only when it satisfies the
benchmark report contract: CPU `11`, realtime priority `80`, passwordless
`sudo -n`, required affinity, required realtime scheduling, required isolated CPU
validation, and best-effort CPU shielding enabled. The host fingerprint must record
the benchmark host and isolation context, including hostname, kernel release, CPU
model, isolated CPU set, kernel command line, benchmark CPU governor, and shielding
summary.

Supported result rows must include the contracted result fields, including profile,
scenario, node and batch counts, throughput metrics, library `text`, `data`, and
`bss`, requested/applied CPU affinity, requested/applied realtime priority, and
whether sudo was requested. For authoritative evidence, every supported row must
show affinity applied, realtime priority applied, and sudo requested.

Exploratory runs, including `make speed-smoke` and ad hoc local runs that relax
affinity, realtime, isolation, sudo, or shielding requirements, are useful for
screening candidates. They are not final acceptance evidence and must not be used
to claim an optimization result.

## Workloads

The default matrix covers `nano`, `micro`, `embedded`, and `full` profiles.

- Read Value: OPC-10000-4 section 5.11.2, single item, 32-node lookup,
  16-item batch, 128-node lookup, and 32-item batch at the service dispatch
  limit.
- Read Bad NodeId: OPC-10000-4 section 5.11.2 and section 7.39 StatusCode
  handling, negative path over a 128-node address space.
- Write Value: OPC-10000-4 section 5.11.4, enabled profiles only, same
  single/batch/address-space shape.
- Write Bad Type: OPC-10000-4 section 5.11.4, enabled profiles only, negative
  path through type validation.
- Subscription idle tick: OPC-10000-4 section 5.14, subscription profiles only,
  no active subscriptions.
- Subscription active tick: OPC-10000-4 sections 5.13 and 5.14, subscription
  profiles only, eight active monitored items over a 32-node address space.

`ops_per_sec` is service requests or tick calls per second. `items_per_sec` is
`ops_per_sec * batch`. Each row also records a small same-process calibration loop
and `normalized_ops_per_sec`; comparison uses the normalized value when present so
CPU frequency and host load changes do not dominate the result. `text`, `data`,
and `bss` are from `src/libmicro_opcua.a` for the matching profile build.
`binary_*` fields measure the benchmark executable and are secondary. Scheduling
fields record requested/applied CPU affinity and realtime priority.

The report also records a host fingerprint: hostname, kernel release, CPU model,
online isolation mask, kernel command line, benchmark CPU governor, and the
shielding summary. Compare lab results only against baselines from the same known
hardware and isolation setup.

## Comparison

`make speed-compare` fails when any baseline-supported workload:

- Drops below `85%` of baseline normalized throughput
- Grows library `.text` by more than `3%`
- Grows library `.data + .bss` by more than `5%`

These thresholds catch obvious regressions without treating normal host timing
jitter as a failure. For PR evidence, keep the JSON artifact and note the host,
commit, and build type recorded in the file.
