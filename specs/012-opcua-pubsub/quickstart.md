# Quickstart: PubSub UADP

```c
// 1. Configure the server for PubSub
mu_server_config_t config = mu_server_config_default();
config.pubsub.enabled = true;
config.pubsub.port = 4840;
config.pubsub.publisher_id = 1234;

// 2. Add a writer group
mu_pubsub_writer_group_t wg = {
    .publishing_interval_ms = 1000,
    .writer_group_id = 1
};
mu_server_add_writer_group(server, &wg);

// 3. Tick the server
// The poll loop will automatically broadcast UDP packets based on the interval
while (true) {
    mu_server_poll(server); 
}
```
