# Conformance Claims

Per OPC-10000-7 §4.2/§4.3, profile and ConformanceUnit claims remain profile-targeting only until external CTT evidence is present.
The Nano Embedded Device Server Profile is the current target; no profile-compliant, CTT-verified, certified, or passed-CTT claim is made.

Guard coverage:

| Guard | Covered stale claim or number | Scanned source/evidence | Standard grounding |
|---|---|---|---|
| Claim wording rejection | `test_conformance_claims_have_evidence`, `test_profile_compliant_claims_require_external_ctt_evidence_per_opc_10000_7_4_2_and_4_3`, `test_ctt_verified_claims_require_external_ctt_evidence_per_opc_10000_7_4_2_and_4_3`, and `test_docs_and_specs_do_not_claim_profile_compliance_without_ctt_evidence` reject unsupported `profile-compliant`, `profile compliance`, `CTT-verified`, `CTT-certified`, `passed CTT`, and equivalent Compliance Test Tool wording unless the line is explicitly negative/policy wording or cites external CTT evidence. | `docs/`, `docs/conformance/`, `docs/validation/`, and `specs/`; evidence in `tests/unit/test_conformance_docs.c`. | OPC-10000-7 §4.2 ConformanceUnits and §4.3 Profiles require claim discipline until external CTT evidence exists. |
| Public support text rejection | `test_public_docs_do_not_contain_stale_profile_support_claims_per_opc_10000_7_4_2_and_4_3` rejects the stale `remaining Micro item` wording and the stale `Not implemented yet: History, NodeManagement, and aggregate subscriptions` README list. | `README.md` and `Makefile`; current support rows in `docs/conformance/services.md` and `docs/conformance/status.md`. | OPC-10000-7 §4.2/§4.3 for support/profile claim honesty; OPC-10000-4 §5.8, §5.11.3/§5.11.5, and §7.22.4 for the affected optional service/filter areas. |
| Size and benchmark number labels | `test_public_size_numbers_are_snapshot_labeled_or_reproducible` requires measured public profile-size/resource rows to have nearby `Measured snapshot`, `Measured 2026`, `reproduce with`, or `scripts/measure_size.sh all` evidence. This implements the Feature 023 size/benchmark-prose contract by guarding the currently scanned public resource rows. | `README.md`, `docs/integration-guide.md`, and `docs/size/feature-size-ledger.md`; evidence in `tests/unit/test_conformance_docs.c`. | Resource values are evidence claims, not OPC UA behavior; when tied to profile rows, the honesty rule is grounded in OPC-10000-7 §4.2/§4.3. |
| Query and NodeManagement section references | `test_conformance_docs_use_current_query_and_nodemanagement_sections` requires the services matrix to cite `QueryFirst / QueryNext` as `B.2.3 / B.2.4` and NodeManagement `AddNodes / DeleteNodes / AddReferences / DeleteReferences` as `5.8`. | `docs/conformance/services.md`; evidence in `tests/unit/test_conformance_docs.c` and Feature 023 traceability. | OPC-10000-4 Appendix B §B.2.3/§B.2.4 for Query; OPC-10000-4 §5.8 for NodeManagement. |
| Aggregate stale NodeId rejection | `test_public_docs_do_not_use_stale_aggregate_nodeids_per_opc_10000_4_7_22_4_and_opc_10000_13` rejects stale values `11565`, `11569`, and `11570` when public docs present them in Aggregate, Average, Minimum, or Maximum context. | `README.md`, `docs/api-reference.md`, `docs/integration-guide.md`, `docs/conformance/services.md`, `docs/conformance/status.md`, and `docs/traceability/012-opcua-pubsub.md`. | OPC-10000-4 §7.22.4 for `AggregateFilter`; OPC-10000-13 §4.2.2.4, §4.2.2.9, and §4.2.2.10 for Average, Minimum, and Maximum AggregateFunction objects. |
| StatusCode documentation | `test_statuscode_names_in_conformance_docs_match_opc_10000_4_7_38_2` requires documented StatusCode names to match `include/muc_opcua/status.h`; `test_statuscode_numeric_values_in_status_conformance_doc_match_implementation_per_opc_10000_4_7_38_2` requires `docs/conformance/status.md` numeric values to match implementation constants. Feature 023 required rows include `Bad_NotSupported` = `0x803D0000`, `Bad_InvalidArgument` = `0x80AB0000`, and `Bad_TooManyOperations` = `0x80100000`. | `docs/conformance/status.md`, `docs/traceability/statuscodes.md`, `docs/traceability/020-audit-hardening.md`, and `docs/validation/audit-hardening.md`; implementation source `include/muc_opcua/status.h`. | OPC-10000-4 §7.38.2 Common StatusCodes. |

Feature 023 evidence ties these rows to `docs/traceability/023-conformance-docs-subscriber.md`.
The guards are documentation/conformance checks only; they do not claim profile
compliance or CTT verification.

## Claim → test enforcement (Feature 028)

The guards above check documentation *wording*. Feature 028 adds an orthogonal,
machine-checked link from each claimed ConformanceUnit/behaviour to a test that
actually runs **in the profile that claims it** — replacing markdown substring
"traceability" with an enforced "claimed unit ⇒ profile-runnable backing test".

| Mechanism | What it enforces | Evidence |
|---|---|---|
| Per-profile CI + `make test-{nano,micro,embedded,full}` | Each profile's suite is built and run with `-DMUC_OPCUA_PROFILE=<p>`, so the per-profile `RUN_TEST` gates in `tests/unit/CMakeLists.txt` are live and each profile's claimed units run against that profile's actual library (not the "force full" default). | `.github/workflows/ci.yml` `profile-tests` matrix; `Makefile`. |
| `test_claim_map` (per-profile ctest) | For the profile a build targets, every row of `tests/conformance/claim_test_map.md` listing that profile MUST name a backing test that is ctest-registered in that build; a claimed unit with no profile-runnable test fails the build. | `tests/conformance/check_claim_map.py`; manifest `tests/conformance/claim_test_map.md`; grounding OPC-10000-7 §4.2/§4.3. |

Because the manifest lists the profile(s) each claim applies to, a claim that is
*doc-corrected* to a narrower profile (e.g. RegisterNodes → full only) automatically
stops requiring a backing test in the profiles it no longer claims, and a claim that
is *implemented* (e.g. AddNodes duplicate → `Bad_NodeIdExists`) must name its test or
the build fails. The status stays **profile-targeting**: this is conformance
*evidence discipline* only — it is not a claim that any profile has been certified or
externally validated by the Compliance Test Tool.
