# Contract: Conformance Citation and Claim Hygiene

## Consumer

A maintainer or implementation agent using documentation rows and Spec Kit tasks as authoritative references for OPC UA behavior.

## Required Citation Outcomes

- Method Call rows cite OPC-10000-4 §5.12.2.
- NodeManagement rows cite OPC-10000-4 §5.8.
- AggregateFilter tasks cite OPC-10000-4 §7.22.4 plus the scoped OPC-10000-13 Average, Minimum, and Maximum references.
- MonitoredItem tasks cite OPC-10000-4 §5.13 subsections that match the service under test.
- Subscription tasks cite OPC-10000-4 §5.14 subsections that match the service under test.
- Profile/conformance tasks cite OPC-10000-7 §4.2 and §4.3 when discussing claim status.

## Required Claim Outcomes

- Current profile wording remains profile-targeting.
- Profile-compliant and CTT-verified claims remain explicitly withheld unless external CTT evidence is included on the same row or nearby evidence block.
- Unsupported services and features name expected `Bad_*` StatusCodes where behavior is wire-visible.

## Verification

- `ctest --test-dir build/baseline --output-on-failure -R test_conformance_docs`
- Manual review of generated `tasks.md` to confirm every behavior-changing task has an OPC UA citation.
