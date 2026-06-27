# Conformance: Micro Embedded Device 2017 Server Profile

This server targets the **Micro Embedded Device 2017 Server Profile**
(`http://opcfoundation.org/UA-Profile/Server/MicroEmbeddedDevice2017`, OPC 10000-7,
OPC Foundation profile id 1659). Per the profile definition, Micro *builds upon the
Nano Embedded Device Server Profile; the most important additions are support for
subscriptions via the Embedded Data Change Subscription Server Facet and support for at
least two sessions.*

## The profile ladder (for orientation)
- **Nano** = Core Server Facet + UA-TCP UA-SC UA-Binary + SecurityPolicy None + Anonymous.
- **Micro** = Nano + subscriptions (Embedded Data Change Subscription facet) + ≥2 sessions.
- **Embedded** = Micro + security policies + the Standard DataChange Subscription facet
  + full type-system exposure.

micro-opcua has the complete Nano surface (see [profile-nano.md](profile-nano.md)) plus,
ahead of schedule, Basic256Sha256 (an Embedded-tier feature).

## Implemented surface (the Embedded Data Change Subscription Server Facet)
A no-heap subscription engine (`src/services/subscription.{c,h}`, compiled behind the
`MICRO_OPCUA_SUBSCRIPTIONS` build option, ON for `make micro`). All state is fixed-size
and lives in the caller-owned server struct; sampling and Publish delivery are driven
cooperatively by `mu_server_poll`.

- **Subscription Service Set (OPC 10000-4 §5.14):** CreateSubscription (§5.14.2),
  ModifySubscription (§5.14.3), SetPublishingMode (§5.14.4), Publish (§5.14.5),
  Republish (§5.14.6), DeleteSubscriptions (§5.14.8).
- **MonitoredItem Service Set (§5.13):** CreateMonitoredItems (§5.13.2),
  ModifyMonitoredItems (§5.13.3), SetMonitoringMode (§5.13.4), DeleteMonitoredItems
  (§5.13.6) — data-change monitoring only.
- **Data-change monitoring:** per-item sampling against the address space with a
  DataChangeFilter trigger (Status / StatusValue, §7.17.2); the initial value is
  reported; changes are queued as DataChangeNotifications.
- **Asynchronous Publish:** PublishRequests are parked in a fixed-size queue and
  answered by the publishing timer with a DataChangeNotification or, after the
  keep-alive count, an empty keep-alive NotificationMessage (§5.14.1). The retained
  message supports Republish; SubscriptionAcknowledgements purge it.

Fixed capacities (integrator-overridable with `-D`): `MU_MAX_SUBSCRIPTIONS` (2),
`MU_MAX_MONITORED_ITEMS` (8), `MU_MAX_PUBLISH_REQUESTS` (4). The engine is
floating-point-free (Durations are converted to integer milliseconds at the dispatch
boundary; no FPU is required on the target).

## Out of scope (Embedded tier and above)
TransferSubscriptions (§5.14.7), SetTriggering (§5.13.5), event subscriptions, and
aggregate/percent-deadband filters belong to the Standard/Enhanced DataChange facets.
Security policies on the Micro tier are not required (None is the baseline), though
Basic256Sha256 is available.

**Concurrent ≥2 sessions** — implemented: the server multiplexes up to
`MU_MAX_SESSIONS` (default 2) logical sessions over a single TCP connection
(`test_session`, `test_single_client_limit`).

## Remaining to full Micro (see [status.md](status.md))
- **CTT verification** — not yet run; `profile-targeting` until it passes. No
  profile-compliance claim is made without that evidence.
