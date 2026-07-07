# Service Sequences: Nano Client

## Client Connection Lifecycle

```text
Client                          Server
  |                               |
  |-- TCP Connect -------------->|
  |<-- Hello Acknowledge --------|
  |-- OpenSecureChannel -------->|
  |<-- OpenSecureChannel resp ---|
  |-- CreateSession ------------>|
  |<-- CreateSession resp -------|
  |-- ActivateSession ---------->|
  |<-- ActivateSession resp -----|
  |                               |
  |=== ACTIVE SESSION ===         |
  |                               |
  |-- Read --------------------->|
  |<-- Read resp ----------------|
  |-- Browse ------------------->|
  |<-- Browse resp --------------|
  |                               |
  |-- CloseSession ------------->|
  |<-- CloseSession resp --------|
  |-- CloseSecureChannel ------->|
  |<-- CloseSecureChannel resp --|
  |-- TCP Disconnect ----------->|
```

## Read Sequence (detail)

```text
mu_client_read(client, node_ids[], num_nodes)
  |
  +-> Build ReadRequest (OPC-10000-4 §5.10.2)
  |   mu_binary_writer_t writer = {client->send_buf, ...}
  |   write_opaquesecurityheader(writer, client->channel)
  |   write_sequencenumber(writer, client->channel.seq++)
  |   write_requestheader(writer, client->session.auth_token)
  |   write_readrequest(writer, node_ids, num_nodes, timestamps, max_age)
  |
  +-> Send via SecureChannel
  |   client->transport->send(client->transport, writer.data, writer.length)
  |
  +-> Receive response
  |   client->transport->recv(client->transport, client->recv_buf, buf_size)
  |
  +-> Parse ReadResponse (OPC-10000-4 §5.10.2)
  |   mu_binary_reader_t reader = {client->recv_buf, recv_len}
  |   read_opaque_security_header(reader, client->channel)
  |   read_sequencenumber(reader)
  |   read_readresponse(reader, &results[], &num_results)
  |
  +-> Return results + StatusCode
      return MU_STATUS_GOOD
```

## Error Handling

```text
Client                          Server
  |                               |
  |-- OpenSecureChannel -------->|
  |<-- ServiceFault -------------|
  |   (e.g., Bad_SecurityPolicyRejected)
  |                               |
  +-> Return error StatusCode
      client->state = MU_CLIENT_ERROR
```

## Timeout Handling

```text
Client                          Server
  |                               |
  |-- Read --------------------->| (lost/delayed)
  |   (timeout_ms elapsed)        |
  |                               |
  +-> Return Bad_Timeout
      client->state = MU_CLIENT_DISCONNECTED
      (caller must reconnect)
```
