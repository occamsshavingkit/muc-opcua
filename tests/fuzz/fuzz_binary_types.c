/* tests/fuzz/fuzz_binary_types.c */
#include <stdint.h>
#include <stddef.h>
#include "micro_opcua/micro_opcua.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    /* To be implemented in T148 */
    (void)data;
    (void)size;
    return 0;
}
