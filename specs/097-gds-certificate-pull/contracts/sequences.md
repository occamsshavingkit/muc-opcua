# Method Call Sequences: GDS Certificate Pull Management

## Pull Workflow (OPC-10000-12 §7.6)

```
Client              CertificateManager         Adapter
  |                        |                       |
  |--- Browse ------------>|                       |
  |<-- type hierarchy -----|                       |
  |                        |                       |
  |--- StartSigningRequest->|                       |
  |                        |--- start_signing ---->|
  |                        |<-- requestId ---------|
  |<-- Good + requestId ---|                       |
  |                        |                       |
  |   [time passes: adapter completes signing]     |
  |                        |                       |
  |--- FinishRequest ----->|                       |
  |                        |--- finish_request --->|
  |                        |<-- cert+key+issuers --|
  |<-- Good + cert --------|                       |
```

## Error Paths

```
Client              CertificateManager
  |                        |
  |--- StartSigningRequest (empty CSR) ->|
  |<-- Bad_InvalidArgument -|
  |
  |--- FinishRequest (unknown requestId) ->|
  |<-- Bad_NotFound ---------|
  |
  |--- GetRejectedList ----->|
  |                        | (adapter returns empty list)
  |<-- Good + empty list ---|
```

## Compile-Out Path

```
Build with MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL=n
→ CertificateManager symbols absent
→ nano build passes without warning
→ test_certificate_manager tests skipped
```
