# Contract: Method Call Service


> **Superseded by spec 062 (Method Server Facet).** The shipped API is
> `mu_server_register_method_callback(server, method_id, callback, context,
> input_args, input_count, output_args, output_count, executable)` with a
> `void *context`-carrying `mu_method_callback_t`, gated on `MUC_OPCUA_METHOD_SERVER`
> (`MUC_OPCUA_CUSTOM_METHODS` is an alias). See `docs/conformance/method-server.md`
> for the authoritative contract; the API shapes described below are historical.

**Feature**: 037-standard-server-profile | **OPC Reference**: OPC-10000-4 §5.12.2.2

## Registration API

```c
// Register a custom method callback. Must be called before server start.
mu_status_t mu_server_register_method(
    mu_server_t *server,
    mu_node_id_t method_id,
    mu_method_callback_t callback,
    const mu_argument_t *input_args,
    uint16_t input_count,
    const mu_argument_t *output_args,
    uint16_t output_count
);

// Callback signature:
typedef mu_status_t (*mu_method_callback_t)(mu_method_call_t *call);

// Method call context:
typedef struct {
    mu_node_id_t   method_id;
    mu_node_id_t   object_id;
    mu_variant_t  *input_arguments;
    uint16_t       input_count;
    mu_variant_t  *output_arguments;  // caller-provided, callback fills
    uint16_t       output_count;
    mu_status_t   *input_argument_results;  // per-argument status
    mu_status_t   *callback_status;
} mu_method_call_t;
```

## Dispatch Contract

1. Client sends CallRequest with MethodsToCall[] containing ObjectId + MethodId + InputArguments[]
2. Server looks up method_id in static registration table
3. If found: extract input arguments from request Variant array, invoke callback
4. If not found: return Bad_MethodInvalid in CallMethodResult.StatusCode
5. Callback populates output_arguments and sets callback_status
6. Server encodes CallResponse with Results[] containing StatusCode + OutputArguments[]

## Error Contract

| Condition | StatusCode | OPC UA Reference |
|-----------|------------|------------------|
| MethodId not registered | Bad_MethodInvalid | OPC-10000-4 §7.38.2 |
| Input argument count mismatch | Bad_ArgumentsMissing | OPC-10000-4 §7.38.2 |
| Input argument type mismatch | Bad_TypeMismatch | OPC-10000-4 §7.38.2 |
| Callback returns error | Callback's status code | Implementation-defined |
| ObjectId does not exist | Bad_NodeIdUnknown | OPC-10000-4 §7.38.2 |

## Concurrency

Methods execute synchronously in the service dispatch thread. Callbacks must be non-blocking and return promptly. Long-running operations should use deferred execution patterns outside the callback.

## Gating

`MUC_OPCUA_METHOD_SERVER` CMake flag. When OFF, Call service returns Bad_ServiceUnsupported for non-built-in methods.
