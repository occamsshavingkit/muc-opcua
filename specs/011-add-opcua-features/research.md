# Research Notes: Custom Methods, Aes256_Sha256_RsaPss, Server Diagnostics, and Dynamic Node Management

## 1. Custom Method Calls (OPC-10000-4 §5.12.2)

### Current Architecture
* `Call` service handler is located in `src/core/service_dispatch.c` under `write_single_call_method_result`.
* It currently checks for numeric NodeIds `MU_ID_SERVER_GETMONITOREDITEMS` and `MU_ID_SERVER_RESENDDATA`. If any other NodeId is passed, it rejects it with `Bad_MethodInvalid`.
* Custom method execution requires looking up the object and method in the address space, locating the registered callback, decoding parameters, executing the callback, and returning the outputs.

### Proposed Callback Interface
```c
typedef opcua_statuscode_t (*mu_method_callback_t)(
    mu_server_t *server,
    const mu_nodeid_t *object_id,
    const mu_nodeid_t *method_id,
    const mu_variant_t *input_args,
    size_t input_args_count,
    mu_variant_t *output_args,
    size_t *output_args_count
);
```

## 2. Aes256_Sha256_RsaPss Security Policy (OPC-10000-7 §6.2)

### Cryptographic Parameter Summary
* **SecurityPolicy URI**: `http://opcfoundation.org/UA/SecurityPolicy#Aes256_Sha256_RsaPss`
* **Symmetric Encryption**: AES-256-CBC
* **Symmetric Signature**: HMAC-SHA-256
* **Asymmetric Signature**: RSASSA-PSS with SHA-256. Salt length = 32 bytes (hash length).
* **Asymmetric Encryption**: RSA-OAEP with SHA-256 (both hash and mask generation function use SHA-256).

### Adapter Requirements
The public `mu_crypto_adapter_t` interface needs to support RSA-PSS signing/verification and RSA-OAEP with SHA-256. We need to implement these primitives across:
* `src/platform/host_crypto_adapter.c` (OpenSSL backend)
* `src/platform/mbedtls_crypto_adapter.c` (Mbed TLS backend)
* `src/platform/wolfssl_crypto_adapter.c` (wolfSSL backend)

## 3. Server Diagnostics (OPC-10000-5 §6.x)

### Target Diagnostics Nodes
* Objects path: `Root -> Objects -> Server -> ServerDiagnostics -> ServerDiagnosticsSummary`
* Standard variables:
  * `ServerDiagnosticsSummary_CurrentSessionCount` (Numeric ID: 2287)
  * `ServerDiagnosticsSummary_AccumulatedSessionCount` (Numeric ID: 2288)
  * `ServerDiagnosticsSummary_SecurityRejectedSessionCount` (Numeric ID: 2292)
  * `ServerDiagnosticsSummary_CurrentSubscriptionCount` (Numeric ID: 2297)

## 4. Dynamic Node Management (OPC-10000-4 §5.5)

### Service Definitions
* **AddNodes**: Client requests server to add one or more nodes and references.
* **DeleteNodes**: Client requests server to delete one or more nodes and references.

### Memory Constraints
To maintain a freestanding, zero-heap runtime design:
* We will initialize a fixed-size node pool inside the address space context at startup.
* `AddNodes` will allocate nodes from this preallocated pool, avoiding dynamic heap allocation.
* `DeleteNodes` will mark the nodes as unused and return them to the pool.
