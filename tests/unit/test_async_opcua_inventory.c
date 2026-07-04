/* tests/unit/test_async_opcua_inventory.c */
#define _POSIX_C_SOURCE 200809L
#include "unity.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

void setUp(void) {}
void tearDown(void) {}

void test_async_opcua_inventory_has_areas(void) {
    char path[1024];
    (void)snprintf(path, sizeof(path), "%s/docs/conformance/async-opcua-inventory.md", PROJECT_ROOT_DIR);
    FILE *fp = fopen(path, "r");

    if (!fp) {
        TEST_IGNORE_MESSAGE("async-opcua-inventory.md not found yet");
        return;
    }

    char buf[1024];
    bool has_devcontainer = false;
    bool has_codegen_tests = false;
    bool has_dotnet_tests = false;
    bool has_fuzz = false;
    bool has_schemas = false;
    bool has_tools = false;

    while (fgets(buf, sizeof(buf), fp)) {
        if (strstr(buf, ".devcontainer") != NULL) {
            has_devcontainer = true;
        }
        if (strstr(buf, "codegen-tests") != NULL) {
            has_codegen_tests = true;
        }
        if (strstr(buf, "dotnet-tests") != NULL) {
            has_dotnet_tests = true;
        }
        if (strstr(buf, "fuzz") != NULL) {
            has_fuzz = true;
        }
        if (strstr(buf, "schemas") != NULL) {
            has_schemas = true;
        }
        if (strstr(buf, "tools") != NULL) {
            has_tools = true;
        }
    }
    (void)fclose(fp);

    TEST_ASSERT_TRUE_MESSAGE(has_devcontainer, "Inventory missing .devcontainer");
    TEST_ASSERT_TRUE_MESSAGE(has_codegen_tests, "Inventory missing codegen-tests");
    TEST_ASSERT_TRUE_MESSAGE(has_dotnet_tests, "Inventory missing dotnet-tests");
    TEST_ASSERT_TRUE_MESSAGE(has_fuzz, "Inventory missing fuzz");
    TEST_ASSERT_TRUE_MESSAGE(has_schemas, "Inventory missing schemas");
    TEST_ASSERT_TRUE_MESSAGE(has_tools, "Inventory missing tools");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_async_opcua_inventory_has_areas);
    return UNITY_END();
}
