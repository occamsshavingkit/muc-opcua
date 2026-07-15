<!-- markdownlint-disable MD013 -->

# Quickstart / Validation: Nano SecurityPolicy-None Crypto Gating

**Feature**: 072-nano-securitypolicy-none-gating

Reproduces the four contracts (C1–C4) and the size result. Run from the repo root.

## 1. Regenerate + validate manifest artifacts

```sh
python3 scripts/profile_manifest/generate.py --manifest profiles/opcua-profile-manifest.yaml \
  --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs,completion
python3 scripts/profile_manifest/validate.py --all        # manifest: OK (drift-free)
```

## 2. C1 + C2 + C3 — nano, crypto gated off

```sh
cmake -S . -B build/nano -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON \
  -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_PROFILE=nano
cmake --build build/nano
# Gate off + crypto TUs absent:
grep -o 'MUC_OPCUA_SECURE_CHANNEL_CRYPTO=[01]' build/nano/compile_commands.json | sort -u   # expect: (none / =0)
# None-path + non-None-rejection tests:
ctest --test-dir build/nano --output-on-failure -R 'secure_channel|session|discovery|browse|read|handshake|framing|dispatch'
```

## 3. C4 — secured profiles unchanged

```sh
for p in micro embedded standard full; do
  cmake -S . -B build/$p -DCMAKE_BUILD_TYPE=Debug -DMUC_OPCUA_BUILD_TESTS=ON \
    -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PROFILE=$p
  cmake --build build/$p
  grep -o 'MUC_OPCUA_SECURE_CHANNEL_CRYPTO=1' build/$p/compile_commands.json | head -1   # expect: =1
  ctest --test-dir build/$p --output-on-failure
done
```

## 4. Gating + size

```sh
bash scripts/test_profile_gating.sh          # asserts gate off@nano, on@secured
# ARM size (nano recovery ~5.8-6.0 KB; secured byte-identical):
scripts/measure_size.sh nano                 # compare .text against the ledger baseline
```

## 5. Full pr-check parity

```sh
cmake --build build/pr-check && ctest --test-dir build/pr-check --output-on-failure
python3 -m pytest scripts/profile_manifest/ tests/conformance/ -q
git diff --check                             # clean
```

Expected: all green; nano `.text` down by the validated delta with no BSS change; secured profiles byte-identical; manifest/docs consistent with the gated build.
