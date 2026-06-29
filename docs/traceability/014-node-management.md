# Traceability: 014 NodeManagement Services

## Target OPC UA Profile

- **Node Management Services** (OPC 10000-7)

## Cited OPC UA Normative References

| Feature | Reference | Implementation Notes |
|---------|-----------|----------------------|
| AddNodes Service | OPC 10000-4, 5.7.2 | Implemented in `mu_add_nodes_process` |
| AddReferences Service | OPC 10000-4, 5.7.3 | Implemented in `mu_add_references_process` |
| DeleteNodes Service | OPC 10000-4, 5.7.4 | Implemented in `mu_delete_nodes_process` |
| DeleteReferences Service | OPC 10000-4, 5.7.5 | Implemented in `mu_delete_references_process` |
| OOM limits for dynamic nodes/refs | OPC 10000-4, 5.7.2.2 | `Bad_OutOfMemory` returned when `MU_MAX_DYNAMIC_NODES` or `MU_MAX_DYNAMIC_REFERENCES` reached |
| DeleteTargetReferences flag | OPC 10000-4, 5.7.4.2 | Supported |
| Auto-assigning NodeId | OPC 10000-4, 5.7.2.2 | The server creates a NodeId in a server-chosen namespace if the client leaves it blank |
