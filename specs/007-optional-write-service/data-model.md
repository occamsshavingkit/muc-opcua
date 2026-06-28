# Phase 1 Data Model: Write Service Structures

This document describes the data structures, binary encoding layouts, and validation rules for the OPC UA Write service.

---

## 1. Data Structures

### A. WriteValue (OPC-10000-4 §5.11.4.2)
Represents a single target node, attribute, index range, and value to be written.

```c
typedef struct {
    opcua_nodeid_t nodeId;
    opcua_int32_t attributeId;
    opcua_string_t indexRange;
    opcua_datavalue_t value;
} mu_write_value_t;
```

*   **nodeId**: Resolved node identifier.
*   **attributeId**: The attribute ID to write. MUST be `13` (`AttributeId_Value`).
*   **indexRange**: Optional array range. Must be empty (length <= 0) in this profile.
*   **value**: Contains the `Variant` value, status, and timestamps. Only the variant value is processed.

---

## 2. Binary Encoding Layout (OPC-10000-6)

### A. WriteRequest
Encodes the request sequence on the wire.

| Field | Type | Description |
|---|---|---|
| `RequestHeader` | Structure | Common request header |
| `nodesToWriteSize` | Int32 | Number of elements in `nodesToWrite` array |
| `nodesToWrite` | WriteValue[] | Array of WriteValues to write |

### B. WriteResponse
Encodes the response sequence on the wire.

| Field | Type | Description |
|---|---|---|
| `ResponseHeader` | Structure | Common response header |
| `resultsSize` | Int32 | Number of elements in `results` array |
| `results` | StatusCode[] | Array of StatusCodes for each write item |
| `diagnosticInfosSize` | Int32 | Must be 0 |

---

## 3. Validation and State Rules

1.  **Service Gating**: If the Write request is received but the service is disabled:
    *   The dispatch layer immediately returns `Bad_ServiceUnsupported`.
2.  **Empty Array**: If `nodesToWriteSize` is <= 0:
    *   The service returns `Bad_NothingToDo`.
3.  **Invalid AttributeId**: If any `attributeId` != `13`:
    *   The result for that item is set to `Bad_AttributeIdInvalid`.
4.  **IndexRange Present**: If `indexRange.length` > 0:
    *   The result for that item is set to `Bad_WriteNotSupported`.
5.  **Node Resolving**: If the `nodeId` is not found in the address space:
    *   The result for that item is set to `Bad_NodeIdUnknown`.
6.  **Type Validation**: If the node exists and has a defined type:
    *   If `value.value.type` != target node `datatype` (or cannot be mapped):
        *   The result for that item is set to `Bad_TypeMismatch`.
7.  **Read-Only check**: If the node is not writable:
    *   The result for that item is set to `Bad_NotWritable`.
