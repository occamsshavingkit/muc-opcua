# Phase 0 Research: Optional Attribute Write Service

This document captures the research, technology decisions, and alternatives considered for implementing the OPC UA Write service (Value attribute only) for the `embedded-write` profile.

---

## 1. Service Gating & Build Configuration

*   **Decision**: Gate the Write service behind a CMake build option: `-DMICRO_OPCUA_SERVICE_WRITE=ON` (defaults to `OFF` for nano/micro, `ON` for embedded-write).
*   **Rationale**: Preserves the extremely small footprint of the nano and micro profiles when write support is not required.
*   **Alternatives Considered**:
    *   *Always compile Write service*: Rejected because it adds unnecessary parsing, decoding, and dispatch code (~1 KB flash) to nano profiles.

---

## 2. API Design & Callback Integration

*   **Decision**: Add a function pointer and an application context handle to `mu_server_config_t`:
    ```c
    typedef opcua_statuscode_t (*mu_write_handler_t)(void *handle,
                                                     const opcua_nodeid_t *node_id,
                                                     const opcua_variant_t *value);
    
    /* Inside mu_server_config_t: */
    mu_write_handler_t write_handler;
    void *write_handler_handle;
    ```
*   **Rationale**: The library does not manage mutable user variables internally to avoid memory consumption. The application is responsible for storing, validating, and updating the state of its backing variables.
*   **Alternatives Considered**:
    *   *Direct variable binding in address space*: Rejected because it requires the library to maintain pointers to mutable state or manage heap variables, violating the zero-mutable-globals and freestanding constraints.

---

## 3. Type Checking & Safety Boundaries

*   **Decision**: The server will resolve the node in the address space. If the node exists, the server checks that the incoming variant's `type` matches the node's expected `DataType` (if defined in the address space). If they mismatch, it returns `Bad_TypeMismatch`.
*   **Rationale**: Ensures type-safety at the protocol boundary before calling the application write handler.
*   **Alternatives Considered**:
    *   *Let the application handle type validation*: Rejected because OPC UA requires returning `Bad_TypeMismatch` for mismatched types, and doing it centrally in the server core guarantees spec conformance across all application integrations.

---

## 4. IndexRange Handling

*   **Decision**: If `indexRange` is present (string length > 0), immediately return `Bad_WriteNotSupported`.
*   **Rationale**: Writing to subsets of arrays or composite structures requires complex parsing (NumericRange) and buffer shifting, which exceeds the scope of the `embedded-write` profile and inflates code size.
*   **Alternatives Considered**:
    *   *Ignore indexRange*: Rejected because writing to a whole variable when the client requested a range write violates data integrity and violates OPC UA conformance.
