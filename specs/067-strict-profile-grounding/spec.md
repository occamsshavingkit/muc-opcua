# Spec 067: Strict Profile Grounding

Out-of-band from the Project-B facet series. Redefines the four named build profiles
(`nano`/`micro`/`embedded`/`standard`) to equal **exactly** the mandatory facet set of their
namesake OPC UA Server Profile — no more, no less. `full` is unchanged ("everything on").
Optional facets remain integrator opt-ins via `-D` (the build already supports this).

## Principle (the bug being fixed)

An OPC UA Server Profile is a **strict composition of mandatory facets** (every included
facet/CU `isOptional=false`). It does **not** say "Embedded, plus some nice-to-haves." Our
named profiles were hand-curated with editorial extras — and, in one case, gaps — so they no
longer match what they advertise. Most visibly, `standard` and `full` became **feature-
identical** (39 flags each; only the capacity-tier marker differs), so `full` added nothing
over `standard`. The fix: each named profile = its OPC profile's mandatory facets; extras are
opt-in.

## Grounded mandatory facet tree (OPC profile DB, [[opc-profile-reporting-api]])

```
StandardUA2017 (1663)
├── EmbeddedUA2017 (1661)  [+ Base Info Type System, Security Policy Required, Default AppInstance Cert]
│   ├── MicroEmbeddedDevice2017 (1659)  [+ Session Minimum 2 Parallel]
│   │   ├── EmbeddedDataChangeSubscription (141)  [basic subscriptions: Monitor Basic, Items 2, QueueSize 1]
│   │   └── NanoEmbeddedDevice2017 (1657)
│   │       ├── Core2017Facet (1673)  [+ UserNamePassword (1096)]
│   │       └── uatcp-uasc-uabinary (219)
│   └── StandardDataChangeSubscription2017 (1675)  [GetMonitoredItems/ResendData, Items 10/100, MinQueueSize_02, Triggering, Deadband, Republish]
├── EnhancedDataChangeSubscription2017 (1678)  [Items 500, MinQueueSize_05, Sub Min 05, Publish Min 10 — capacity]
└── X509Certificate (1097)
```

Core2017 (1673) mandatory CUs: Address Space Base/Atomicity/Full-Array-Only, Attribute Read,
Base Info Core Structure, Discovery Find-Servers-Self + Get-Endpoints, SecurityPolicy
Support, Session Base/Minimum-1/General-Service-Behaviour, View Basic, View Minimum
Continuation Point 01, **View RegisterNodes**, **View TranslateBrowsePath**. Plus
UserNamePassword (Security User Name Password / Invalid user token).

## CU → build-flag map (grounded facts, verified in code)

| Mandated capability | Flag | Notes |
| --- | --- | --- |
| Attribute Read | `SERVICE_READ` | default ON |
| View Basic + TranslateBrowsePath + BrowseNext | `SERVICE_BROWSE` | one flag (default ON) covers Browse+BrowseNext+TranslateBrowsePaths |
| View RegisterNodes | `SERVICE_REGISTER_NODES` | default OFF |
| Discovery FindServers/GetEndpoints | `SERVICE_DISCOVERY` | default ON |
| Address Space Base / Base Info Core Structure (Server object) | `BASE_NODES` | default OFF — library-provided Server node set |
| UserName/Password + X509 user tokens | `USER_AUTH` | default OFF |
| Security Policy Required + AppInstance Cert (Basic256Sha256) | `SECURITY` | default OFF |
| Base Info Type System | `BASE_TYPE_SYSTEM` | |
| Session Minimum 2 Parallel | `MULTIPLE_CONNECTIONS` | **grounded: mandated at Micro** |
| Basic + Standard + Enhanced DataChange | `SUBSCRIPTIONS`, `SUBSCRIPTIONS_STANDARD`, capacity tier | |
| GetMonitoredItems / ResendData (built-in) | *(no flag)* | **`dispatch_method.c` is gated on `SUBSCRIPTIONS && SUBSCRIPTIONS_STANDARD && BASE_TYPE_SYSTEM`, NOT `METHOD_SERVER`** — built-ins serve without the Method Server facet |

**Key grounded correction:** `METHOD_SERVER` is the *optional* arbitrary-method (Method
Server) facet — it is **not** required for the mandated built-in methods. Likewise
`DATA_ACCESS`, `EVENTS`/`EVENT_FILTER_WHERE`, `SERVICE_WRITE`, `MULTI_CHUNK`,
`EXTENDED_NODEIDS`, `COMPLEX_TYPES`, `TIME_SYNC`, `SERVER_DIAGNOSTICS`, `REVERSE_CONNECT`,
`AGGREGATE_FULL`, `SERVICE_HISTORY`, `SERVICE_QUERY`, `SERVICE_NODEMANAGEMENT`,
`DYNAMIC_NODES`, `PUBSUB`, `REDUNDANCY`, `ECC`, `CUSTOM_METHODS`, `AUDITING` are all
**optional** (mandated by no profile in this tree).

## Target feature set per profile (strict)

| Flag | nano | micro | embedded | standard | full |
| --- | --- | --- | --- | --- | --- |
| SERVICE_READ / SERVICE_BROWSE / SERVICE_DISCOVERY | ✅ | ✅ | ✅ | ✅ | ✅ |
| SERVICE_REGISTER_NODES | ✅ | ✅ | ✅ | ✅ | ✅ |
| USER_AUTH | ✅ | ✅ | ✅ | ✅ | ✅ |
| BASE_NODES | ✅? | ✅? | ✅ | ✅ | ✅ |
| MULTIPLE_CONNECTIONS | — | ✅ | ✅ | ✅ | ✅ |
| SUBSCRIPTIONS | — | ✅ | ✅ | ✅ | ✅ |
| SUBSCRIPTIONS_STANDARD | — | — | ✅ | ✅ | ✅ |
| SECURITY / BASE_TYPE_SYSTEM | — | — | ✅ | ✅ | ✅ |
| STANDARD_PROFILE (Enhanced capacity + X509) | — | — | — | ✅ | ✅ |
| NAMESPACES | — | — | — | ✅ | ✅ |
| **everything else** (WRITE, MULTI_CHUNK, EXTENDED_NODEIDS, EVENTS, EVENT_FILTER_WHERE, METHOD_SERVER, CUSTOM_METHODS, DATA_ACCESS, COMPLEX_TYPES, TIME_SYNC, AGGREGATE_FULL, SERVER_DIAGNOSTICS, REVERSE_CONNECT, REDUNDANCY, SERVICE_HISTORY, SERVICE_QUERY, SERVICE_NODEMANAGEMENT, DYNAMIC_NODES, PUBSUB, ECC, AUDITING) | — | — | — | — | ✅ |

### Determinations to confirm (the genuinely ambiguous calls)

1. **`nano`/`micro` `BASE_NODES` + `USER_AUTH`** — Core mandates the Server object (Base Info
   Core Structure) and UserName/Password. Today `BASE_NODES`/`USER_AUTH` are OFF for
   nano/micro (integrator-provided address space, anonymous). Options: (a) leave nano/micro
   integrator-provided/anonymous (library stance — the integrator supplies nodes/tokens), or
   (b) enable BASE_NODES/USER_AUTH to be self-contained-conformant. **Recommend (a) for nano
   minimality; add BASE_NODES at micro+.** Needs a call.
2. **Method properties without `METHOD_SERVER`** — B3 gated the built-in methods'
   `InputArguments`/`OutputArguments` Property values and the `Executable` attribute on
   `METHOD_SERVER`. Strict profiles drop `METHOD_SERVER`, so verify whether a Method node's
   InputArguments/OutputArguments (arguably expected for GetMonitoredItems/ResendData) must
   be un-gated from `METHOD_SERVER` to remain conformant, or whether their absence is
   acceptable (they are optional Properties).

## Requirements

**FR-1 — Restructure the CMake profile blocks** to the strict target above; `full` unchanged.
Remove the now-false `features.h` guards `STANDARD_PROFILE requires DATA_ACCESS` and
`STANDARD_PROFILE requires METHOD_SERVER` (neither is mandated).

**FR-2 — Fill grounded gaps** per the confirmed determinations (RegisterNodes/USER_AUTH where
mandated; BASE_NODES tier; method-property un-gating if required).

**FR-3 — Re-gate tests + claim map + CI.** Tests that assumed stripped features run as
no-ops under the leaner profiles (they still cover `full`); update `claim_test_map.md` profile
tokens so optional-facet claims map to `full` (and standard where still mandated). Update the
CI profile matrix if the set changes.

**FR-4 — Docs + sizes.** Re-measure every profile's `.text`/RAM (standard shrinks materially;
full flat); update README profile+size tables, profile conformance docs, size ledger. Each
profile's advertised `ServerProfileArray` must match its mandatory facet set.

## Verification

- Per profile, the compiled flag set equals the strict target (a test or documented check).
- All-profile matrix + sanitizers green; `full` byte-identical to pre-spec (its features are
  unchanged); nano/micro/embedded/standard shrink to their grounded sets.
- `standard` is now a strict subset of `full` (the standard==full defect is gone).

## On approval

Confirm the two determinations above, then speckit plan/tasks, execute, PR against `main`
with a merge commit. Resume B7 (client-redundancy) afterward on the corrected profile base.
