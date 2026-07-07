# API Contract: mDNS Discovery

## mu_mdns_adapter_t

```c
typedef struct mu_mdns_adapter {
    void *context;

    /* Publish an _opcua-tcp._tcp DNS-SD record.
     * - hostname: extracted from endpoint_url (IPv4, IPv6, or hostname)
     * - port: extracted from endpoint_url
     * - app_uri: from config->application_uri
     * - path: always "/discovery" per OPC-10000-12 §6.3.3
     * Called once during mu_server_init. Best-effort; callbacks are void-returning.
     * Failure to publish does not abort server init. If hostname is empty,
     * the callback should return immediately (no-op). */
    void (*publish)(void *context, const char *hostname, uint16_t port,
                    const char *app_uri, const char *path);

    /* Remove the previously published DNS-SD record.
     * Called once during mu_server_close. Best-effort; does not block shutdown. */
    void (*unpublish)(void *context);
} mu_mdns_adapter_t;
```

## mu_mdns_adapter_noop

```c
/* Static no-op adapter. Use when mDNS is not needed.
 * Both callbacks are NULL-safe no-ops. */
extern mu_mdns_adapter_t mu_mdns_adapter_noop;
```

## mu_host_mdns_adapter_init

```c
/* Initialize the host POSIX mDNS adapter using UDP multicast on 224.0.0.251:5353.
 * Creates a UDP socket, joins the multicast group, and stores the socket fd in
 * adapter->context. Returns MU_STATUS_GOOD on success. On failure (socket or
 * bind error), returns an appropriate error status code — the caller should
 * treat a failed init as a misconfiguration. Because the server stores the
 * adapter by pointer, a failed init adapter should not be passed to config.
 * Performance: O(1) — constant-time socket creation, no heap usage. */
opcua_statuscode_t mu_host_mdns_adapter_init(mu_mdns_adapter_t *adapter);
```

## DNS-SD Record Format

The POSIX adapter publishes the following records to `224.0.0.251:5353`:

```
_opcua-tcp._tcp.local.  PTR  <hostname>._opcua-tcp._tcp.local.
<hostname>._opcua-tcp._tcp.local.  SRV  0 0 <port> <hostname>.local.
<hostname>.local.  TXT  "path=/discovery" "applicationUri=<app_uri>"
<hostname>.local.  A  <IPv4 address>
```

Per OPC-10000-12 §6.3.3, the TXT record MUST include `path=/discovery`.

## Error Model

- **Publish/unpublish callbacks**: Void-returning (fire-and-forget). Failures are
  logged at the adapter level; the server does not abort on publish failure.
  This matches the best-effort nature of mDNS multicast.
- **`mu_host_mdns_adapter_init`**: Status-code returning. A socket/bind failure
  is a misconfiguration that prevents any mDNS operation. The caller should not
  set `config->mdns_adapter` to a failed adapter.
- **Empty hostname**: The publish callback receives an empty string. It must
  treat this as a no-op (return immediately without sending a multicast packet).
- The adapter struct itself is trivially constructible — zero-initializing a
  `mu_mdns_adapter_t` produces an all-NULL adapter (equivalent to no-op).
