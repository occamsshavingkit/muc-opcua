# Data Model: GDS Certificate Pull Management

## OPC UA Type Hierarchy (OPC-10000-12 §7.8-7.9)

```
BaseObjectType
  └── CertificateDirectoryType (i=15594)              §7.9.2
  └── CertificateGroupType (i=12555)                  §7.8.3.1
  └── CertificateType (i=12556)                       §7.8.4.1
        └── ApplicationCertificateType (i=12557)       §7.8.4.2
        │     └── RsaSha256ApplicationCertificateType (i=12559)  §7.8.4.9
        │     └── RsaMinApplicationCertificateType (i=15421)     §7.8.4.8
        └── HttpsCertificateType (i=12558)             §7.8.4.3
        └── UserCertificateType (i=15017)              §7.8.4.4
```

## Standard Instances

| Instance | NodeId | TypeDefinition | Organizer |
|----------|--------|---------------|-----------|
| CertificateGroups | i=15624 | FolderType | Objects Folder |
| DefaultApplicationGroup | i=15625 | CertificateGroupType | CertificateGroups |
| DefaultHttpsGroup | i=15626 | CertificateGroupType | CertificateGroups |
| DefaultUserTokenGroup | i=15627 | CertificateGroupType | CertificateGroups |

## Method Assignments

| Method | NodeId | On Type | Signature |
|--------|--------|---------|-----------|
| StartSigningRequest | i=12482 | CertificateType subtypes | In: CSR (ByteString), In: certificateGroupId (NodeId), Out: requestId (UInt32) |
| StartNewKeyPairRequest | i=12483 | CertificateType subtypes | In: keySpec (ByteString), In: certificateGroupId (NodeId), Out: requestId (UInt32) |
| FinishRequest | i=12484 | CertificateDirectoryType (deferred) | In: requestId (UInt32), Out: certificate (ByteString), Out: privateKey (ByteString), Out: issuerCertificates (ByteString[]) |
| GetRejectedList | i=12747 | CertificateGroupType | Out: rejectedList (CertificateGroupDataType[]) |

## Adapter Interface

```c
typedef struct {
    mu_status_code_t (*start_signing_request)(const uint8_t *csr, size_t csr_len,
                                              mu_node_id_t group_id, uint32_t *out_request_id);
    mu_status_code_t (*finish_request)(uint32_t request_id,
                                       uint8_t *out_cert, size_t *out_cert_len,
                                       uint8_t *out_key, size_t *out_key_len,
                                       uint8_t *out_issuers, size_t *out_issuers_len);
    mu_status_code_t (*get_rejected_list)(uint8_t *out_buffer, size_t *out_len);
    mu_status_code_t (*start_new_key_pair)(const uint8_t *key_spec, size_t spec_len,
                                           mu_node_id_t group_id, uint32_t *out_request_id);
} mu_certificate_manager_adapter_t;
```

## State Machine

The adapter manages certificate lifecycle; the library is a stateless dispatch layer:

```
[Client] → StartSigningRequest → adapter.start_signing_request → requestId
[Client] → FinishRequest → adapter.finish_request → certificate + key + issuers
[Client] → StartNewKeyPairRequest → adapter.start_new_key_pair → requestId
[Client] → GetRejectedList → adapter.get_rejected_list → rejected entries
```

Each Method call is independent; the requestId is the only cross-call state handle.
