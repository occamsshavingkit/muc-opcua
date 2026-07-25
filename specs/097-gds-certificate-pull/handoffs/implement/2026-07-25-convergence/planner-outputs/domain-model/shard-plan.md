# Domain-model shard plan

## S03-domain-model-01 — T040

- Add only the six missing forward HasSubtype edges in `base_nodes.c`.
- Preserve all existing inverse edges and all instance/component relationships.
- Gate targets with the Pull CU exactly as their target nodes are gated.
- Cite OPC-10000-12 §7.9.2, §7.8.3.1, and §7.8.4.1-§7.8.4.9 beside the hierarchy.

## S04-domain-model-01 — T041

- Add the twelve missing forward instance-hierarchy edges in `base_nodes.c`.
- Preserve the existing inverse Organizes references and all type hierarchy edges.
- Keep every new edge under the same Pull/type-information gates as its target nodes.
- Cite OPC-10000-12 §7.8.3.1 and §7.9.2 beside the edge declarations.
