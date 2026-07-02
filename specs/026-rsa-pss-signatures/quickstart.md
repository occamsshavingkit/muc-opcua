# Quickstart: SecurityPolicy-aware RSA-PSS Signatures

## Build & test (host)

```bash
cmake -S . -B build/host -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_BUILD_EXAMPLES=ON \
      -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON
cmake --build build/host -j
ctest --test-dir build/host --output-on-failure
```

## Verify

### PSS policy now works (US1)

```bash
ctest --test-dir build/host -R test_secure_handshake_modern --output-on-failure
```
- The `Aes256_Sha256_RsaPss` sub-test signs the ClientSignature with PSS and the
  ServerSignature is PSS with the `rsa-pss-sha2-256` URI → full handshake succeeds.

### PKCS#1.5 policies unchanged (US2) + algorithm mismatch rejected (US3)

```bash
ctest --test-dir build/host -R 'secure_handshake|server_handshake_secure|response_regression' --output-on-failure
```
- Basic256Sha256 / Aes128 handshakes byte-identical to before.
- The new negative case: PSS channel + PKCS#1.5 ClientSignature URI → rejected.

### Gates

```bash
scripts/measure_size.sh all      # .text <= +3%, data+bss <= +5%, no new heap
scripts/check_build_matrix.sh    # four ARM profiles green
# ASAN
CC=clang cmake -S . -B build/asan -DMUC_OPCUA_BUILD_TESTS=ON -DMUC_OPCUA_SANITIZERS="address,undefined" \
      -DMUC_OPCUA_PLATFORM=host -DMUC_OPCUA_HAVE_MBEDTLS=ON -DMUC_OPCUA_HAVE_WOLFSSL=ON -DMUC_OPCUA_PUBSUB=ON
cmake --build build/asan -j && ctest --test-dir build/asan --output-on-failure
```

## Interop (optional, needs .NET SDK)

The OPC Foundation reference client selects the most-secure endpoint. With PSS now
correct, a client that picks `Aes256_Sha256_RsaPss` will interoperate:

```bash
tests/interop/run_interop_dotnet.sh
```

## Definition of done

- SC-001..SC-005 met: PSS handshake works; ServerSignature URI/scheme correct per
  policy; wrong-algorithm/scheme ClientSignatures rejected; PKCS#1.5/None
  byte-identical; full ctest + ASAN + four ARM profiles green within gates.
