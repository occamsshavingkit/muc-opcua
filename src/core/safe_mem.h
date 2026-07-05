/* src/core/safe_mem.h
 * Bounds-checked memory operations for static analysis visibility.
 * Wraps memcpy with a capacity check so that Codacy/Semgrep/flawfinder
 * can verify safety without flagging every raw memcpy in protocol code. */

#ifndef MU_SAFE_MEM_H
#define MU_SAFE_MEM_H

#include "muc_opcua/types.h"
#include <stddef.h>
#include <string.h>

/* Checked memcpy: returns dst on success, NULL if n exceeds dst_size. */
static inline void *mu_checked_memcpy(void *dst, size_t dst_size, const void *src, size_t n) {
    if (n > dst_size) {
        return NULL;
    }
    return memcpy(dst, src, n);
}

/* Checked memcpy at offset: capacity is dst_cap, write starts at dst + offset.
 * Returns dst on success, NULL if offset + n exceeds dst_cap. */
static inline void *mu_checked_memcpy_off(void *dst, size_t dst_cap, size_t dst_offset, const void *src, size_t n) {
    if (dst_offset > dst_cap || n > dst_cap - dst_offset) {
        return NULL;
    }
    return memcpy((opcua_byte_t *)dst + dst_offset, src, n);
}

#endif /* MU_SAFE_MEM_H */
