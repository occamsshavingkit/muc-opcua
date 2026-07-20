# Conformance: KeyCredential Service Server Facet (spec 094)

This server implements the OPC UA **KeyCredential Service Server Facet**
(OPC-10000-7 PG18, CU 2113) — credential management for OAuth2/OIDC broker
authentication per OPC-10000-12 §8.5-8.6. Gated behind
**`MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE`** (default **ON** for the Full profile;
depends on `MUC_OPCUA_CU_METHOD_SERVER`).

Grounded against:
- OPC-10000-4 §5.11 (Call service)
- OPC-10000-7 v1.05.02 CU 2113 (KeyCredential Service Server Facet)
- OPC-10000-12 §8.5 (KeyCredentialConfigurationType) and §8.6
  (KeyCredentialConfigurationFolderType)

## Scope

| Method | NodeId | On Type | Priority |
|---|---:|---|---|
| `GetEncryptingKey` | 17516 | KeyCredentialConfigurationType (17533) | P1 |
| `CreateCredential` | 17522 | KeyCredentialConfigurationFolderType (17496) | P2 |
| `UpdateCredential` | 17519 | KeyCredentialConfigurationFolderType (17496) | P3 |
| `DeleteCredential` | 17521 | KeyCredentialConfigurationFolderType (17496) | P3 |

The library treats credentials as opaque ByteStrings — it never decrypts.
`GetEncryptingKey` returns a DER-encoded public key plus keyId; the integrator
uses these to encrypt a credential before submitting it via `CreateCredential`.

## Adapter Interface

```c
typedef struct {
    opcua_statuscode_t (*get_encrypting_key)(void *ctx, const mu_string_t *resource_uri,
                                             mu_bytestring_t *out_public_key,
                                             mu_string_t *out_key_id);
    opcua_statuscode_t (*create_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_bytestring_t *credential,
                                            const mu_string_t *key_id);
    opcua_statuscode_t (*update_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_bytestring_t *credential,
                                            const mu_string_t *key_id);
    opcua_statuscode_t (*delete_credential)(void *ctx, const mu_string_t *resource_uri,
                                            const mu_string_t *key_id);
    void *context;
} mu_key_credential_adapter_t;
```

Stored on `mu_server_config_t.key_credential_adapter`. When the pointer is
NULL (or any of the four function pointers is NULL), every method returns
`Bad_NotSupported` (SC-003) — the method nodes remain browsable so clients can
discover the facet even on a server without a backing credential store.

## Registration

`mu_server_init()` calls `mu_key_credential_register(server)` automatically
when the CU is enabled, registering the four method callbacks via the existing
Method Server Facet (`mu_server_register_method_callback()`). The callbacks
consume 4 of the `MU_MAX_REGISTERED_METHODS` (8) slots.

## Address Space

When `MUC_OPCUA_CU_BASE_INFO_TYPE_INFORMATION` is also enabled, the
InstanceDeclarations for both ObjectTypes are exposed in namespace 0:

- `KeyCredentialConfigurationFolderType` (17496) — subtype of BaseObjectType
  (58); children `<ServiceName>` (17511), `CreateCredential` (17522),
  `UpdateCredential` (17519), `DeleteCredential` (17521).
- `KeyCredentialConfigurationType` (17533) — subtype of BaseObjectType (58);
  children `ResourceUri` (17512), `ProfileUri` (17513), `EndpointUrls`
  (17514), `ServiceStatus` (17515), `GetEncryptingKey` (17516) plus the
  method argument Property nodes 17517-17518, 17520, 17523-17524.

## StatusCodes

| StatusCode | When |
|---|---|
| `Bad_NotSupported` (0x803D0000) | No adapter configured |
| `Bad_NoData` (0x809B0000) | Adapter has no encrypting key for ResourceUri |
| `Bad_NoEntryExists` (0x80A00000) | Update/Delete on unknown keyId |
| `Bad_InvalidArgument` (0x80AB0000) | Wrong input argument type |
| `Bad_ArgumentsMissing` (0x80AB0000) | Too few input arguments |

## Out of Scope

- Credential decryption (server stores opaque blobs).
- Push/Pull notification flows.
- `KeyCredentialAuditEvent` types (audit logging deferred).
- Online certificate enrollment with a CA.
- Key rotation automation.

## Backing Tests

- `tests/unit/test_key_credential.c` — handler dispatch, missing adapter,
  Bad_NoEntryExists paths, wrong argument types.
