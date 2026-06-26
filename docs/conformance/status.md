# Conformance Status

**Target Profile:** Nano Embedded Device 2017 Server Profile
(`http://opcfoundation.org/UA-Profile/Server/NanoEmbeddedDevice2017`), which is
functionally equivalent to the **Core 2017 Server Facet** over UA-TCP UA-SC
UA-Binary with SecurityPolicy None and Anonymous identity.

**Status:** `profile-targeting`. This server has **not** been verified with the OPC
Foundation Compliance Test Tool (CTT); a compliance claim is withheld
until a CTT run passes (project policy, plan.md).

Conformance-unit status (Core 2017 Server Facet groups; see [services.md](services.md)):

| Conformance group / unit | Status | Evidence |
|---|---|---|
| Transport — UA-TCP UA-SC UA-Binary | Implemented | `test_server_handshake*`, `interop`/`dotnet-interop` CI |
| Security — SecurityPolicy None | Implemented | interop with asyncua + .NET reference client |
| Security — Basic256Sha256 (beyond Nano) | Implemented | `dotnet-interop` SignAndEncrypt (#162-#166) |
| Security — User Token Anonymous | Implemented | ActivateSession; non-Anonymous -> `Bad_IdentityTokenInvalid` |
| Discovery — FindServers / GetEndpoints | Implemented | `test_discovery_*` |
| Session — Base / General Behaviour / Minimum 1 | Implemented | `test_session`, `test_server_handshake*` |
| Attribute — Read | Implemented | `test_read_service`, Base Info reads |
| View — Browse (View Basic) | Implemented | `test_browse_*`, handshake tests |
| View — BrowseNext | Implemented | `test_view_services` |
| View — TranslateBrowsePaths | Implemented | `test_view_services` |
| View — RegisterNodes / UnregisterNodes | Implemented | `test_view_services` |
| Address Space Base — standard nodes | Implemented (static) | `base_nodes.c`; `test_view_services` |
| Base Info — Server / ServerCapabilities objects | Implemented (static) | `base_nodes.c` |
| Base Info — ServerStatus.CurrentTime/StartTime (live) | Implemented | `base_nodes.c` runtime value sources; `test_view_services` |

All in-scope Nano conformance units are implemented and covered by tests/CI.

## Remaining for Nano
1. **CTT verification** — run the OPC Foundation Compliance Test Tool against the
   server and record results; only then may a profile-compliance claim be made.
   (External step; the implementation surface is complete.)

See `specs/002-nano-profile-completion/` for the task breakdown.
