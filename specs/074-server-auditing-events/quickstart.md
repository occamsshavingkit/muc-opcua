<!-- markdownlint-disable MD013 -->

# Quickstart / Validation: Server-Emitted AuditEvents

**Feature**: 074-server-auditing-events

## 1. Manifest + gating

```sh
python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml \
  --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs,completion
python3 scripts/profile_manifest/validate.py --all
bash scripts/test_profile_gating.sh          # auditing gate off@nano, on where enabled
```

## 2. C2/C3 — emit + observe E2E (auditing-enabled profile)

```sh
cmake -S . -B build/full -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON \
  -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PROFILE=full
cmake --build build/full
# Subscribe to Server EventNotifier -> perform auditable action -> assert AuditEvent published:
ctest --test-dir build/full --output-on-failure -R 'event_notifications|audit|read_attribute|secure_channel|session'
```

## 3. C1 — Server EventNotifier readable

```sh
ctest --test-dir build/full -R 'read_attribute|discovery_endpoint' --output-on-failure
# reading Server (i=2253) EventNotifier (attr 12) returns 0x01 (SubscribeToEvents)
```

## 4. C4 — compiles out when auditing off

```sh
cmake -S . -B build/nano -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PROFILE=nano
cmake --build build/nano
grep -c 'MUC_OPCUA_CU_AUDITING=1' build/nano/compile_commands.json   # expect 0
scripts/measure_size.sh nano   # unchanged vs pre-074 baseline
```

## 5. Full matrix + size

```sh
for p in nano micro embedded standard full; do ctest --test-dir build/$p; done
python3 -m pytest scripts/profile_manifest/ tests/conformance/ -q
scripts/measure_size.sh all    # record auditing .text cost where enabled
```
