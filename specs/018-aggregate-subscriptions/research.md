# Research Findings: Aggregate Subscriptions

## 1. Aggregate State Integration

- **Decision**: Integrate a compact `mu_aggregate_state_t` structure directly into the existing `mu_monitored_item_t` structure.
- **Rationale**: Storing the aggregation state inside each monitored item slot conforms to the project's zero-heap policy. It ensures no dynamic allocation is needed when a client creates an aggregate subscription.
- **Alternatives considered**:
  - A separate global array of aggregate calculators. Rejected because it introduces look-up latency and wastes static RAM.

## 2. Sampling and Accumulation Strategy

- **Decision**: Reuse the existing monitored item sampling/polling mechanism. Each time the item samples a new value, the value is fed into the running accumulator instead of being queued directly for publish.
- **Rationale**: Leverages the current server architecture and minimizes the changes required.
- **Alternatives considered**:
  - Separate polling timers for aggregates. Rejected as it adds redundant code and timer structure overhead.

## 3. Publication Mechanism

- **Decision**: When the processing interval expires, compute the final average, minimum, or maximum from the accumulator, create a new `DataValue`, push it to the subscription publish queue, and reset the accumulator.
- **Rationale**: Aligns with OPC UA Part 4 requirements for aggregate publishing intervals.
- **Alternatives considered**:
  - Pushing intermediate values. Rejected as it violates the core intent of AggregateFilter downsampling.
