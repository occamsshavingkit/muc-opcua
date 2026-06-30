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
static char sections_to_files_content[65536];
static char optimize_hot_paths_content[131072];
static char queries_content[65536];

typedef struct {
    const char *part;
    const char *section;
} opc_section_ref_t;

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

static const char *find_backticked_mapping(const char *content, const char *file_path) {
    char needle[256];
    const char *basename = strrchr(file_path, '/');
    basename = basename ? basename + 1 : file_path;

    snprintf(needle, sizeof(needle), "`%s`", file_path);
    const char *mapping = strstr(content, needle);
    if (mapping)
        return mapping;

    snprintf(needle, sizeof(needle), "`%s`", basename);
    return strstr(content, needle);
}

static const char *line_start_for(const char *content, const char *position) {
    while (position > content && position[-1] != '\n')
        --position;
    return position;
}

static const char *line_end_for(const char *position) {
    const char *line_end = strchr(position, '\n');
    return line_end ? line_end : position + strlen(position);
}

static const char *find_char_between(const char *start, const char *end, char needle) {
    while (start < end) {
        if (*start == needle)
            return start;
        ++start;
    }

    return NULL;
}

static int cell_matches(const char *start, const char *end, const char *expected) {
    size_t expected_len = strlen(expected);

    while (start < end && (*start == ' ' || *start == '\t'))
        ++start;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
        --end;

    return (size_t)(end - start) == expected_len && memcmp(start, expected, expected_len) == 0;
}

static const char *mapping_block_end(const char *line_start) {
    const char *line_end = strchr(line_start, '\n');
    if (!line_end)
        return line_start + strlen(line_start);

    const char *cursor = line_end + 1;
    int continuation_lines = 0;
    while (*cursor && *cursor != '|' && continuation_lines < 2) {
        const char *next_end = strchr(cursor, '\n');
        line_end = next_end ? next_end : cursor + strlen(cursor);
        if (!next_end)
            break;
        cursor = next_end + 1;
        ++continuation_lines;
    }

    return line_end;
}

static const char *find_section_mapping_row(const char *content, const char *part, const char *section) {
    const char *line = content;

    while (*line) {
        const char *line_end = line_end_for(line);
        if (*line == '|') {
            const char *part_start = line + 1;
            const char *part_end = find_char_between(part_start, line_end, '|');
            if (part_end) {
                const char *section_start = part_end + 1;
                const char *section_end = find_char_between(section_start, line_end, '|');
                if (section_end && cell_matches(part_start, part_end, part) &&
                    cell_matches(section_start, section_end, section))
                    return line;
            }
        }

        if (*line_end == '\0')
            break;
        line = line_end + 1;
    }

    return NULL;
}

static int contains_between(const char *start, const char *end, const char *needle) {
    size_t needle_len = strlen(needle);
    const char *cursor;

    for (cursor = start; cursor + needle_len <= end; ++cursor) {
        if (memcmp(cursor, needle, needle_len) == 0)
            return 1;
    }

    return 0;
}

static int table_cell_bounds(const char *row_start, size_t cell_index, const char **cell_start, const char **cell_end) {
    const char *line_end = line_end_for(row_start);
    const char *cursor = row_start;
    size_t current_cell = 0;

    if (*cursor != '|')
        return 0;

    ++cursor;
    while (cursor < line_end) {
        const char *next_pipe = find_char_between(cursor, line_end, '|');
        if (!next_pipe)
            return 0;

        if (current_cell == cell_index) {
            *cell_start = cursor;
            *cell_end = next_pipe;
            return 1;
        }

        cursor = next_pipe + 1;
        ++current_cell;
    }

    return 0;
}

static const char *find_markdown_table_row_containing(const char *content, const char *needle) {
    const char *line = content;

    while (*line) {
        const char *line_end = line_end_for(line);
        if (*line == '|' && contains_between(line, line_end, needle))
            return line;

        if (*line_end == '\0')
            break;
        line = line_end + 1;
    }

    return NULL;
}

static int feature_section_cell_has_exact_section(const char *row_start, const char *section_anchor) {
    const char *section_start;
    const char *section_end;
    size_t section_anchor_len = strlen(section_anchor);

    if (!table_cell_bounds(row_start, 1, &section_start, &section_end))
        return 0;

    while (section_start < section_end && (*section_start == ' ' || *section_start == '\t'))
        ++section_start;

    if ((size_t)(section_end - section_start) < section_anchor_len)
        return 0;

    if (memcmp(section_start, section_anchor, section_anchor_len) != 0)
        return 0;

    return section_start + section_anchor_len == section_end || section_start[section_anchor_len] == ' ' ||
           section_start[section_anchor_len] == '\t';
}

static const char *find_feature_mapping_row_for_section(const char *content, const char *section_anchor) {
    const char *line = content;

    while (*line) {
        const char *line_end = line_end_for(line);
        if (*line == '|' && feature_section_cell_has_exact_section(line, section_anchor))
            return line;

        if (*line_end == '\0')
            break;
        line = line_end + 1;
    }

    return NULL;
}

static int section_mapping_row_has_file_path(const char *row_start) {
    const char *line_end = line_end_for(row_start);
    const char *cursor = row_start;
    const char *files_start = NULL;
    const char *files_end = line_end;
    int pipe_count = 0;

    while (cursor < line_end) {
        if (*cursor == '|') {
            ++pipe_count;
            if (pipe_count == 4) {
                files_start = cursor + 1;
            } else if (pipe_count == 5) {
                files_end = cursor;
                break;
            }
        }
        ++cursor;
    }

    if (!files_start)
        return 0;

    return contains_between(files_start, files_end, "`src/") || contains_between(files_start, files_end, "`include/") ||
           contains_between(files_start, files_end, "`tests/") || contains_between(files_start, files_end, "`docs/") ||
           contains_between(files_start, files_end, "`specs/");
}

static int feature_mapping_row_has_source_file(const char *row_start) {
    const char *source_start;
    const char *source_end;

    if (!table_cell_bounds(row_start, 2, &source_start, &source_end))
        return 0;

    return contains_between(source_start, source_end, "`src/") ||
           contains_between(source_start, source_end, "`include/");
}

static int feature_mapping_row_has_test_or_doc_evidence(const char *row_start) {
    const char *evidence_start;
    const char *evidence_end;

    if (!table_cell_bounds(row_start, 3, &evidence_start, &evidence_end))
        return 0;

    if (contains_between(evidence_start, evidence_end, "Placeholder") ||
        contains_between(evidence_start, evidence_end, "placeholder") ||
        contains_between(evidence_start, evidence_end, "TBD"))
        return 0;

    return contains_between(evidence_start, evidence_end, "`tests/") ||
           contains_between(evidence_start, evidence_end, "`docs/") ||
           contains_between(evidence_start, evidence_end, "`specs/") ||
           contains_between(evidence_start, evidence_end, "`scripts/");
}

static void append_traceability_failure(char *buffer, size_t buffer_len, const char *file_path, const char *reason) {
    size_t used = strlen(buffer);
    int written;
    if (used >= buffer_len - 1)
        return;

    written = snprintf(buffer + used, buffer_len - used, "%s%s: %s", used ? "\n" : "", file_path, reason);
    (void)written;
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

void test_traceability_docs_changed_protocol_files_have_opc_section_mappings(void) {
    static const char *const changed_protocol_files[] = {
        "src/core/service_dispatch.c",    "src/core/message_chunk.c",     "src/core/server.c",
        "src/core/tcp_connection.c",      "src/encoding/binary_reader.c", "src/encoding/binary_extension_object.c",
        "src/encoding/binary_query.c",    "src/services/discovery.c",     "src/services/history.c",
        "src/services/node_management.c", "src/services/query.c",         "src/services/session.c",
        "src/services/subscription.c",    "include/micro_opcua/status.h",
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/files-to-sections.md", PROJECT_ROOT_DIR);
    read_file_content(path, files_to_sections_content, sizeof(files_to_sections_content));
    TEST_ASSERT_MESSAGE(strlen(files_to_sections_content) > 0, "Could not read files-to-sections.md");

    for (i = 0; i < sizeof(changed_protocol_files) / sizeof(changed_protocol_files[0]); ++i) {
        const char *mapping = find_backticked_mapping(files_to_sections_content, changed_protocol_files[i]);
        const char *mapping_start;
        const char *mapping_end;

        if (!mapping) {
            append_traceability_failure(failures, sizeof(failures), changed_protocol_files[i],
                                        "missing from files-to-sections.md");
            continue;
        }

        mapping_start = line_start_for(files_to_sections_content, mapping);
        mapping_end = mapping_block_end(mapping_start);
        if (!contains_between(mapping_start, mapping_end, "OPC-10000-")) {
            append_traceability_failure(failures, sizeof(failures), changed_protocol_files[i],
                                        "mapping has no OPC-10000- citation");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_traceability_docs_cited_opc_sections_have_file_mappings(void) {
    static const opc_section_ref_t cited_sections[] = {
        {"4", "5.5.2"},     {"4", "5.5.4"},    {"4", "5.6.2"},     {"4", "5.7.2"},    {"4", "5.7.3"},
        {"4", "5.13.2.4"},  {"4", "5.13.3.4"}, {"4", "5.14"},      {"4", "7.22.1"},   {"4", "7.22.4"},
        {"4", "7.38.2"},    {"6", "5.2"},      {"6", "5.2.2.15"},  {"6", "5.2.5"},    {"6", "6.7.2"},
        {"6", "7.1.2.2"},   {"6", "7.1.2.3"},  {"6", "7.1.2.4"},   {"7", "4.2"},      {"7", "4.3"},
        {"13", "4.2.2.4"},  {"13", "4.2.2.9"}, {"13", "4.2.2.10"}, {"13", "5.4.3.5"}, {"13", "5.4.3.10"},
        {"13", "5.4.3.11"},
    };
    char path[1024];
    char failures[8192];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/sections-to-files.md", PROJECT_ROOT_DIR);
    read_file_content(path, sections_to_files_content, sizeof(sections_to_files_content));
    TEST_ASSERT_MESSAGE(strlen(sections_to_files_content) > 0, "Could not read sections-to-files.md");

    for (i = 0; i < sizeof(cited_sections) / sizeof(cited_sections[0]); ++i) {
        const char *row =
            find_section_mapping_row(sections_to_files_content, cited_sections[i].part, cited_sections[i].section);
        char label[64];

        snprintf(label, sizeof(label), "OPC-10000-%s section %s", cited_sections[i].part, cited_sections[i].section);
        if (!row) {
            append_traceability_failure(failures, sizeof(failures), label, "missing from sections-to-files.md");
            continue;
        }

        if (!section_mapping_row_has_file_path(row)) {
            append_traceability_failure(failures, sizeof(failures), label, "has no source/test/doc file mapping");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_optimize_hot_paths_binary_traceability_rows_have_source_and_evidence_mappings(void) {
    static const char *const binary_section_anchors[] = {
        "OPC-10000-6 section 5.2.1 Binary DataEncoding general rules",
        "OPC-10000-6 section 5.2.2.4 String",
        "OPC-10000-6 section 5.2.2.7 ByteString",
        "OPC-10000-6 section 5.2.2.9 NodeId",
        "OPC-10000-6 section 5.2.5 Arrays",
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/022-optimize-hot-paths.md", PROJECT_ROOT_DIR);
    read_file_content(path, optimize_hot_paths_content, sizeof(optimize_hot_paths_content));
    TEST_ASSERT_MESSAGE(strlen(optimize_hot_paths_content) > 0, "Could not read 022-optimize-hot-paths.md");

    for (i = 0; i < sizeof(binary_section_anchors) / sizeof(binary_section_anchors[0]); ++i) {
        const char *row = find_markdown_table_row_containing(optimize_hot_paths_content, binary_section_anchors[i]);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), binary_section_anchors[i],
                                        "missing from 022-optimize-hot-paths.md OPC UA Section Mapping table");
            continue;
        }

        if (!feature_mapping_row_has_source_file(row)) {
            append_traceability_failure(failures, sizeof(failures), binary_section_anchors[i],
                                        "has no source file mapping");
        }

        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), binary_section_anchors[i],
                                        "has no concrete test/doc evidence mapping");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_optimize_hot_paths_transport_framing_traceability_rows_have_source_and_evidence_mappings(void) {
    static const char *const transport_framing_section_anchors[] = {
        "OPC-10000-6 section 6.7.2", "OPC-10000-6 section 6.7.2.2", "OPC-10000-6 section 6.7.2.4",
        "OPC-10000-6 section 6.7.4", "OPC-10000-6 section 7.1.2.2", "OPC-10000-6 section 7.2",
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/022-optimize-hot-paths.md", PROJECT_ROOT_DIR);
    read_file_content(path, optimize_hot_paths_content, sizeof(optimize_hot_paths_content));
    TEST_ASSERT_MESSAGE(strlen(optimize_hot_paths_content) > 0, "Could not read 022-optimize-hot-paths.md");

    for (i = 0; i < sizeof(transport_framing_section_anchors) / sizeof(transport_framing_section_anchors[0]); ++i) {
        const char *row =
            find_feature_mapping_row_for_section(optimize_hot_paths_content, transport_framing_section_anchors[i]);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), transport_framing_section_anchors[i],
                                        "missing from 022-optimize-hot-paths.md OPC UA Section Mapping table");
            continue;
        }

        if (!feature_mapping_row_has_source_file(row)) {
            append_traceability_failure(failures, sizeof(failures), transport_framing_section_anchors[i],
                                        "has no source file mapping");
        }

        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), transport_framing_section_anchors[i],
                                        "has no concrete test/doc evidence mapping");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_optimize_hot_paths_service_statuscode_traceability_rows_have_source_and_evidence_mappings(void) {
    static const char *const service_statuscode_section_anchors[] = {
        "OPC-10000-4 section 5.11.2.4",
        "OPC-10000-4 section 5.11.4.4",
        "OPC-10000-4 section 7.38.2",
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/022-optimize-hot-paths.md", PROJECT_ROOT_DIR);
    read_file_content(path, optimize_hot_paths_content, sizeof(optimize_hot_paths_content));
    TEST_ASSERT_MESSAGE(strlen(optimize_hot_paths_content) > 0, "Could not read 022-optimize-hot-paths.md");

    for (i = 0; i < sizeof(service_statuscode_section_anchors) / sizeof(service_statuscode_section_anchors[0]); ++i) {
        const char *row =
            find_feature_mapping_row_for_section(optimize_hot_paths_content, service_statuscode_section_anchors[i]);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), service_statuscode_section_anchors[i],
                                        "missing from 022-optimize-hot-paths.md OPC UA Section Mapping table");
            continue;
        }

        if (!feature_mapping_row_has_source_file(row)) {
            append_traceability_failure(failures, sizeof(failures), service_statuscode_section_anchors[i],
                                        "has no source file mapping");
        }

        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), service_statuscode_section_anchors[i],
                                        "has no concrete test/doc evidence mapping");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_optimize_hot_paths_write_audit_and_conformance_traceability_rows_have_source_and_evidence_mappings(void) {
    static const char *const write_audit_and_conformance_section_anchors[] = {
        "OPC-10000-4 section 6.5.8",
        "OPC-10000-7 section 4.2",
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/022-optimize-hot-paths.md", PROJECT_ROOT_DIR);
    read_file_content(path, optimize_hot_paths_content, sizeof(optimize_hot_paths_content));
    TEST_ASSERT_MESSAGE(strlen(optimize_hot_paths_content) > 0, "Could not read 022-optimize-hot-paths.md");

    for (i = 0; i < sizeof(write_audit_and_conformance_section_anchors) /
                        sizeof(write_audit_and_conformance_section_anchors[0]);
         ++i) {
        const char *row = find_feature_mapping_row_for_section(optimize_hot_paths_content,
                                                               write_audit_and_conformance_section_anchors[i]);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), write_audit_and_conformance_section_anchors[i],
                                        "missing from 022-optimize-hot-paths.md OPC UA Section Mapping table");
            continue;
        }

        if (!feature_mapping_row_has_source_file(row)) {
            append_traceability_failure(failures, sizeof(failures), write_audit_and_conformance_section_anchors[i],
                                        "has no source file mapping");
        }

        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), write_audit_and_conformance_section_anchors[i],
                                        "has no concrete test/doc evidence mapping");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_traceability_completeness);
    RUN_TEST(test_mcp_provenance_coverage);
    RUN_TEST(test_traceability_docs_do_not_keep_stale_aggregate_or_tbd_protocol_rows);
    RUN_TEST(test_traceability_docs_changed_protocol_files_have_opc_section_mappings);
    RUN_TEST(test_traceability_docs_cited_opc_sections_have_file_mappings);
    RUN_TEST(test_optimize_hot_paths_binary_traceability_rows_have_source_and_evidence_mappings);
    RUN_TEST(test_optimize_hot_paths_transport_framing_traceability_rows_have_source_and_evidence_mappings);
    RUN_TEST(test_optimize_hot_paths_service_statuscode_traceability_rows_have_source_and_evidence_mappings);
    RUN_TEST(test_optimize_hot_paths_write_audit_and_conformance_traceability_rows_have_source_and_evidence_mappings);
    return UNITY_END();
}
