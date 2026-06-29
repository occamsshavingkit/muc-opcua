# Quickstart Guide: Aggregate Subscriptions

This guide explains how to enable and configure `AggregateFilter` subscriptions.

## 1. Configure Server

Ensure `MICRO_OPCUA_SERVICE_HISTORY` (which provides the aggregate ID definitions) or `MICRO_OPCUA_SERVICE_SUBSCRIPTIONS` is defined in `include/micro_opcua/config.h`.

## 2. Client Usage

When creating a MonitoredItem via the `CreateMonitoredItems` service:

1. Set the `itemToMonitor` NodeId to a numeric variable node.
2. Set the `monitoringMode` to `Reporting`.
3. Set the `requestedParameters.filter` ExtensionObject:
   - **TypeId**: `MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY` (729)
   - **Body**:
     - `startTime`: DateTime (when the calculation starts, e.g., 0)
     - `processingInterval`: Double (milliseconds, e.g., 5000.0)
     - `aggregateType`: NodeId (numeric namespace 0: 11565 for Average, 11569 for Minimum, 11570 for Maximum)
     - `aggregateConfiguration`: Configuration byte mask

## 3. Verify Output

The server will emit a publish notification containing the average, min, or max value of the variable computed over every `processingInterval` duration.
