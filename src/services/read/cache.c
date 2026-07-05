/* src/services/read/cache.c */
#include "common.h"
#include <stddef.h>
#include <string.h>

#define MU_READ_CACHE_SLOTS 1UL
#define MU_MAXAGE_ALWAYS_CACHE_THRESHOLD_MS 1e12

typedef struct {
    mu_nodeid_t node_id;
    mu_variant_t variant;
    opcua_datetime_t read_time;
    opcua_boolean_t valid;
} mu_read_cache_entry_t;

static mu_read_cache_entry_t mu_read_cache[MU_READ_CACHE_SLOTS];
static size_t mu_read_cache_hits = 0;
static size_t mu_read_cache_misses = 0;

opcua_boolean_t mu_read_cache_lookup(const mu_nodeid_t *node_id, opcua_double_t max_age_ms, opcua_datetime_t now,
                                     mu_variant_t *out) {
    if (max_age_ms == 0.0) {
        mu_read_cache_misses++;
        return false;
    }

    for (size_t i = 0; i < MU_READ_CACHE_SLOTS; i++) {
        if (!mu_read_cache[i].valid)
            continue;

        if (!mu_nodeid_equal(node_id, &mu_read_cache[i].node_id))
            continue;

        opcua_datetime_t age_ticks = now - mu_read_cache[i].read_time;

        if (age_ticks < 0) {
            memcpy(out, &mu_read_cache[i].variant, sizeof(mu_variant_t));
            mu_read_cache_hits++;
            return true;
        }

        if (max_age_ms >= MU_MAXAGE_ALWAYS_CACHE_THRESHOLD_MS) {
            memcpy(out, &mu_read_cache[i].variant, sizeof(mu_variant_t));
            mu_read_cache_hits++;
            return true;
        }

        opcua_datetime_t max_age_ticks = (opcua_datetime_t)(max_age_ms * 10000.0);
        if (age_ticks <= max_age_ticks) {
            memcpy(out, &mu_read_cache[i].variant, sizeof(mu_variant_t));
            mu_read_cache_hits++;
            return true;
        }
    }

    mu_read_cache_misses++;
    return false;
}

void mu_read_cache_store(const mu_nodeid_t *node_id, const mu_variant_t *val, opcua_datetime_t read_time) {
    (void)node_id;
    memcpy(&mu_read_cache[0].node_id, node_id, sizeof(mu_nodeid_t));
    memcpy(&mu_read_cache[0].variant, val, sizeof(mu_variant_t));
    mu_read_cache[0].read_time = read_time;
    mu_read_cache[0].valid = true;
}

void mu_read_cache_get_stats(mu_read_cache_stats_t *out) {
    if (out) {
        out->hits = mu_read_cache_hits;
        out->misses = mu_read_cache_misses;
    }
}
