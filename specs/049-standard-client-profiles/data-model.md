# Data Model: Standard UA Client 2025 Profile

## mu_client_subscription_t

Client-side subscription state.

| Field | Type | Description |
|-------|------|-------------|
| `in_use` | `bool` | Subscription slot active |
| `subscription_id` | `uint32` | Server-assigned subscription ID |
| `publishing_interval_ms` | `uint32` | Negotiated publishing interval |
| `lifetime_count` | `uint32` | Lifetime count from server |
| `max_keep_alive_count` | `uint32` | Max keep-alive from server |
| `monitored_item_count` | `uint32` | Number of monitored items |
| `last_sequence_number` | `uint32` | Last acknowledged sequence number |
| `publish_pending` | `bool` | Publish request is outstanding |

## mu_client_monitored_item_t

Client-side monitored item state.

| Field | Type | Description |
|-------|------|-------------|
| `in_use` | `bool` | Monitored item slot active |
| `monitored_item_id` | `uint32` | Server-assigned monitored item ID |
| `subscription_id` | `uint32` | Owning subscription ID |
| `node_id` | `mu_nodeid_t` | Monitored node |
| `attribute_id` | `uint32` | Monitored attribute (usually Value=13) |
| `sampling_interval_ms` | `uint32` | Negotiated sampling interval |
| `queue_size` | `uint32` | Configured queue depth |
| `discard_oldest` | `bool` | Queue overflow policy |
| `filter_type` | `uint8` | 0=none, 1=data change, 2=event |
| `queue` | `mu_variant_t[]` | Notification value buffer |
| `queue_head` | `uint8` | Ring buffer head |
| `queue_tail` | `uint8` | Ring buffer tail |
| `queue_count` | `uint8` | Items in queue |
| `overflow` | `bool` | Queue overflow since last drain |

## Call Flow

```
Application                    Client Library                    Server
    |                              |                               |
    |-- mu_client_create_subscription() -->|                       |
    |                              |---- CreateSubscription ------>|
    |                              |<--- CreateSubscriptionResp ---|
    |<--- subscription_id ---------|                               |
    |                              |                               |
    |-- mu_client_create_monitored_item() ->|                      |
    |  (node, filter)             |---- CreateMonitoredItems ---->|
    |                              |<-- CreateMonitoredItemsResp -|
    |<--- monitored_item_id ------|                               |
    |                              |                               |
    |                              |---- Publish ----------------->| (parked)
    |                              |<--- PublishResponse ---------|
    |                              |   (DataChangeNotification)    |
    |-- mu_client_poll() --------->|                               |
    |<--- new notification value --|                               |
    |                              |---- Publish ----------------->| (re-park)
```
