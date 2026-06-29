# Feature Specification: Query Services

**Feature Branch**: `015-query-services`  
**Created**: 2026-06-29
**Status**: Draft  
**Input**: User description: "Implement QueryFirst and QueryNext to allow clients to search the address space using complex filters."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Client Queries Address Space with QueryFirst (Priority: P1)

Authorized clients need to be able to issue a `QueryFirstRequest` to search the static and dynamic address space using a `ContentFilter` and `NodeTypeDescription`. The server processes the request and returns a `QueryFirstResponse` with the initial batch of matching nodes and potentially a `ContinuationPoint`.

**Why this priority**: QueryFirst is the entry point for the Query service set, enabling clients to discover nodes dynamically based on types and attributes without recursively browsing the entire address space.

**Independent Test**: Can be fully tested by sending a valid `QueryFirstRequest` and verifying that the returned nodes match the filter criteria.

**Acceptance Scenarios**:

1. **Given** an active session and an address space containing nodes of a specific type, **When** the client sends a `QueryFirstRequest` filtering by that type, **Then** the server responds with a `QueryFirstResponse` containing the matching nodes.
2. **Given** a query that exceeds the `MaxQueryResults` limit, **When** the server processes it, **Then** the server returns the first batch of results along with a `ContinuationPoint`.
3. **Given** an invalid or unsupported `ContentFilter` in the request, **When** the server processes it, **Then** the server returns an appropriate error `StatusCode` (e.g., `BadFilterNotAllowed`).

---

### User Story 2 - Client Retrieves Remaining Results with QueryNext (Priority: P2)

When a `QueryFirst` request yields more results than can fit in a single response, the client must be able to retrieve the subsequent batches using `QueryNextRequest` with the provided `ContinuationPoint`.

**Why this priority**: QueryNext is essential for handling large address spaces or broad queries that return many results, ensuring the server doesn't violate message size limits.

**Independent Test**: Can be tested by creating a scenario where `QueryFirst` returns a continuation point, and then issuing a `QueryNextRequest` with that point to get the rest of the results.

**Acceptance Scenarios**:

1. **Given** a valid `ContinuationPoint` from a previous `QueryFirstResponse`, **When** the client sends a `QueryNextRequest` with it, **Then** the server returns the next batch of results.
2. **Given** a `QueryNextRequest` with `releaseContinuationPoint` set to true, **When** the server processes it, **Then** the server frees the resources associated with the continuation point and returns an empty response with `Good` status.
3. **Given** an invalid or expired `ContinuationPoint`, **When** the server processes it, **Then** the server returns `BadContinuationPointInvalid`.

## Out of Scope *(mandatory)*

- Advanced or highly complex `ContentFilter` operators (e.g., spatial queries, complex arithmetic) that are beyond the core Embedded Profile requirements.
- Full `QueryFirst` support across remote servers (we only query the local address space).

## Open Questions

- What subset of `ContentFilterElement` operators (e.g., `Equals`, `Like`, `GreaterThan`, `InList`) must we support for the Embedded Profile query compliance?
- What is the maximum number of active continuation points the server should track at once? (Memory constraint).
