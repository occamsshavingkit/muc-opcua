# Requirements Quality Checklist: mDNS Discovery

**Purpose**: Validate specification completeness, clarity, and consistency for F1 mDNS discovery
**Created**: 2026-07-07
**Feature**: [spec.md](../spec.md)

## Requirement Completeness

- [x] CHK001 — Are mDNS adapter lifecycle requirements defined: when is publish called (init), when is unpublish called (close), and what happens on re-init? [Completeness, Spec §FR-003, FR-004]
- [x] CHK002 — Are requirements defined for port 4840 being the standard OPC UA port when no port is specified in endpoint_url? [Gap, Completeness]
- [x] CHK003 — Are requirements defined for IPv6 endpoint URLs (e.g., `opc.tcp://[::1]:4840`)? [Gap, Coverage]
- [x] CHK004 — Are requirements defined for the `mu_mdns_adapter_noop` adapter — is it a static global, a function return, or a macro? [Completeness, Spec §FR-005]

## Requirement Clarity

- [x] CHK005 — Is the endpoint_url parsing behavior clearly specified — what constitutes a valid hostname portion and what error behavior occurs on malformed URLs? [Clarity, Spec §FR-003]
- [x] CHK006 — Is the publish callback signature clear about whether `hostname` receives the raw extracted string or a resolved IP address? [Clarity, Spec §FR-001]
- [x] CHK007 — Is the TXT record content format explicitly specified — key=value pairs, encoding, maximum length? [Clarity, Spec §OPC-002]

## Requirement Consistency

- [x] CHK008 — Do the adapter callback signatures in FR-001 align with the call site parameters in FR-003 (both specify hostname, port, app_uri, path)? [Consistency, Spec §FR-001, FR-003]
- [x] CHK009 — Is the pointer-vs-inline storage decision for `mdns_adapter` consistent with existing patterns (`persistence_adapter`, `crypto_adapter`)? [Consistency, Spec §FR-002]

## Acceptance Criteria Quality

- [x] CHK010 — Is SC-001 measurable — "visible to avahi-browse / dns-sd" requires both tools to be installed; is there a programmatic verification path for CI? [Measurability, Spec §SC-001]
- [x] CHK011 — Is SC-004 measurable — "zero regressions" requires a defined baseline test count and profile; is the baseline documented? [Measurability, Spec §SC-004]

## Scenario Coverage

- [x] CHK012 — Are requirements defined for the case where mDNS publish fails (e.g., port in use, network unavailable) — does the server continue or abort? [Coverage, Exception Flow]
- [x] CHK013 — Are requirements defined for periodic re-announcement of the DNS-SD record, or is a single publish at init sufficient? [Coverage, Gap]
- [x] CHK014 — Are requirements defined for multiple network interfaces — does the adapter bind to all interfaces or just one? [Coverage, Gap]

## Edge Case Coverage

- [x] CHK015 — Is the behavior specified when `endpoint_url` is NULL or an empty string? [Edge Case, Gap]
- [x] CHK016 — Is the behavior specified when `application_uri` is NULL or empty — is the TXT record omits the field or fails? [Edge Case, Gap]
- [x] CHK017 — Are edge case requirements defined for endpoint URLs with no hostname component (e.g., `opc.tcp://:4840`)? [Edge Case, Gap]

## Dependencies & Assumptions

- [x] CHK018 — Is the UDP multicast dependency (224.0.0.251:5353) explicitly documented as a requirement of the host adapter, with fallback behavior if multicast is unavailable? [Dependency, Gap]
- [x] CHK019 — Are the assumptions about POSIX socket availability in the host adapter documented (Linux/macOS only, not Windows)? [Assumption, Gap]
