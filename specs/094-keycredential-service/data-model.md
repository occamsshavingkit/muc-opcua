# Data Model: KeyCredential Service Server Facet

## Entities

### mu_key_credential_adapter_t

Integrator-provided credential storage adapter, embedded in `mu_server_config_t`.

| Field | Type | Description |
|-------|------|-------------|
| `get_encrypting_key` | `opcua_statuscode_t (*)(void*, const mu_string_t*, mu_bytestring_t*, mu_string_t*)` | Return DER public key + keyId for the given ResourceUri |
| `create_credential` | `opcua_statuscode_t (*)(void*, const mu_string_t*, const mu_bytestring_t*, const mu_string_t*)` | Store a new credential blob |
| `update_credential` | `opcua_statuscode_t (*)(void*, const mu_string_t*, const mu_bytestring_t*, const mu_string_t*)` | Replace an existing credential blob |
| `delete_credential` | `opcua_statuscode_t (*)(void*, const mu_string_t*, const mu_string_t*)` | Remove a credential |
| `context` | `void *` | Integrator context pointer |

### KeyCredentialConfigurationType (NodeId 17533)

ObjectType with InstanceDeclarations (OPC-10000-12 §8.5.4):
- ResourceUri (17512) — String Property
- ProfileUri (17513) — String Property
- EndpointUrls (17514) — String[] Property
- ServiceStatus (17515) — StatusCode Property
- GetEncryptingKey Method (17516)

### KeyCredentialConfigurationFolderType (NodeId 17496)

ObjectType with InstanceDeclarations (OPC-10000-12 §8.6.2):
- ServiceName_Placeholder (17511) — KeyCredentialConfigurationType instance
- CreateCredential Method (17522)
- UpdateCredential Method (17519)
- DeleteCredential Method (17521)

## Kconfig

| Symbol | CU | Default | Depends |
|--------|-----|---------|---------|
| `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE` | 2113 | `y` (full) | `MUC_OPCUA_CU_METHOD_SERVER` |

## File Map

| File | Purpose |
|------|---------|
| `include/muc_opcua/services/key_credential.h` | Public adapter type + method NodeId constants |
| `src/cu/core_2022_server/key_credential/key_credential.c` | Method handlers (dispatch from Method Server callbacks) |
| `src/address_space/base_nodes.c` | Type-system InstanceDeclarations |
| `Kconfig` | `MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE` |
| `include/muc_opcua/server.h` | Add `mu_key_credential_adapter_t*` to config |
| `tests/unit/test_key_credential.c` | Unit tests |
