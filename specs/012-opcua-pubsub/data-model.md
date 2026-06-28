# Data Model: PubSub

## Entities

1. **mu_pubsub_connection_t**
   - Fields: UDP port, multicast address (optional), PublisherId.
   - Purpose: Maps to the PubSubConnection in Part 14. Represents the network transport configuration.

2. **mu_pubsub_writer_group_t**
   - Fields: PublishingInterval, WriterGroupId, network message configuration.
   - Purpose: Triggers the publishing of messages at the given interval. Owns the timing.

3. **mu_pubsub_dataset_writer_t**
   - Fields: DataSetWriterId, reference to a list of variables (PublishedDataSet).
   - Purpose: Encodes the actual data fields into a DataSetMessage using UADP Binary.
