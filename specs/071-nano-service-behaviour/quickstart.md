<!-- markdownlint-disable MD013 -->

# Quickstart Validation

Run these checks after implementation tasks update code or generated artifacts.

## 1. Validate Manifest and Generated Artifacts

```sh
python3 scripts/profile_manifest/validate.py --all
```

Expected result: manifest validation succeeds and generated Kconfig/docs/claim maps are consistent.

## 2. Regenerate Profile Artifacts When Manifest Changes

```sh
python3 scripts/profile_manifest/generate.py \
  --manifest profiles/opcua-profile-manifest.yaml \
  --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
python3 scripts/profile_manifest/validate.py --all
```

Expected result: generated artifacts reflect the in-scope CU symbols and profile defaults.

## 3. Run Focused Service Tests

```sh
cmake --build build/pr-check --target test_discovery_services test_discovery_endpoint test_browse_service test_write_service test_write_errors test_session_auth test_session test_diagnostics
ctest --test-dir build/pr-check --output-on-failure -R 'discovery|browse|write|session|diagnostics'
```

Expected result: service behaviour tests pass for enabled CUs and disabled/unsupported paths return explicit StatusCodes.

## 4. Run Profile Gating Tests

```sh
bash scripts/test_profile_gating.sh
```

Expected result: nano includes only nano-default in-scope CUs; optional/micro+ CUs remain off unless their profile/default enables them; the `FIX-005` custom matrix disables each dedicated CU one at a time and proves only that CU behavior and claim become unavailable while unrelated enabled CUs remain available.

## 5. Run Standard Local Quality Gates

```sh
git diff --check
cmake --build build/pr-check --target format-check
cmake --build build/pr-check
ctest --test-dir build/pr-check --output-on-failure
```

Expected result: formatting, build, and tests pass.

## 6. Record Size Impact

```sh
scripts/measure_size.sh nano
```

Expected result: default nano growth is within the planned 0.5-2 KB budget or the size ledger explains the measured variance.
