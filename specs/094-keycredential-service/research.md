# Research: KeyCredential Service Server Facet

## R1: Method Dispatch Architecture

**Decision**: Register KeyCredential methods via the existing `mu_server_register_method_callback()` API from the Method Server Facet (spec 062). No new dispatch path needed.

**Rationale**: The Method Server Facet already provides arbitrary method dispatch via registered callbacks. KeyCredential methods (GetEncryptingKey, CreateCredential, UpdateCredential, DeleteCredential) are just 4 more methods on KeyCredentialConfigurationType/FolderType instances. The existing dispatch in `dispatch_method.c` routes by NodeId to the registered handler. Each method has a fixed NodeId from NodeIds.csv.

**Alternatives**: A dedicated KeyCredential dispatch table (unnecessary — the Method Server already handles this).

## R2: Credential Storage Adapter

**Decision**: Define a `mu_key_credential_adapter_t` struct with 4 function pointers, stored in `mu_server_config_t`. The adapter is integrator-provided.

**Rationale**: The library is embedded-first with no persistent storage. The adapter pattern matches existing adapters (crypto, time, entropy, mdns). The 4 operations map directly to the 4 method calls.

```c
typedef struct {
    opcua_statuscode_t (*get_encrypting_key)(void *ctx, const mu_string_t *resource_uri,
                                             mu_bytestring_t *out_public_key, mu_string_t *out_key_id);
    opcua_statuscode_t (*create_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_bytestring_t *credential, const mu_string_t *key_id);
    opcua_statuscode_t (*update_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_bytestring_t *credential, const mu_string_t *key_id);
    opcua_statuscode_t (*delete_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_string_t *key_id);
    void *context;
} mu_key_credential_adapter_t;
```

## R3: Address Space Type Nodes

**Decision**: Add KeyCredentialConfigurationType (17533) and KeyCredentialConfigurationFolderType (17496) InstanceDeclarations in base_nodes.c, gated on `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE && MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION`.

**Rationale**: These types are in namespace 0 (standard UA). The InstanceDeclarations follow the OPC-10000-12 §8.5-8.6 tables. The Method nodes (GetEncryptingKey=17516, etc.) are HasComponent children of the type's placeholder.

## R4: GetEncryptingKey Implementation

**Decision**: The default GetEncryptingKey implementation returns the server's application instance certificate public key. When the adapter provides its own `get_encrypting_key`, that takes precedence.

**Rationale**: The server already has a certificate (used for SecureChannel). Reusing it for KeyCredential encryption avoids a second key pair. Integrators who need a dedicated key provide it via the adapter.

## R5: Kconfig and Gating

**Decision**: Single Kconfig symbol `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE` with `depends on MUC_OPCUA_CU_METHOD_SERVER`. Default `y` for full profile only.

**Rationale**: The CU depends on the Method Server for method dispatch. One symbol covers both the type nodes and the method handlers.

## Validation Strategy

- **Unit tests**: Mock adapter, verify each method returns correct StatusCode. Test missing adapter → Bad_NotSupported.
- **Integration tests**: Full Call service round-trip with a configured KeyCredentialConfiguration instance.
- **Compile-out tests**: Verify zero growth when CU is undefined.
