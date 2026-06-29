# Quickstart Guide: Aggregate Subscriptions

This guide explains how to enable and configure `AggregateFilter` subscriptions.

## 1. Configure Server

Ensure `MICRO_OPCUA_SERVICE_HISTORY` (which provides the aggregate ID definitions) or `MICRO_OPCUA_SERVICE_SUBSCRIPTIONS` is defined in `include/micro_opcua/config.h`.

## 2. Client Usage

When creating a MonitoredItem via the `CreateMonitoredItems` service:

1. Set the `itemToMonitor` NodeId to a numeric variable node.
2. Set the `monitoringMode` to `Reporting`.
3. Set the `requestedParameters.filter` ExtensionObject:
   - **TypeId**: `MU_ID_AGGREGATEFILTER_ENCODING_DEFAULTBINARY` (`730`, OPC-10000-4 §7.22.4)
   - **Body**:
     - `startTime`: DateTime (when the calculation starts, e.g., 0)
     - `processingInterval`: Double (milliseconds, e.g., 5000.0)
     - `aggregateType`: NodeId (numeric namespace 0: `2342` for Average, `2346` for Minimum, `2347` for Maximum)
     - `aggregateConfiguration`: Configuration byte mask

## 3. Verify Output

The server will emit a publish notification containing the average, min, or max value of the variable computed over every `processingInterval` duration.
