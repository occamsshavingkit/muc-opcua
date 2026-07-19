# Quickstart: Authorization Service Server Facet

## Prerequisites

- OpenSSL dev headers (`libssl-dev`) for host builds with RSA verification
- Python 3 with `cryptography` and `PyJWT` packages for test JWT generation

## Build

```bash
# Full profile (JWT enabled by default):
cmake -S . -B build/full -DMUC_OPCUA_PROFILE=full -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/full -j

# Verify JWT symbols present:
grep MUC_OPCUA_CU_USER_TOKEN_JWT build/full/kconfig_macros.h

# Standard profile (JWT off by default, enable manually):
cmake -S . -B build/standard -DMUC_OPCUA_PROFILE=standard \
  -DMUC_OPCUA_CU_USER_TOKEN_JWT=ON -DMUC_OPCUA_BUILD_TESTS=ON

# Verify compile-out (nano — no JWT code):
cmake -S . -B build/nano -DMUC_OPCUA_PROFILE=nano -DMUC_OPCUA_BUILD_TESTS=ON
cmake --build build/nano -j
# Expected: MU_MAX_ADDRESS_SPACE_NODES unchanged, no jwt.o in build
```

## Run Tests

```bash
# Unit tests (JWT parser, claims, signature validation):
ctest --test-dir build/full -R test_jwt --output-on-failure

# All tests:
ctest --test-dir build/full --output-on-failure
```

## Manual Verification

```bash
# Generate a test RSA key pair:
openssl genrsa -out /tmp/test_jwt_private.pem 2048
openssl rsa -in /tmp/test_jwt_private.pem -pubout -outform DER -out /tmp/test_jwt_public.der

# Generate a valid JWT (Python):
python3 -c "
import jwt, json, time
with open('/tmp/test_jwt_private.pem', 'rb') as f:
    key = f.read()
token = jwt.encode({
    'iss': 'test-issuer',
    'sub': 'operator1',
    'aud': 'opcua-server',
    'exp': int(time.time()) + 3600,
    'iat': int(time.time())
}, key, algorithm='RS256')
print(token)
" > /tmp/valid.jwt

# Run the integration test against a real server:
./build/full/tests/integration/test_jwt_activate_session \
  --jwt "$(cat /tmp/valid.jwt)" \
  --public-key /tmp/test_jwt_public.der
# Expected: Session activated with user identity "operator1"
```

## Profile Size Check

```bash
for p in nano micro embedded standard full; do
  cmake -S . -B build/size-$p -DMUC_OPCUA_PROFILE=$p -DMUC_OPCUA_BUILD_SIZE=ON
  cmake --build build/size-$p --target muc_opcua
  echo -n "$p: "
  size build/size-$p/src/libmuc_opcua.a | awk 'END{print $1}'
done

# Expected: nano/micro unchanged; embedded/standard grow ≤5 KB; full grows ≤5 KB
```
