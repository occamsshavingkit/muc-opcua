/* tests/unit/test_size_report.c */
#define _POSIX_C_SOURCE 200809L
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

void setUp(void) {}
void tearDown(void) {}

void test_size_ledger_contains_required_fields(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/docs/size/feature-size-ledger.md", PROJECT_ROOT_DIR);
    FILE *fp = fopen(path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(fp, "Could not open feature-size-ledger.md");

    char buf[1024];
    bool has_flash = false;
    bool has_ram = false;
    bool has_heap = false;
    
    while (fgets(buf, sizeof(buf), fp)) {
        if (strstr(buf, "Flash") != NULL) has_flash = true;
        if (strstr(buf, "RAM") != NULL) has_ram = true;
        if (strstr(buf, "Heap") != NULL || strstr(buf, "Dynamic") != NULL) has_heap = true;
    }
    fclose(fp);

    TEST_ASSERT_TRUE_MESSAGE(has_flash, "feature-size-ledger.md does not contain Flash field");
    TEST_ASSERT_TRUE_MESSAGE(has_ram, "feature-size-ledger.md does not contain RAM field");
    TEST_ASSERT_TRUE_MESSAGE(has_heap, "feature-size-ledger.md does not contain Heap field");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_size_ledger_contains_required_fields);
    return UNITY_END();
}
