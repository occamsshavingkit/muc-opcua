<!-- markdownlint-disable MD013 -->

# Service Sequences

## CU Claim Promotion

```mermaid
sequenceDiagram
    participant Auditor
    participant Manifest
    participant ServiceTests
    participant Generator
    participant ProfileBuild

    Auditor->>Manifest: Read in-scope CU record
    Auditor->>ServiceTests: Locate existing behaviour proof
    alt proof complete
        Auditor->>Manifest: Add named CU symbol and backing tests
    else proof incomplete
        Auditor->>ServiceTests: Add failing behaviour test first
        ServiceTests-->>Auditor: Failure proves missing behaviour
        Auditor->>Manifest: Keep claim unimplemented until behaviour passes
    end
    Auditor->>Generator: Regenerate Kconfig/docs/claim maps
    Generator->>ProfileBuild: Validate profile defaults and compile-out state
```

## Service Request Behaviour

```mermaid
sequenceDiagram
    participant Client
    participant Dispatcher
    participant ServiceHandler
    participant ServerState
    participant Response

    Client->>Dispatcher: Encoded request
    Dispatcher->>ServiceHandler: Route by service id if CU enabled
    alt CU disabled or unsupported
        Dispatcher->>Response: Explicit OPC UA failure StatusCode
    else request malformed
        ServiceHandler->>Response: Cited decode or service failure StatusCode
    else request valid
        ServiceHandler->>ServerState: Read or update bounded state
        ServiceHandler->>Response: Service response and operation results
    end
```

## Diagnostics Response Behaviour

```mermaid
sequenceDiagram
    participant Client
    participant ServiceHandler
    participant Diagnostics
    participant ResponseHeader
    participant AddressSpace

    Client->>ServiceHandler: Request with returnDiagnostics mask
    ServiceHandler->>Diagnostics: Record service/request/session outcome
    alt Base Services Diagnostics enabled
        Diagnostics->>ResponseHeader: Available DiagnosticInfo or explicit empty result
    else diagnostics disabled
        Diagnostics->>ResponseHeader: No diagnostics claim; no fabricated data
    end
    Client->>AddressSpace: Read ServerDiagnostics nodes
    AddressSpace->>Diagnostics: Read live counters
```
