# Conformance: Micro Embedded Device 2017 Server Profile

This server targets the **Micro Embedded Device 2017 Server Profile**
(`http://opcfoundation.org/UA-Profile/Server/MicroEmbeddedDevice2017`, OPC 10000-7,
OPC Foundation profile id 1659).
>
> **Strict profile (spec 067).** The `micro` build now equals exactly Nano + the
> EmbeddedDataChangeSubscription facet + Session-Minimum-2-Parallel (`MULTIPLE_CONNECTIONS`).
> The previously-bundled Attribute Write, Multi-chunk, and Extended NodeIds are **not**
> mandated by MicroEmbeddedDevice2017 and are now `-D` opt-ins.

Per the profile definition, Micro *builds upon the
Nano Embedded Device Server Profile; the most important additions are support for
subscriptions via the Embedded Data Change Subscription Server Facet and support for at
least two sessions.*

## The profile ladder (for orientation)
OPC 10000-7 §4.2 defines conformance units and §4.3 defines profiles. The
shorthand below is orientation for the current profile target, not a
claim that external profile validation has been completed.

- **Nano** = Core Server Facet + UA-TCP UA-SC UA-Binary + SecurityPolicy None + Anonymous.
- **Micro** = Nano + subscriptions (Embedded Data Change Subscription facet) + ≥2 sessions.
- **Embedded** = Micro + security policies + the Standard DataChange Subscription facet
  + full type-system exposure.

muc-opcua currently targets the Nano surface documented in
[profile-nano.md](profile-nano.md) plus, ahead of schedule, Basic256Sha256
(an Embedded-tier feature).

## Micro-targeted surface (the Embedded Data Change Subscription Server Facet)
A no-heap subscription engine (`src/services/subscription.{c,h}`, compiled behind the
`MUC_OPCUA_SUBSCRIPTIONS` build option, ON for `make micro`). All state is fixed-size
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

## Out of scope for Micro (Embedded tier and above)

The following are not required by the Micro profile and remain disabled unless the
Embedded profile gates are enabled:

- Standard DataChange Subscription 2017 facet deltas: absolute-deadband filtering
  (OPC-10000-4 §7.22.2), queue size 2 with overflow handling (§5.13.2, §7.20.1),
  SetTriggering (§5.13.5), and raised capacities.
- Method Call for `Server.GetMonitoredItems` and `Server.ResendData`
  (OPC-10000-4 §5.12.2.2; OPC-10000-5 §9.1, §9.2).
- Base Info Type System exposure.
- Base Information nodes and the `ServerProfileArray` (`MUC_OPCUA_BASE_NODES` is OFF
  for Micro, like Nano): the `MicroEmbeddedDevice2017` URI names the profile *target*
  and is **not** emitted anywhere as a runtime `ServerProfileArray` value; the
  integrator supplies the address space (`test_profile_surface`). OPC-10000-7 §4.3
  (ServerProfileArray / profile URIs), OPC-10000-5.
- Security policies beyond None. Basic256Sha256 is available in the embedded build.

TransferSubscriptions (§5.14.7) belongs to the Client Redundancy Facet and is not part
of the selected Micro or Embedded work. Percent deadband belongs to the Data Access
Server Facet (`MUC_OPCUA_DATA_ACCESS`, spec 060 — off in the Micro profile, so percent
deadband returns `Bad_MonitoredItemFilterUnsupported` here); aggregate filters
(Average/Min/Max) are supported.

**Concurrent ≥2 sessions** — implemented: the server multiplexes up to
`MU_MAX_SESSIONS` (default 2) logical sessions over a single TCP connection
(`test_session`, `test_single_client_limit`).

## Remaining evidence for Micro profile status (see [status.md](status.md))
- **CTT verification** — not yet run; `profile-targeting` until it passes. No
  profile conformance status is claimed without that evidence.
