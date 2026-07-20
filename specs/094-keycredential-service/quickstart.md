# Quickstart: KeyCredential Service Server Facet

## Build

```bash
# Full profile (KeyCredential enabled by default):
cmake -S . -B build/full -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/full -j

# Verify compile-out (nano):
cmake -S . -B build/nano -DMUC_OPCUA_PROFILE=nano
cmake --build build/nano -j
# Expected: zero key_credential.o in build
```

## Run Tests

```bash
ctest --test-dir build/full -R test_key_credential --output-on-failure
```

## Manual Verification

```bash
# GetEncryptingKey on a configured instance:
# 1. Server must have a KeyCredentialConfiguration instance in its address space
# 2. Client calls Call service with MethodId=17516
# 3. Response contains DER public key + keyId
```

## Size Check

```bash
for p in nano embedded standard full; do
  cmake -S . -B build/size-$p -DMUC_OPCUA_PROFILE=$p
  cmake --build build/size-$p --target muc_opcua
done
# Expected: nano/micro/embedded unchanged; full grows ≤3 KB
```
