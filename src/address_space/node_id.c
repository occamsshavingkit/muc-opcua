/* src/address_space/node_id.c */
#include "base_nodes.h"
#include "muc_opcua/address_space.h"
#include <string.h>

#undef mu_address_space_find_node

static int mu_nodeid_compare_direct(const mu_nodeid_t *left, const mu_nodeid_t *right) {
    if (left->namespace_index < right->namespace_index) {
        return -1;
    }
    if (left->namespace_index > right->namespace_index) {
        return 1;
    }

    if (left->identifier_type < right->identifier_type) {
        return -1;
    }
    if (left->identifier_type > right->identifier_type) {
        return 1;
    }

    switch (left->identifier_type) {
    case MU_NODEID_NUMERIC:
        if (left->identifier.numeric < right->identifier.numeric) {
            return -1;
        }
        if (left->identifier.numeric > right->identifier.numeric) {
            return 1;
        }
        return 0;

    case MU_NODEID_STRING: {
        opcua_int32_t len1 = left->identifier.string.length;
        opcua_int32_t len2 = right->identifier.string.length;
        if (len1 < len2) {
            return -1;
        }
        if (len1 > len2) {
            return 1;
        }
        if (len1 <= 0) {
            return 0;
        }
        if (!left->identifier.string.data || !right->identifier.string.data) {
            if (left->identifier.string.data < right->identifier.string.data) {
                return -1;
            }
            if (left->identifier.string.data > right->identifier.string.data) {
                return 1;
            }
            return 0;
        }
        return memcmp(left->identifier.string.data, right->identifier.string.data, (size_t)len1);
    }
    case MU_NODEID_GUID:
        return memcmp(left->identifier.guid, right->identifier.guid, 16);
    case MU_NODEID_OPAQUE:
        if (left->identifier.opaque.length < right->identifier.opaque.length) {
            return -1;
        }
        if (left->identifier.opaque.length > right->identifier.opaque.length) {
            return 1;
        }
        if (left->identifier.opaque.length == 0) {
            return 0;
        }
        return memcmp(left->identifier.opaque.data, right->identifier.opaque.data,
                      left->identifier.opaque.length);
    default:
        return 0;
    }
}

static void mu_address_space_rebuild_index(const mu_address_space_t *address_space, mu_address_space_index_t *index) {
    size_t i;

    index->built_for = address_space;
    index->built_count = address_space->node_count;
    index->count = 0;
    index->indexed = false;

    if (address_space->node_count > MU_MAX_ADDRESS_SPACE_NODES || address_space->node_count > 0xffffu) {
        return;
    }

    index->indexed = true;

    for (i = 0; i < address_space->node_count; i++) {
        size_t position = index->count;

        while (position > 0) {
            const mu_node_t *previous = &address_space->nodes[index->order[position - 1u]];
            const mu_node_t *current = &address_space->nodes[i];

            if (mu_nodeid_compare_direct(&previous->node_id, &current->node_id) <= 0) {
                break;
            }

            index->order[position] = index->order[position - 1u];
            position--;
        }

        index->order[position] = (opcua_uint16_t)i;
        index->count++;
    }
}

static const mu_node_t *mu_address_space_find_node_linear(const mu_address_space_t *address_space,
                                                          const mu_nodeid_t *node_id) {
    size_t i;

    for (i = 0; i < address_space->node_count; i++) {
        if (mu_nodeid_equal(&address_space->nodes[i].node_id, node_id)) {
            return &address_space->nodes[i];
        }
    }

    return NULL;
}

static const mu_node_t *mu_address_space_find_node_indexed(const mu_address_space_t *address_space,
                                                           const mu_address_space_index_t *index,
                                                           const mu_nodeid_t *node_id) {
    size_t left = 0;
    size_t right = index->count;

    while (left < right) {
        size_t mid = left + ((right - left) / 2u);
        opcua_uint16_t node_index = index->order[mid];
        const mu_node_t *node = &address_space->nodes[node_index];
        int comparison = mu_nodeid_compare_direct(&node->node_id, node_id);

        if (comparison < 0) {
            left = mid + 1u;
        } else if (comparison > 0) {
            right = mid;
        } else {
            return node;
        }
    }

    return NULL;
}

opcua_boolean_t mu_nodeid_equal(const mu_nodeid_t *n1, const mu_nodeid_t *n2) {
    if (!n1 || !n2) {
        return false;
    }

    if (n1->namespace_index != n2->namespace_index) {
        return false;
    }

    if (n1->identifier_type != n2->identifier_type) {
        return false;
    }

    switch (n1->identifier_type) {
    case MU_NODEID_NUMERIC:
        return n1->identifier.numeric == n2->identifier.numeric;

    case MU_NODEID_STRING:
        if (n1->identifier.string.length != n2->identifier.string.length) {
            return false;
        }
        if (n1->identifier.string.length <= 0) {
            return true;
        }
        if (!n1->identifier.string.data || !n2->identifier.string.data) {
            return n1->identifier.string.data == n2->identifier.string.data;
        }
        return memcmp(n1->identifier.string.data, n2->identifier.string.data, (size_t)n1->identifier.string.length) ==
               0;

    case MU_NODEID_GUID:
        return memcmp(n1->identifier.guid, n2->identifier.guid, 16) == 0;

    case MU_NODEID_OPAQUE:
        if (n1->identifier.opaque.length != n2->identifier.opaque.length) {
            return false;
        }
        if (n1->identifier.opaque.length == 0) {
            return true;
        }
        return memcmp(n1->identifier.opaque.data, n2->identifier.opaque.data,
                      n1->identifier.opaque.length) == 0;

    default:
        return false;
    }
}

opcua_boolean_t mu_nodeid_in_namespace(const mu_nodeid_t *node_id, opcua_uint16_t namespace_index) {
    if (!node_id) {
        return false;
    }
    return node_id->namespace_index == namespace_index;
}

static const mu_node_t *mu_address_space_find_node_binary_direct(const mu_address_space_t *address_space,
                                                                 const mu_nodeid_t *node_id) {
    size_t left = 0;
    size_t right = address_space->node_count;

    while (left < right) {
        size_t mid = left + ((right - left) / 2u);
        const mu_node_t *node = &address_space->nodes[mid];
        int comparison = mu_nodeid_compare_direct(&node->node_id, node_id);

        if (comparison < 0) {
            left = mid + 1u;
        } else if (comparison > 0) {
            right = mid;
        } else {
            return node;
        }
    }

    return NULL;
}

const mu_node_t *mu_address_space_find_node(const mu_address_space_t *address_space, mu_address_space_index_t *index,
                                            const mu_nodeid_t *node_id) {
    if (!address_space || !address_space->nodes || !node_id) {
        return NULL;
    }

    if (index == NULL) {
        if (address_space == mu_base_address_space()) {
            return mu_address_space_find_node_binary_direct(address_space, node_id);
        }
        return mu_address_space_find_node_linear(address_space, node_id);
    }

    if (index->built_for != address_space || index->built_count != address_space->node_count) {
        mu_address_space_rebuild_index(address_space, index);
    }

    if (!index->indexed) {
        return mu_address_space_find_node_linear(address_space, node_id);
    }

    return mu_address_space_find_node_indexed(address_space, index, node_id);
}
