# Research: Standard UA Client 2025 Profile

## Decision 1: Client subscription state architecture

**Decision**: Static arrays embedded in the client struct, same pattern as the
server-side subscription engine. Maximum subscription count set via
`MU_CLIENT_MAX_SUBSCRIPTIONS` and `MU_CLIENT_MAX_MONITORED_ITEMS`.

**Rationale**: Matches the existing server-side subscription pattern. No heap
allocation. Bounded state makes worst-case memory predictable.

**Alternatives considered**:
- **Dynamic linked list**: Flexible but introduces heap allocation.
- **Single subscription only**: Simpler but insufficient for Standard Client profile.

## Decision 2: Publish request parking model

**Decision**: The client parks a single Publish request at a time, re-posting
after each response, following the existing server-side Publish request model.

**Rationale**: The server-side already handles Publish requests this way.
The client must maintain at least one pending Publish request to receive
notifications.

**Alternatives considered**:
- **Multiple parked Publish requests**: Adds complexity without benefit for
  a single-session client.

## Decision 3: Notification queue location

**Decision**: Notifications are buffered in a per-monitored-item ring buffer
inside the client struct, matching the server-side `monitored_item.queue[]`
pattern.

**Rationale**: Decouples notification receipt from application processing.
The application drains the queue on its own schedule.

**Alternatives considered**:
- **Callback-based dispatch**: Application must process synchronously.
  Inappropriate for embedded event loops.

## Decision 4: EventFilter support

**Decision**: Client parses `EventNotificationList` and extracts field values
by position based on the `SelectClause` used when creating the monitored item.
The `SelectClause` is stored in the monitored item state on the client side.

**Rationale**: The server does not echo the `SelectClause` in notifications.
The client must remember which fields it requested to interpret the positional
field values in the notification.

**Alternatives considered**:
- **Opaque blob delivery**: Returns raw `ExtensionObject` to application.
  Pushes complexity to the caller.

## Decision 5: Profile gating via `MUC_OPCUA_CLIENT_PROFILE`

**Decision**: Reuse the existing `MUC_OPCUA_CLIENT_PROFILE` CMake option with
values `nano` (existing) and `standard` (new). The standard value enables
subscriptions, write, call, and event features.

**Rationale**: Follows the project's existing profile gating pattern
(`MUC_OPCUA_PROFILE` for the server). Simple, consistent, testable.

**Alternatives considered**:
- **Separate feature flags per capability**: More granular but adds
  configuration complexity. Not needed until a mid-range profile
  between nano and standard is defined.
