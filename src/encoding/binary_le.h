/* src/encoding/binary_le.h */
#ifndef MICRO_OPCUA_BINARY_LE_H
#define MICRO_OPCUA_BINARY_LE_H

#include "micro_opcua/opcua_types.h"

/* Shift-based little-endian helpers; no host-endian or alignment assumptions. */
static inline void mu_binary_le_put_u16(opcua_byte_t *dst, opcua_uint16_t value) {
    dst[0] = (opcua_byte_t)(value & 0xFFu);
    dst[1] = (opcua_byte_t)((value >> 8) & 0xFFu);
}

static inline void mu_binary_le_put_u32(opcua_byte_t *dst, opcua_uint32_t value) {
    dst[0] = (opcua_byte_t)(value & 0xFFu);
    dst[1] = (opcua_byte_t)((value >> 8) & 0xFFu);
    dst[2] = (opcua_byte_t)((value >> 16) & 0xFFu);
    dst[3] = (opcua_byte_t)((value >> 24) & 0xFFu);
}

static inline void mu_binary_le_put_u64(opcua_byte_t *dst, opcua_uint64_t value) {
    dst[0] = (opcua_byte_t)(value & 0xFFu);
    dst[1] = (opcua_byte_t)((value >> 8) & 0xFFu);
    dst[2] = (opcua_byte_t)((value >> 16) & 0xFFu);
    dst[3] = (opcua_byte_t)((value >> 24) & 0xFFu);
    dst[4] = (opcua_byte_t)((value >> 32) & 0xFFu);
    dst[5] = (opcua_byte_t)((value >> 40) & 0xFFu);
    dst[6] = (opcua_byte_t)((value >> 48) & 0xFFu);
    dst[7] = (opcua_byte_t)((value >> 56) & 0xFFu);
}

static inline opcua_uint16_t mu_binary_le_get_u16(const opcua_byte_t *src) {
    return (opcua_uint16_t)((opcua_uint16_t)src[0] |
                           ((opcua_uint16_t)src[1] << 8));
}

static inline opcua_uint32_t mu_binary_le_get_u32(const opcua_byte_t *src) {
    return (opcua_uint32_t)src[0] |
           ((opcua_uint32_t)src[1] << 8) |
           ((opcua_uint32_t)src[2] << 16) |
           ((opcua_uint32_t)src[3] << 24);
}

static inline opcua_uint64_t mu_binary_le_get_u64(const opcua_byte_t *src) {
    return (opcua_uint64_t)src[0] |
           ((opcua_uint64_t)src[1] << 8) |
           ((opcua_uint64_t)src[2] << 16) |
           ((opcua_uint64_t)src[3] << 24) |
           ((opcua_uint64_t)src[4] << 32) |
           ((opcua_uint64_t)src[5] << 40) |
           ((opcua_uint64_t)src[6] << 48) |
           ((opcua_uint64_t)src[7] << 56);
}

#endif /* MICRO_OPCUA_BINARY_LE_H */
