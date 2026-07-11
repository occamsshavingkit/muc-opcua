# Spec 062: Method Server Facet (B3)

Project-B facet B3 (roadmap `docs/superpowers/specs/2026-07-09-...-roadmap.md`). Feature
flag **`MUC_OPCUA_METHOD_SERVER`** (exists; ON standard/full; the Standard profile's
`features.h` guard already *requires* it). Scope stance (per B1/B2, locked with user):
**implement the facet completely** — a real, browsable, validated Method Server, not the
current split of a hard-coded built-in dispatcher plus a half-wired callback path.

Grounded against OPC-10000-4 §5.11 (Call service), OPC-10000-3 (Method NodeClass,
Executable/UserExecutable), OPC-10000-5 §12.2.12.1 (`Argument` DataType, NodeId **296**,
confirmed via the reference tool), OPC-10000-6 (CallMethodRequest/Result encoding).

## Motivating gap (from a full read of the current implementation)

**`MUC_OPCUA_METHOD_SERVER` — the flag the Standard Profile mandates — is declared but
never implemented; the working code lives under a *different* flag.**

- **Two divergent APIs.** The working path is gated `MUC_OPCUA_CUSTOM_METHODS`:
  `mu_server_register_method_callback` (`server.h:143`, impl `server/dispatch.c:5`) into a
  fixed `registered_methods[MU_MAX_REGISTERED_METHODS]` (=**8**, `server_internal.h:139`),
  invoked from `write_single_call_method_result` (`dispatch_method.c:181-222`). Separately,
  `services/method.h:35-41` declares `mu_method_server_register` / `mu_method_server_dispatch`
  + a richer `mu_method_call_t` (carries `input_argument_results`) + `MU_MAX_METHOD_REGISTRATIONS`
  (=**16**) — **zero implementations** (grep-confirmed); `MUC_OPCUA_METHOD_SERVER` is
  referenced only by CMake and the `features.h:82` guard, by **no `.c` file**.
- **Broken gating.** `METHOD_SERVER=ON, CUSTOM_METHODS=OFF` is buildable: the Call path's
  `#else` (`dispatch_method.c:220`) returns `Bad_MethodInvalid` for every custom method while
  the header advertises unimplemented `mu_method_server_*`. The Call handler itself is gated on
  `SUBSCRIPTIONS && SUBSCRIPTIONS_STANDARD && BASE_TYPE_SYSTEM` (`dispatch_method.c:24`), not a
  method flag (the file comment notes no `MUC_OPCUA_METHOD_CALL` guard exists).
- **Method attributes not served.** `Executable`(21)/`UserExecutable`(22) enums exist
  (`read.h:33`) but `read_attribute` (`read_attribute.c:203-241`) has no case → `default →
  Bad_AttributeIdInvalid`. Consequently **`Bad_NotExecutable`** (0x81110000, `status.h:161`) is
  **never emitted** — the Call path never checks executability.
- **Argument metadata is empty.** Method nodes 11492/12873 link `HasProperty` →
  InputArguments/OutputArguments Variables (11493/11494/12874) but those carry a **NULL value
  source** (`base_nodes.c:955/963/1040`) → reading their Value gives `Bad_NotReadable`. There is
  **no `Argument`(296) struct encoder** anywhere, so a client cannot browse a method's signature.
- **No argument validation for custom methods.** The custom path does no count/type checking and
  always writes `inputArgumentResults` count **0** (`dispatch_method.c:219`); the
  `mu_method_call_t.input_argument_results` field is never populated. Type mismatches on built-ins
  return `Bad_InvalidArgument` where §5.11 expects `Bad_TypeMismatch`.
- **Weak lookup semantics.** Non-existent object → `Bad_NodeIdInvalid` where §5.11 expects
  `Bad_NodeIdUnknown`; the method is not verified to be `HasComponent` of the supplied object; no
  session/user-executable gating (`Bad_UserAccessDenied`).
- **Dead `context`.** `mu_server_register_method_callback` stores a `void *context`
  (`server_internal.h`) that is **never passed** to the callback (`dispatch_method.c:213`).
- **Contract drift.** `specs/037.../method-call.md` and `specs/005.../method-call.md` document an
  API (`mu_server_register_method`, `mu_argument_t`, `Bad_TypeMismatch`) that does not match the
  shipped code. `test_method_call_arbitrary.c` has a comment-only "test"
  (`TEST_PASS_MESSAGE("validated by dispatch_method.c:...")`).

## Grounded Call model

**CallMethodRequest** (§5.11.2): `objectId: NodeId`, `methodId: NodeId`,
`inputArguments: Variant[]`. **CallMethodResult**: `statusCode`,
`inputArgumentResults: StatusCode[]`, `inputArgumentDiagnosticInfos: DiagnosticInfo[]`,
`outputArguments: Variant[]` (encoding already correct in `write_call_method_result`).

**Argument** (DataType **296**, OPC-10000-5 §12.2.12.1): `name: String`, `dataType: NodeId`,
`valueRank: Int32`, `arrayDimensions: UInt32[]`, `description: LocalizedText`. Served as the
`Argument[]` Value of the InputArguments/OutputArguments Properties.

**Method attributes** (OPC-10000-3): `Executable: Boolean` (21), `UserExecutable: Boolean` (22).

**Status codes** (§5.11.2) — pin exact values from `status.h`:
- Operation-level: `Bad_NodeIdUnknown`, `Bad_NodeIdInvalid`, `Bad_MethodInvalid`,
  `Bad_NotExecutable`, `Bad_UserAccessDenied`, `Bad_ArgumentsMissing`, `Bad_TooManyArguments`.
- Per-input-argument (`inputArgumentResults[]`): `Bad_TypeMismatch`, `Bad_OutOfRange`,
  `Bad_InvalidArgument`; `Good` otherwise.

## Requirements

**FR-1 — One Method Server API under `MUC_OPCUA_METHOD_SERVER`.** Make `METHOD_SERVER` the
facet flag that gates the working custom-method machinery (retarget/rename, keeping
`mu_server_register_method_callback` as the public entry). Retire the dead
`mu_method_server_register`/`_dispatch` header declarations (or implement them as thin wrappers)
so there is a single, real API; reconcile the capacity macro to one name/value. `features.h`:
`METHOD_SERVER` requires the base Call machinery (Subscriptions-Standard + BaseTypeSystem today),
and the Standard-profile guard stays. `CUSTOM_METHODS` either becomes an alias of `METHOD_SERVER`
or is removed — no build may enable the advertised API without the implementation behind it.

**FR-2 — Pass `context` to the callback.** The stored per-method `void *context` must reach the
callback (extend `mu_method_callback_t` or thread it), so integrators can register stateful
methods. Currently dropped (`dispatch_method.c:213`).

**FR-3 — Serve `Executable`/`UserExecutable` + enforce them.** Add attribute-21/22 cases to
`read_attribute` returning Boolean for Method nodes (Executable=true for served/registered
methods; UserExecutable per session, default = Executable). The Call path must reject a
non-executable method with **`Bad_NotExecutable`** (currently unreachable).

**FR-4 — Serve `Argument[]` signature metadata.** Add an `Argument`(296) struct encoder and give
the InputArguments/OutputArguments Properties a real `Argument[]` Value (built-in methods'
signatures, and a way for a registered method to declare its in/out `Argument`s). A client must be
able to Browse→Read a method's InputArguments to discover its signature (the facet's core
requirement). Include the ExtensionObject/`Argument`-array DataType nodes needed to decode it.

**FR-5 — Validate arguments; emit per-argument results.** For a called method with a declared
signature: too few inputs → `Bad_ArgumentsMissing`; too many → `Bad_TooManyArguments`; per input,
type-check against the declared `dataType`/`valueRank` → `inputArgumentResults[i]` =
`Bad_TypeMismatch`/`Bad_OutOfRange`/`Bad_InvalidArgument` or `Good`, and fail the call
`Bad_InvalidArgument` if any input is bad (§5.11.2). Populate the `input_argument_results` the
model already reserves (`method.h:30`), instead of the hardcoded count 0.

**FR-6 — Correct lookup semantics.** Unknown object/method NodeId → `Bad_NodeIdUnknown` (not
`Bad_NodeIdInvalid` for malformed vs unknown, per §5.11); verify the method is `HasComponent` of
the supplied object → else `Bad_MethodInvalid`; optional user gating → `Bad_UserAccessDenied`.

**FR-7 — Conformance doc + contract reconciliation + size.** `docs/conformance/method-server.md`
(the API, served attributes, Argument metadata, status codes, built-in + custom methods); update
`specs/037`/`specs/005` method-call.md (or supersede-note) to the shipped API; size ledger delta;
`full` stays under the 128 KiB stopper.

## Verification (test-first)

- **Registration/API KATs** — register, overwrite-duplicate, capacity-full → `Bad_*`; the stored
  `context` is delivered to the callback (replace the comment-only test).
- **Executability** — Read Executable/UserExecutable on a Method node → Boolean; Call a
  non-executable method → `Bad_NotExecutable`.
- **Argument metadata** — Browse a method's HasProperty, Read InputArguments Value, decode the
  `Argument[]`, assert name/dataType/valueRank for a built-in (e.g. GetMonitoredItems) and a
  registered custom method (non-circular: hand-computed expected fields).
- **Argument validation** — custom method with a declared signature: missing/extra/typemismatched
  inputs → the grounded operation-level + per-`inputArgumentResults[i]` codes; happy path → outputs.
- **Batch Call** — multiple MethodsToCall with mixed results; array-valued input argument.
- **Wire e2e** — a CallRequest over the dispatch path returns a well-formed CallResponse
  (result count, per-method result layout, trailing DiagnosticInfos) a slicing client can decode
  (apply the #288 lesson: validate encoded lengths).
- Full matrix green; non-METHOD_SERVER profiles unchanged.

## Out of scope

- Method-level RolePermissions / fine-grained authorization beyond a UserExecutable hook.
- Async/long-running methods (Call is synchronous here).
- Optional-argument descriptions (`HasOptionalInputArgumentDescription` i=131) — BaseEventType/
  A&C method surfaces stay as-is.
- Non-scalar `arrayDimensions` deep validation beyond valueRank/type (document the subset).

## Scope decision to confirm before implementation

**API-unification direction (FR-1)** is the one integrator-facing, hard-to-reverse fork — surface
it to the user after this spec is committed (retarget existing `CUSTOM_METHODS` machinery to
`METHOD_SERVER` vs implement the `method.h` API vs alias). Everything else follows the "implement
completely / follow the spec" mandate.

## On approval

speckit plan/tasks, execute test-first, PR against `main` with a merge commit. B3 of Project B;
remeasure `full` `.text` and continue to B4 while under the stopper.
