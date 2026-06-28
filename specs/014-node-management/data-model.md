# Data Model: NodeManagement Services

## 1. Dynamic Address Space (C Structs)

To accommodate dynamic nodes and references without using a heap, we add a pre-allocated dynamic region to the server's state:

```c
typedef struct {
    mu_node_t *nodes;
    size_t nodes_capacity;
    size_t nodes_count;
    
    mu_reference_t *references;
    size_t references_capacity;
    size_t references_count;
} mu_dynamic_address_space_t;
```

## 2. Server Configuration

Add configurable sizes for the dynamic node arrays:

```c
#ifndef MU_MAX_DYNAMIC_NODES
#define MU_MAX_DYNAMIC_NODES 32
#endif

#ifndef MU_MAX_DYNAMIC_REFERENCES
#define MU_MAX_DYNAMIC_REFERENCES 64
#endif
```

## 3. NodeManagement Request/Response Structures

```c
typedef struct {
    mu_nodeid_t requested_new_node_id;
    mu_nodeid_t parent_node_id;
    mu_nodeid_t reference_type_id;
    mu_expanded_nodeid_t browse_name;
    mu_nodeclass_t node_class;
    mu_extensionobject_t node_attributes;
    mu_expanded_nodeid_t type_definition;
} mu_add_nodes_item_t;

typedef struct {
    mu_statuscode_t status_code;
    mu_nodeid_t added_node_id;
} mu_add_nodes_result_t;

// DeleteNodes, AddReferences, DeleteReferences similar structures
```
