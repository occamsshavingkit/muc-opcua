/* src/services/read/cache.c */
#include "common.h"
#include <stddef.h>
#include <string.h>

#define MU_MAXAGE_ALWAYS_CACHE_THRESHOLD_MS 1e12

opcua_boolean_t mu_read_cache_lookup(mu_read_cache_t *cache, const mu_nodeid_t *node_id, opcua_double_t max_age_ms,
                                     opcua_datetime_t now, mu_variant_t *out) {
    if (!cache)
        return false;
    if (max_age_ms == 0.0) {
        cache->misses++;
        return false;
    }

    for (size_t i = 0; i < MU_READ_CACHE_SLOTS; i++) {
        if (!cache->entries[i].valid)
            continue;

        if (!mu_nodeid_equal(node_id, &cache->entries[i].node_id))
            continue;

        opcua_datetime_t age_ticks = now - cache->entries[i].read_time;

        if (age_ticks < 0) {
            memcpy(out, &cache->entries[i].variant, sizeof(mu_variant_t));
            cache->hits++;
            return true;
        }

        if (max_age_ms >= MU_MAXAGE_ALWAYS_CACHE_THRESHOLD_MS) {
            memcpy(out, &cache->entries[i].variant, sizeof(mu_variant_t));
            cache->hits++;
            return true;
        }

        opcua_datetime_t max_age_ticks = (opcua_datetime_t)(max_age_ms * 10000.0);
        if (age_ticks <= max_age_ticks) {
            memcpy(out, &cache->entries[i].variant, sizeof(mu_variant_t));
            cache->hits++;
            return true;
        }
    }

    cache->misses++;
    return false;
}

void mu_read_cache_store(mu_read_cache_t *cache, const mu_nodeid_t *node_id, const mu_variant_t *val,
                         opcua_datetime_t read_time) {
    if (!cache)
        return;
    memcpy(&cache->entries[0].node_id, node_id, sizeof(mu_nodeid_t));
    memcpy(&cache->entries[0].variant, val, sizeof(mu_variant_t));
    cache->entries[0].read_time = read_time;
    cache->entries[0].valid = true;
}
