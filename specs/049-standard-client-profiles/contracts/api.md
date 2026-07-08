# API Contract: Subscription and MonitoredItem

## mu_client_create_subscription

```c
/* Create a subscription on the server.
 * @param client     Connected client
 * @param interval   Requested publishing interval in ms (0 = server default)
 * @param out_id     Receives the server-assigned subscription ID
 * @return MU_STATUS_GOOD on success */
opcua_statuscode_t mu_client_create_subscription(mu_client_t *client, uint32_t interval, uint32_t *out_id);
```

## mu_client_delete_subscription

```c
opcua_statuscode_t mu_client_delete_subscription(mu_client_t *client, uint32_t subscription_id);
```

## mu_client_create_monitored_item

```c
/* Create a monitored item on an existing subscription.
 * @param client           Connected client
 * @param subscription_id  Subscription to attach to
 * @param node_id          Node to monitor
 * @param attribute_id     Attribute to monitor (usually 13 for Value)
 * @param sampling_interval Requested sampling interval in ms
 * @param filter_type      0=none, 1=data change, 2=event
 * @param filter_data      Filter-specific data (DataChangeFilter or EventFilter)
 * @param out_id           Receives the server-assigned monitored item ID
 * @return MU_STATUS_GOOD on success */
opcua_statuscode_t mu_client_create_monitored_item(mu_client_t *client, uint32_t subscription_id,
    const mu_nodeid_t *node_id, uint32_t attribute_id, uint32_t sampling_interval,
    uint8_t filter_type, const void *filter_data, uint32_t *out_id);
```

## mu_client_delete_monitored_item

```c
opcua_statuscode_t mu_client_delete_monitored_item(mu_client_t *client, uint32_t subscription_id, uint32_t monitored_item_id);
```

## mu_client_write

```c
/* Write a value to a server node.
 * @param client      Connected client
 * @param node_id     Target node
 * @param attribute_id Target attribute
 * @param value       Value to write
 * @param out_status  Receives the per-item status code from the server
 * @return MU_STATUS_GOOD if the request was processed */
opcua_statuscode_t mu_client_write(mu_client_t *client, const mu_nodeid_t *node_id,
    uint32_t attribute_id, const mu_variant_t *value, opcua_statuscode_t *out_status);
```

## mu_client_call

```c
/* Call a method on a server object.
 * @param client         Connected client
 * @param object_id      Object node ID
 * @param method_id      Method node ID
 * @param input_args     Input argument array
 * @param input_count    Number of input arguments
 * @param output_args    Output argument array (caller-provided)
 * @param output_count   On input: capacity; on output: number of returned args
 * @return MU_STATUS_GOOD on success */
opcua_statuscode_t mu_client_call(mu_client_t *client, const mu_nodeid_t *object_id,
    const mu_nodeid_t *method_id, const mu_variant_t *input_args, size_t input_count,
    mu_variant_t *output_args, size_t *output_count);
```

## mu_client_poll

```c
/* Process one incoming Publish response (non-blocking).
 * Must be called periodically to drain notification queues.
 * @param client  Client with parked subscription(s)
 * @return MU_STATUS_GOOD if a notification was processed */
opcua_statuscode_t mu_client_poll(mu_client_t *client);
```
