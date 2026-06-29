# Historical Access Research

**Feature**: Historical Access (017-historical-access)
**Scope**: OPC UA Part 11 (Historical Access)

## Core Concepts

1. **HistoryRead Service**:
   - `HistoryReadRequest` takes a `HistoryReadDetails` structure.
   - For this feature, we implement `ReadRawModifiedDetails` (isReadModified flag differentiates Raw vs Modified).
   - `HistoryReadResult` contains a `HistoryData` or `HistoryModifiedData` ExtensionObject in the `historyData` field.
   - Uses `continuationPoint` if data is too large to fit in a single chunk.

2. **HistoryUpdate Service**:
   - `HistoryUpdateRequest` takes a `HistoryUpdateDetails`.
   - For this feature, we implement `UpdateDataDetails` for Insert, Replace, and Delete.
   - `PerformInsertReplace`, `PerformUpdate`, `PerformDelete`.

3. **Storage/Adapter Pattern**:
   - Since Micro OPC UA operates in a zero-heap environment, we cannot accumulate unbounded data in RAM.
   - We need an adapter interface: `mu_history_adapter_t` that allows the user application to back the historical store with SD cards, flash, etc.
   - The server will pass read/write queries to the adapter.
   - Continuation points are just byte arrays; we will ask the adapter to yield the next chunk of data given a continuation point.

## Memory Implications

- **Read**: The adapter will populate a provided static buffer (part of `mu_server_storage`) up to the `max_message_size`. If the data exceeds the buffer, the adapter sets a continuation point.
- **Update**: Data points are passed to the adapter one by one. No dynamic allocation required.
- **Adapter**: `mu_history_adapter_t` struct will hold function pointers and a context pointer. Overhead is ~32 bytes per server.

## OPC UA Normative Mapping

- Part 4, 5.10.3 (HistoryRead)
- Part 4, 5.10.4 (HistoryUpdate)
- Part 11 (Historical Access) - specifics on semantics of UpdateDataDetails.
