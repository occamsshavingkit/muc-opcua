/* tests/unit/test_conformance_docs.c */
#define _POSIX_C_SOURCE 200809L
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

void setUp(void) {}
void tearDown(void) {}

void test_conformance_claims_have_evidence(void) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "grep -rniE 'profile-compliant|CTT-verified' %s/docs | grep -v 'evidence'", PROJECT_ROOT_DIR);
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        TEST_IGNORE_MESSAGE("popen failed");
        return;
    }
    char buf[1024];
    size_t found = 0;
    while (fgets(buf, sizeof(buf), fp)) {
        printf("Found unsupported claim: %s", buf);
        found++;
    }
    pclose(fp);
    TEST_ASSERT_EQUAL_MESSAGE(0, found, "Found unsupported conformance claims without evidence");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_conformance_claims_have_evidence);
    return UNITY_END();
}
