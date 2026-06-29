/* tests/unit/test_traceability_docs.c */
#include "unity.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void setUp(void) {}
void tearDown(void) {}

static char files_to_sections_content[65536];
static char queries_content[65536];

static void read_file_content(const char *path, char *buffer, size_t max_len) {
    FILE *f = fopen(path, "r");
    if (!f) {
        buffer[0] = '\0';
        return;
    }
    size_t read_len = fread(buffer, 1, max_len - 1, f);
    buffer[read_len] = '\0';
    fclose(f);
}

static void check_file_traceability(const char *dir_path) {
    DIR *dir = opendir(dir_path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.')
            continue;

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(path, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                check_file_traceability(path);
            } else if (S_ISREG(st.st_mode)) {
                const char *ext = strrchr(entry->d_name, '.');
                if (ext && (strcmp(ext, ".c") == 0 || strcmp(ext, ".h") == 0)) {
                    /* Check if path is in files_to_sections.md */
                    /* Note: The paths in files-to-sections.md might be like `src/core/server.c`
                       but our dir_path is absolute or relative like `../src/...`
                       Let's just check if the basename is present. */
                    if (strstr(files_to_sections_content, entry->d_name) == NULL) {
                        char msg[1024];
                        snprintf(msg, sizeof(msg), "File %s missing from files-to-sections.md", entry->d_name);
                        TEST_FAIL_MESSAGE(msg);
                    }
                }
            }
        }
    }
    closedir(dir);
}

void test_traceability_completeness(void) {
    /* Read files-to-sections.md */
    char path[1024];
    snprintf(path, sizeof(path), "%s/docs/traceability/files-to-sections.md", PROJECT_ROOT_DIR);
    read_file_content(path, files_to_sections_content, sizeof(files_to_sections_content));
    TEST_ASSERT_MESSAGE(strlen(files_to_sections_content) > 0, "Could not read files-to-sections.md");

    snprintf(path, sizeof(path), "%s/src", PROJECT_ROOT_DIR);
    check_file_traceability(path);
    snprintf(path, sizeof(path), "%s/include", PROJECT_ROOT_DIR);
    check_file_traceability(path);
}

void test_mcp_provenance_coverage(void) {
    /* Read opcua-mcp-queries.md */
    char path[1024];
    snprintf(path, sizeof(path), "%s/docs/traceability/opcua-mcp-queries.md", PROJECT_ROOT_DIR);
    read_file_content(path, queries_content, sizeof(queries_content));
    TEST_ASSERT_MESSAGE(strlen(queries_content) > 0, "Could not read opcua-mcp-queries.md");

    /* Just verify it has the table header */
    TEST_ASSERT_NOT_NULL(strstr(queries_content, "Resulting Decision"));
}

void test_traceability_docs_do_not_keep_stale_aggregate_or_tbd_protocol_rows(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/docs/traceability/files-to-sections.md", PROJECT_ROOT_DIR);
    read_file_content(path, files_to_sections_content, sizeof(files_to_sections_content));
    TEST_ASSERT_MESSAGE(strlen(files_to_sections_content) > 0, "Could not read files-to-sections.md");

    /* OPC-10000-4 §7.22.4 is the current AggregateFilter section. */
    TEST_ASSERT_NULL_MESSAGE(strstr(files_to_sections_content, "Part 4 | 7.16"),
                             "AggregateFilter traceability must not cite stale OPC-10000-4 §7.16");
    TEST_ASSERT_NULL_MESSAGE(strstr(files_to_sections_content, "| TBD | TBD | TBD | TBD |"),
                             "Protocol traceability rows must have exact OPC UA references");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_traceability_completeness);
    RUN_TEST(test_mcp_provenance_coverage);
    RUN_TEST(test_traceability_docs_do_not_keep_stale_aggregate_or_tbd_protocol_rows);
    return UNITY_END();
}
