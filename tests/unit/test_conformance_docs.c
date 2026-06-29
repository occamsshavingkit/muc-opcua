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
    snprintf(cmd, sizeof(cmd),
             "grep -rniE 'profile-compliant|CTT-verified' %s/docs "
             "| grep -viE 'no |not |without|until|unless|before any|forbid|forbidding|reject|evidence|profile-targeting'",
             PROJECT_ROOT_DIR);
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

void test_docs_and_specs_do_not_claim_profile_compliance_without_ctt_evidence(void) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "grep -rniE 'profile-compliant|CTT-verified' %s/docs %s/specs "
             "| grep -viE 'no (conformance or )?(profile-compliant|CTT-verified)|not (profile-compliant|CTT-verified|claim)|without (actual )?(CTT|external|matching)|until|unless|before any|forbidding|reject unsupported|remove or qualify unsupported|does not claim|do not claim|must not claim|one of experimental|rg -n|accurately'",
             PROJECT_ROOT_DIR, PROJECT_ROOT_DIR);
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
    TEST_ASSERT_EQUAL_MESSAGE(0, found, "Found profile-compliance claims without CTT evidence");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_conformance_claims_have_evidence);
    RUN_TEST(test_docs_and_specs_do_not_claim_profile_compliance_without_ctt_evidence);
    return UNITY_END();
}
