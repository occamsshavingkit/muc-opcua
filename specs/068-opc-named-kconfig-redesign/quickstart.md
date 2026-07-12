# Quickstart: OPC-Named Kconfig Redesign

## Developer Verification Path

### 1. Validate manifest structure

```sh
python3 scripts/profile_manifest/validate.py --manifest-only
# Expected: manifest: OK
```

### 2. Regenerate all artifacts

```sh
python3 scripts/profile_manifest/generate.py \
    --manifest profiles/opcua-profile-manifest.yaml \
    --outputs kconfig,defconfigs,capacities_h,claim_map,roadmap,build_docs
# Expected: writes Kconfig, configs/*.defconfig, include/muc_opcua/capacities.h,
#   tests/conformance/claim_test_map.md, docs/conformance/opc-profile-roadmap.md,
#   updated docs/build-and-gating.md
```

### 3. Check generated files pass drift and naming validation

```sh
python3 scripts/profile_manifest/validate.py --all
# Expected: manifest: OK
```

### 4. Verify Kconfig parses with kconfiglib (all profiles)

```sh
# Done as part of --all above, but can be isolated:
python3 -c "
import os
import sys; sys.path.insert(0, 'scripts/kconfig')
os.environ['CONFIG_'] = ''
import kconfiglib
for pk in ['nano','micro','embedded','standard','full','custom']:
    kconf = kconfiglib.Kconfig('Kconfig', warn=False)
    kconf.load_config('configs/' + pk + '.defconfig')
    print('  profile', pk, 'parsed OK')
"
```

### 5. Verify naming convention and OPC references

```sh
python3 scripts/profile_manifest/validate.py --all
# Expected: manifest: OK, no redundant symbol suffixes, and every generated
# Profile/Facet/CU prompt is grounded in its manifest OPC reference.
```

### 6. Run profile gating tests with new symbols

```sh
bash scripts/test_profile_gating.sh
# Expected: all checks pass
```

### 7. Full test suite (standard profile)

```sh
cmake -S . -B build/standard-check \
    -DMUC_OPCUA_PROFILE=standard \
    -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/standard-check
ctest --test-dir build/standard-check --output-on-failure
# Expected: all configured tests pass. Standard profile configures 116 tests
# because full-only feature tests are not added when those features are off.
```

### 8. Full test suite (full profile)

```sh
cmake -S . -B build/full-check \
    -DMUC_OPCUA_PROFILE=full \
    -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/full-check
ctest --test-dir build/full-check --output-on-failure
# Expected: 132/132 tests passed
```

### 9. Capacity override still works

```sh
cmake -S . -B build/capacity-check \
    -DMUC_OPCUA_PROFILE=standard \
    -DMU_MAX_SESSIONS=42 \
    -DMUC_OPCUA_BUILD_TESTS=OFF \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/capacity-check
grep -r "MU_MAX_SESSIONS" build/capacity-check/compile_commands.json | head -1
# Expected: -DMU_MAX_SESSIONS=42 in compile command
```

### 10. Verify no old symbols remain in config output

```sh
# Old project-centric generated Kconfig symbols should no longer appear in .config.
# Internal compatibility gates such as MUC_OPCUA_SECURITY may still appear in
# muc_opcua_config.cmake while source-level migration is in progress.
cmake -S . -B build/no-legacy-check -DMUC_OPCUA_PROFILE=standard
grep -E 'MUC_OPCUA_SERVICE_READ|MUC_OPCUA_SERVICE_BROWSE|MUC_OPCUA_BASE_NODES|MUC_OPCUA_SECURITY[^_]' \
    build/no-legacy-check/.config
# Expected: no output (symbols removed)
```

### 11. Kconfig menu structure

```sh
# Verify the profile choice shows OPC names
python3 -c "
with open('Kconfig') as f:
    content = f.read()
assert 'Standard 2017 UA Server Profile' in content, 'Missing OPC profile name'
assert 'Facet: Core 2017 Server' in content, 'Missing Facet label'
assert 'CU: Attribute Read' in content, 'Missing CU label'
print('Kconfig structure: OK')
"
```
