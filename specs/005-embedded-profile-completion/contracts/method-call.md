# Contract: Call service — GetMonitoredItems & ResendData


> **Superseded by spec 062 (Method Server Facet).** The shipped API is
> `mu_server_register_method_callback(server, method_id, callback, context,
> input_args, input_count, output_args, output_count, executable)` with a
> `void *context`-carrying `mu_method_callback_t`, gated on `MUC_OPCUA_METHOD_SERVER`
> (`MUC_OPCUA_CUSTOM_METHODS` is an alias). See `docs/conformance/method-server.md`
> for the authoritative contract; the API shapes described below are historical.

**Service**: Call — OPC-10000-4 §5.12.2.2 (CallRequest / CallResponse / CallMethodResult).
**Methods**: GetMonitoredItems, ResendData — defined on the Server object in OPC-10000-5.
**Facet**: `Method Call`, `Base Info GetMonitoredItems Method`, `Base Info ResendData Method`
CUs of the Standard DataChange Subscription 2017 Server Facet (OPC-10000-7 §6.6.17; research.md).
**Build gate**: embedded profile scope (Call dispatch + method nodes). Minimal surface: ONLY
these two methods are dispatched; all others are rejected.

## Request (CallRequest, §5.12.2.2)

| Field | Type | Notes |
|---|---|---|
| requestHeader | RequestHeader | standard |
| methodsToCall | CallMethodRequest[] | each: { objectId: NodeId, methodId: NodeId, inputArguments: Variant[] } |

## Response (CallResponse, §5.12.2.2)

| Field | Type | Notes |
|---|---|---|
| responseHeader | ResponseHeader | standard |
| results | CallMethodResult[] | each: { statusCode, inputArgumentResults[], inputArgumentDiagnosticInfos[], outputArguments[] } |
| diagnosticInfos | DiagnosticInfo[] | empty unless requested |

## Method: GetMonitoredItems (OPC-10000-5 §9.1; behavior via Call §5.12.2.2)

- **objectId**: the Server object; **methodId**: GetMonitoredItems method node.
- **Input**: `subscriptionId` (UInt32).
- **Output**: `serverHandles` (UInt32[]) and `clientHandles` (UInt32[]) — the handle pairs of
  the subscription's monitored items (serverHandle = the server-assigned MonitoredItemId).
- **Errors**: unknown subscription → result `Bad_SubscriptionIdInvalid`; subscription not owned
  by the calling session → cited StatusCode.

## Method: ResendData (OPC-10000-5 §9.2; behavior via Call §5.12.2.2)

- **objectId**: the Server object; **methodId**: ResendData method node.
- **Input**: `subscriptionId` (UInt32).
- **Output**: none.
- **Effect**: marks the subscription so its next Publish re-reports the current value of every
  monitored item (a one-shot resend flag drained in `mu_subscriptions_tick`).
- **Errors**: unknown subscription → `Bad_SubscriptionIdInvalid`.

## Validation / failure (correctness gate)

- `methodId` not GetMonitoredItems/ResendData → result `Bad_MethodInvalid`; `objectId` not the
  Server object → `Bad_NodeIdInvalid`.
- Wrong input-argument count/type → per-argument `inputArgumentResults` StatusCode
  (`Bad_ArgumentsMissing` / `Bad_InvalidArgument` / `Bad_TooManyArguments`), §5.12.2.2.
- methodsToCall length > fixed limit → `Bad_TooManyOperations`. Malformed request → cited decode
  StatusCode; no out-of-bounds read.

## Address-space requirement (US2/US3)

The GetMonitoredItems and ResendData method nodes MUST exist on the Server object (HasComponent),
each with InputArguments/OutputArguments Properties, so Browse/Call resolve them (OPC-10000-5).

## Tests (test-first)

- GetMonitoredItems on a subscription with items → matching serverHandles/clientHandles.
- ResendData → next Publish re-reports current values of all items.
- Unknown method / wrong object / bad arguments → cited StatusCodes.
- Over-length/malformed Call request → cited StatusCodes; ASan-clean; fuzz the decoder.
