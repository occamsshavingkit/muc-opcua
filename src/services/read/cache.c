/* src/services/read/cache.c
 *
 * Optional Read value cache (MUC_OPCUA_READ_CACHE). Implements the maxAge
 * optimization of OPC-10000-4 5.11.2: return a cached value when it is younger
 * than the requested maxAge instead of re-reading the source.
 *
 * maxAge arrives as a Duration (Double, milliseconds) on the wire, but the spec
 * only needs millisecond granularity and permits a "best effort" answer, so the
 * value is converted to integer 100ns ticks exactly once here and every per-entry
 * comparison is integer. That keeps double-precision soft-float off the hot path
 * (a single decode conversion is all that remains) on FPU-less targets. */
#include "common.h"
#include <stddef.h>
#include <string.h>

/* OPC-10000-4 5.11.2.2: "If maxAge is set to the max Int32 value or greater, the
   Server shall attempt to get a cached value." */
#define MU_MAXAGE_ALWAYS_CACHE_MS 2147483647.0

opcua_boolean_t mu_read_cache_lookup(mu_read_cache_t *cache, const mu_nodeid_t *node_id, opcua_double_t max_age_ms,
                                     opcua_datetime_t now, mu_variant_t *out) {
    if (!cache)
        return false;
    /* maxAge == 0 (or a nonsensical negative) directs a fresh read (5.11.2.2). */
    if (max_age_ms <= 0.0) {
        cache->misses++;
        return false;
    }

    /* Convert Duration ms -> integer 100ns ticks once. >= Int32 max ms means
       "always use the cached value" regardless of age. */
    opcua_boolean_t always_cache = (max_age_ms >= MU_MAXAGE_ALWAYS_CACHE_MS);
    opcua_datetime_t max_age_ticks = always_cache ? 0 : (opcua_datetime_t)max_age_ms * 10000;

    for (size_t i = 0; i < MU_READ_CACHE_SLOTS; i++) {
        if (!cache->entries[i].valid)
            continue;
        if (!mu_nodeid_equal(node_id, &cache->entries[i].node_id))
            continue;

        /* age_ticks < 0 means the clock moved backwards; treat the entry as
           still current rather than forcing a re-read. */
        opcua_datetime_t age_ticks = now - cache->entries[i].read_time;
        if (age_ticks < 0 || always_cache || age_ticks <= max_age_ticks) {
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
