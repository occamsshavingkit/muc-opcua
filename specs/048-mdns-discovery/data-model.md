# Data Model: mDNS Discovery

## mu_mdns_adapter_t

Platform adapter for mDNS DNS-SD service publishing.

| Field | Type | Description |
|-------|------|-------------|
| `context` | `void *` | Opaque adapter context (socket fd, state) |
| `publish` | `void (*)(void *context, const char *hostname, uint16_t port, const char *app_uri, const char *path)` | Publish an `_opcua-tcp._tcp` DNS-SD record |
| `unpublish` | `void (*)(void *context)` | Remove the published DNS-SD record |

## mu_server_config_t Addition

| Field | Type | Description |
|-------|------|-------------|
| `mdns_adapter` | `mu_mdns_adapter_t *` | Optional mDNS adapter. NULL = disabled (default). |

## Call Flow

```
mu_server_init()
  → config_validate()
  → init server state
  → if (config->mdns_adapter && config->mdns_adapter->publish)
      extract hostname, port from endpoint_url
      mdns_adapter->publish(context, hostname, port, application_uri, "/discovery")

mu_server_close()
  → if (config->mdns_adapter && config->mdns_adapter->unpublish)
      mdns_adapter->unpublish(context)
  → shutdown TCP, clear state
```
