# Conformance: Method Server Facet (spec 062)

This server implements the OPC UA **Method Server Facet** (OPC-10000-4 §5.11 Call
service) — invoking arbitrary integrator-registered Methods plus the built-in Base-Info
methods. Gated behind **`MUC_OPCUA_METHOD_SERVER`** (default **ON standard/full**; the
Standard profile's `features.h` guard requires it). The legacy flag
`MUC_OPCUA_CUSTOM_METHODS` is an alias, resolved to `MUC_OPCUA_METHOD_SERVER` in
`config.h` so existing `-D` builds keep working.

Grounded against OPC-10000-4 §5.11 (Call), OPC-10000-3 §5.7 (Method NodeClass,
Executable/UserExecutable), OPC-10000-5 §12.2.12.1 (`Argument` DataType, NodeId **296**;
`Argument_Encoding_DefaultBinary` = ns0 i=**298**).

## Registration API

```c
opcua_statuscode_t mu_server_register_method_callback(
    mu_server_t *server, const mu_nodeid_t *method_id,
    mu_method_callback_t callback, void *context,
    const mu_argument_t *input_args, size_t input_count,
    const mu_argument_t *output_args, size_t output_count,
    opcua_boolean_t executable);
```

- `context` is delivered to the callback (`mu_method_callback_t` carries a `void *context`
  as its second parameter) so methods can be stateful.
- The optional `mu_argument_t` input/output signature (caller-owned, must outlive the
  server) drives per-argument validation and the browsable InputArguments/OutputArguments
  metadata; pass `NULL`/`0` to skip validation.
- `executable` sets the Method's Executable attribute.
- Capacity: `MU_MAX_REGISTERED_METHODS` (8). Re-registering a `method_id` overwrites in
  place; overflow → `Bad_OutOfMemory`.

## Served Method attributes

| Attribute | Id | Value |
|---|---:|---|
| Executable | 21 | Boolean `true` for any Method node (Read) |
| UserExecutable | 22 | Boolean `true` (Read) |

Reading Executable/UserExecutable on a non-Method node → `Bad_AttributeIdInvalid`.
Per-registration non-executability is enforced at **Call** time (`Bad_NotExecutable`),
independent of the attribute Read (the static node model has no per-node flag).

## Argument signature metadata

The built-in methods' InputArguments/OutputArguments Property Values are served as a
browsable `Argument[]` (Variant of `ExtensionObject` array, each `Argument_Encoding_
DefaultBinary` i=298):

| Method | Inputs | Outputs |
|---|---|---|
| GetMonitoredItems (11492) | SubscriptionId : UInt32 | ServerHandles : UInt32[], ClientHandles : UInt32[] |
| ResendData (12873) | SubscriptionId : UInt32 | — |

`mu_argument_t` = { name (String), dataType (NodeId), valueRank (Int32: -1 scalar / ≥1
array), description (LocalizedText) }. `arrayDimensions` is encoded as null (not modelled;
valueRank is the validated dimension). Integrators expose a custom method's signature by
giving its InputArguments Property node a static `Argument[]` Value in the same form (the
`mu_binary_write_argument` encoder + the ExtensionObject-array Variant path support it).

## Call status codes (OPC-10000-4 §5.11.2)

| Condition | Status |
|---|---|
| Object NodeId not in address space | `Bad_NodeIdUnknown` |
| Method node absent, or no registered callback | `Bad_MethodInvalid` |
| Registered method marked non-executable | `Bad_NotExecutable` |
| Too few inputs vs declared signature | `Bad_ArgumentsMissing` |
| Too many inputs vs declared signature | `Bad_TooManyArguments` |
| An input's type/rank ≠ declared | operation `Bad_InvalidArgument`, `inputArgumentResults[i]` = `Bad_TypeMismatch` |
| > `MU_DISPATCH_MAX_CALL_METHODS` (8) methods / > 4 input args | `Bad_TooManyOperations` |
| Callback returns non-Good | that status (outputs discarded) |

Per-input type-checking compares the builtin DataType NodeId (ns0 numeric == the
`mu_builtin_type_t` enum value) and valueRank; complex/non-ns0 declared types are accepted
without deep checking (documented subset — no `Bad_OutOfRange` value-range checks).

## Verification

- `tests/unit/test_method_call_arbitrary.c` — registration KATs: context/signature/
  executable stored, duplicate-overwrite, capacity-full → `Bad_OutOfMemory`, null-arg reject.
- `tests/unit/test_method_call.c` — wire Call of a custom method (context delivered);
  Executable/UserExecutable attribute Read; non-executable → `Bad_NotExecutable`;
  type-mismatch → `Bad_InvalidArgument` + per-input result; InputArguments Value encodes a
  decodable `Argument[]` with an exact ExtensionObject length (the #288 slicing lesson).
- `tests/unit/test_method_call_errors.c` — Bad_MethodInvalid / ArgumentsMissing /
  TooManyOperations negative paths.

## Out of scope

- Method-level RolePermissions / fine-grained authorization beyond the Executable hook.
- Async/long-running methods (Call is synchronous).
- `arrayDimensions` deep validation; `Bad_OutOfRange` value-range checks.
- Auto-serving a *custom* method's Argument metadata from its registration (integrator
  supplies the InputArguments Value; the encoder supports the form).
