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
static char feature_023_content[65536];
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

static const char *find_feature_mapping_row_for_section_with_text(const char *content, const char *section_anchor,
                                                                  const char *required_text) {
    const char *line = content;

    while (*line) {
        const char *line_end = line_end_for(line);
        if (*line == '|' && feature_section_cell_has_exact_section(line, section_anchor) &&
            contains_between(line, line_end, required_text))
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

static int feature_mapping_row_cell_has_text(const char *row_start, size_t cell_index, const char *text) {
    const char *cell_start;
    const char *cell_end;

    if (!table_cell_bounds(row_start, cell_index, &cell_start, &cell_end))
        return 0;

    return contains_between(cell_start, cell_end, text);
}

static int feature_mapping_row_cell_has_path(const char *row_start, size_t cell_index, const char *path) {
    return feature_mapping_row_cell_has_text(row_start, cell_index, path);
}

static int markdown_table_row_has_anchor(const char *row_start, const char *anchor) {
    return contains_between(row_start, line_end_for(row_start), anchor);
}

static const char *markdown_list_item_end(const char *item_start) {
    const char *line_end = line_end_for(item_start);
    const char *cursor = line_end;

    if (*cursor != '\n')
        return line_end;

    cursor++;
    while (*cursor == ' ' || *cursor == '\t') {
        line_end = line_end_for(cursor);
        if (*line_end == '\0')
            return line_end;
        cursor = line_end + 1;
    }

    return line_end;
}

static int feature_mapping_row_has_concrete_source_or_doc_path(const char *row_start) {
    const char *source_start;
    const char *source_end;

    if (!table_cell_bounds(row_start, 2, &source_start, &source_end))
        return 0;

    if (contains_between(source_start, source_end, "Placeholder") ||
        contains_between(source_start, source_end, "placeholder") || contains_between(source_start, source_end, "TBD"))
        return 0;

    return contains_between(source_start, source_end, "`src/") ||
           contains_between(source_start, source_end, "`include/") ||
           contains_between(source_start, source_end, "`tests/") ||
           contains_between(source_start, source_end, "`docs/") ||
           contains_between(source_start, source_end, "`README.md`") ||
           contains_between(source_start, source_end, "`Makefile`");
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

void test_023_feature_traceability_pubsub_rows_have_sections_sources_and_evidence(void) {
    static const char *const section_anchors[] = {
        "OPC-10000-14 section 7.2.4.4.2", "OPC-10000-14 section 7.2.4.5.2", "OPC-10000-14 section 7.2.4.5.5",
        "OPC-10000-6 section 5.2.2.16",   "OPC-10000-4 section 7.38.2",
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    for (i = 0; i < sizeof(section_anchors) / sizeof(section_anchors[0]); ++i) {
        const char *row = find_feature_mapping_row_for_section(feature_023_content, section_anchors[i]);
        if (!row) {
            append_traceability_failure(failures, sizeof(failures), section_anchors[i],
                                        "missing from 023 feature traceability table");
            continue;
        }
        if (!feature_mapping_row_has_source_file(row)) {
            append_traceability_failure(failures, sizeof(failures), section_anchors[i], "has no source file mapping");
        }
        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), section_anchors[i],
                                        "has no concrete test/doc evidence mapping");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_023_feature_traceability_documentation_and_conformance_check_rows_have_sources_and_evidence(void) {
    static const struct {
        const char *section_anchor;
        const char *required_source_path;
        const char *required_evidence_path;
        const char *required_secondary_evidence_path;
    } documentation_check_rows[] = {
        {"OPC-10000-7 section 4.2", "`docs/conformance/services.md`", "`tests/unit/test_conformance_docs.c`",
         "`docs/traceability/conformance-claims.md`"},
        {"OPC-10000-7 section 4.3", "`docs/conformance/status.md`", "`tests/unit/test_conformance_docs.c`",
         "`README.md`"},
        {"OPC-10000-4 section 7.38.2", "`include/micro_opcua/status.h`", "`tests/unit/test_conformance_docs.c`",
         "`docs/conformance/status.md`"},
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    for (i = 0; i < sizeof(documentation_check_rows) / sizeof(documentation_check_rows[0]); ++i) {
        const char *row =
            find_feature_mapping_row_for_section(feature_023_content, documentation_check_rows[i].section_anchor);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), documentation_check_rows[i].section_anchor,
                                        "missing from 023 feature traceability table");
            continue;
        }

        if (!feature_mapping_row_has_concrete_source_or_doc_path(row)) {
            append_traceability_failure(failures, sizeof(failures), documentation_check_rows[i].section_anchor,
                                        "has no concrete source/doc path mapping");
        }
        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), documentation_check_rows[i].section_anchor,
                                        "has no concrete test/doc evidence mapping");
        }
        if (!feature_mapping_row_cell_has_path(row, 2, documentation_check_rows[i].required_source_path)) {
            append_traceability_failure(failures, sizeof(failures), documentation_check_rows[i].section_anchor,
                                        "missing required source/doc path");
        }
        if (!feature_mapping_row_cell_has_path(row, 3, documentation_check_rows[i].required_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), documentation_check_rows[i].section_anchor,
                                        "missing required conformance-check evidence path");
        }
        if (!feature_mapping_row_cell_has_path(row, 3, documentation_check_rows[i].required_secondary_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), documentation_check_rows[i].section_anchor,
                                        "missing required documentation evidence path");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_023_feature_traceability_lists_negative_path_evidence(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "Bad_MonitoredItemIdInvalid"),
                                 "Missing invalid MonitoredItemId evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "Bad_SubscriptionIdInvalid"),
                                 "Missing invalid SubscriptionId evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "Bad_SequenceNumberUnknown"),
                                 "Missing Publish acknowledgement evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "Bad_MessageNotAvailable"),
                                 "Missing Republish invalid sequence evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "Bad_MonitoredItemFilterUnsupported"),
                                 "Missing DataChange/Aggregate filter evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "`tests/unit/test_subscriptions_errors.c`"),
                                 "Missing subscription error unit test evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "`tests/integration/test_subscriptions.c`"),
                                 "Missing subscription integration test evidence");
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(feature_023_content, "`tests/unit/test_aggregate.c`"),
                                 "Missing aggregate unit test evidence");
}

void test_023_feature_traceability_invalid_id_rows_have_opc_refs_statuses_and_evidence(void) {
    static const struct {
        const char *label;
        const char *section_anchor;
        const char *status_code;
        const char *required_unit_evidence_path;
        const char *required_related_evidence_path;
    } invalid_id_rows[] = {
        {"ModifyMonitoredItems invalid MonitoredItemId", "OPC-10000-4 section 5.13.3", "Bad_MonitoredItemIdInvalid",
         "`tests/unit/test_subscriptions_errors.c`", "`tests/unit/test_traceability_docs.c`"},
        {"DeleteMonitoredItems invalid MonitoredItemId", "OPC-10000-4 section 5.13.6", "Bad_MonitoredItemIdInvalid",
         "`tests/unit/test_subscriptions_errors.c`", "`tests/integration/test_subscriptions.c`"},
        {"ModifySubscription invalid SubscriptionId", "OPC-10000-4 section 5.14.3", "Bad_SubscriptionIdInvalid",
         "`tests/unit/test_subscriptions_errors.c`", "`tests/integration/test_subscriptions.c`"},
        {"DeleteSubscriptions invalid SubscriptionId", "OPC-10000-4 section 5.14.8", "Bad_SubscriptionIdInvalid",
         "`tests/unit/test_subscriptions_errors.c`", "`tests/integration/test_subscriptions.c`"},
    };
    char path[1024];
    char failures[4096];
    const char *negative_path_row;
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    negative_path_row = find_markdown_table_row_containing(feature_023_content, "Negative-path evidence");
    if (!negative_path_row) {
        append_traceability_failure(failures, sizeof(failures), "US4 invalid ID evidence",
                                    "missing Negative-path evidence lane");
    } else {
        if (!markdown_table_row_has_anchor(negative_path_row, "OPC-10000-4 §7.38.2")) {
            append_traceability_failure(failures, sizeof(failures), "US4 invalid ID evidence",
                                        "negative-path evidence lane is missing OPC-10000-4 §7.38.2");
        }
        if (!markdown_table_row_has_anchor(negative_path_row, "`tests/unit/test_subscriptions_errors.c`")) {
            append_traceability_failure(failures, sizeof(failures), "US4 invalid ID evidence",
                                        "negative-path evidence lane is missing subscription error unit evidence");
        }
        if (!markdown_table_row_has_anchor(negative_path_row, "`tests/integration/test_subscriptions.c`")) {
            append_traceability_failure(failures, sizeof(failures), "US4 invalid ID evidence",
                                        "negative-path evidence lane is missing subscription integration evidence");
        }
    }

    for (i = 0; i < sizeof(invalid_id_rows) / sizeof(invalid_id_rows[0]); ++i) {
        const char *row = find_feature_mapping_row_for_section(feature_023_content, invalid_id_rows[i].section_anchor);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), invalid_id_rows[i].label,
                                        "missing OPC UA Section Mapping row");
            continue;
        }

        if (!feature_mapping_row_cell_has_text(row, 2, "`src/services/subscription.c`")) {
            append_traceability_failure(failures, sizeof(failures), invalid_id_rows[i].label,
                                        "missing subscription service source path");
        }
        if (!feature_mapping_row_cell_has_text(row, 3, invalid_id_rows[i].required_unit_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), invalid_id_rows[i].label,
                                        "missing subscription error unit evidence path");
        }
        if (!feature_mapping_row_cell_has_text(row, 3, invalid_id_rows[i].required_related_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), invalid_id_rows[i].label,
                                        "missing related traceability or integration evidence path");
        }
        if (!feature_mapping_row_cell_has_text(row, 4, invalid_id_rows[i].status_code)) {
            append_traceability_failure(failures, sizeof(failures), invalid_id_rows[i].label,
                                        "missing required invalid ID StatusCode in row notes");
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_023_feature_traceability_publish_republish_rows_have_opc_refs_statuses_and_evidence(void) {
    static const struct {
        const char *label;
        const char *section_anchor;
        const char *status_code;
        const char *required_evidence_path;
    } publish_republish_rows[] = {
        {"Publish acknowledgement invalid sequence", "OPC-10000-4 section 5.14.5.4", "Bad_SequenceNumberUnknown",
         "`tests/integration/test_subscriptions.c`"},
        {"Republish unavailable sequence", "OPC-10000-4 section 5.14.6.3", "Bad_MessageNotAvailable",
         "`tests/integration/test_subscriptions.c`"},
    };
    char path[1024];
    char failures[4096];
    const char *negative_path_row;
    const char *negative_path_section;
    const char *negative_path_section_end;
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    negative_path_row = find_markdown_table_row_containing(feature_023_content, "Negative-path evidence");
    if (!negative_path_row) {
        append_traceability_failure(failures, sizeof(failures), "US4 publish/republish evidence",
                                    "missing Negative-path evidence lane");
    } else {
        if (!markdown_table_row_has_anchor(negative_path_row, "OPC-10000-4 §7.38.2")) {
            append_traceability_failure(failures, sizeof(failures), "US4 publish/republish evidence",
                                        "negative-path evidence lane is missing OPC-10000-4 §7.38.2");
        }
        if (!markdown_table_row_has_anchor(negative_path_row, "`tests/integration/test_subscriptions.c`")) {
            append_traceability_failure(failures, sizeof(failures), "US4 publish/republish evidence",
                                        "negative-path evidence lane is missing subscription integration evidence");
        }
    }

    negative_path_section = strstr(feature_023_content, "## Negative-Path Evidence");
    negative_path_section_end = NULL;
    if (!negative_path_section) {
        append_traceability_failure(failures, sizeof(failures), "US4 publish/republish evidence",
                                    "missing Negative-Path Evidence section");
    } else {
        negative_path_section_end = strstr(negative_path_section + strlen("## Negative-Path Evidence"), "\n## ");
    }

    for (i = 0; i < sizeof(publish_republish_rows) / sizeof(publish_republish_rows[0]); ++i) {
        const char *row =
            find_feature_mapping_row_for_section(feature_023_content, publish_republish_rows[i].section_anchor);
        const char *evidence_status;

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), publish_republish_rows[i].label,
                                        "missing OPC UA Section Mapping row");
            continue;
        }

        if (!feature_mapping_row_cell_has_text(row, 2, "`src/services/subscription.c`")) {
            append_traceability_failure(failures, sizeof(failures), publish_republish_rows[i].label,
                                        "missing subscription service source path");
        }
        if (!feature_mapping_row_cell_has_text(row, 3, publish_republish_rows[i].required_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), publish_republish_rows[i].label,
                                        "missing subscription integration evidence path");
        }
        if (!feature_mapping_row_cell_has_text(row, 4, publish_republish_rows[i].status_code)) {
            append_traceability_failure(failures, sizeof(failures), publish_republish_rows[i].label,
                                        "missing required publish/republish StatusCode in row notes");
        }

        if (!negative_path_section)
            continue;

        evidence_status = strstr(negative_path_section, publish_republish_rows[i].status_code);
        if (evidence_status && negative_path_section_end && evidence_status >= negative_path_section_end)
            evidence_status = NULL;

        if (!evidence_status) {
            append_traceability_failure(failures, sizeof(failures), publish_republish_rows[i].label,
                                        "missing StatusCode entry in Negative-Path Evidence section");
        } else {
            const char *item_start = line_start_for(feature_023_content, evidence_status);
            const char *item_end = markdown_list_item_end(item_start);

            if (!contains_between(item_start, item_end, publish_republish_rows[i].required_evidence_path)) {
                append_traceability_failure(failures, sizeof(failures), publish_republish_rows[i].label,
                                            "negative-path StatusCode evidence is missing integration path");
            }
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_023_feature_traceability_filter_rows_have_opc_refs_statuses_and_evidence(void) {
    static const struct {
        const char *label;
        const char *section_anchor;
        const char *row_anchor;
        const char *required_source_path;
        const char *required_evidence_path;
        const char *required_related_evidence_path;
        const char *required_status_code;
        const char *required_secondary_status_code;
        const char *required_concept;
    } filter_rows[] = {
        {"CreateMonitoredItems DataChangeFilter", "OPC-10000-4 section 5.13.2", "DataChangeFilter",
         "`src/services/subscription.c`", "`tests/unit/test_subscriptions_errors.c`", "`docs/conformance/services.md`",
         "Bad_MonitoredItemFilterUnsupported", "Bad_MonitoredItemFilterInvalid", "percent deadband"},
        {"ModifyMonitoredItems DataChangeFilter", "OPC-10000-4 section 5.13.3", "DataChangeFilter",
         "`src/services/subscription.c`", "`tests/unit/test_subscriptions_errors.c`", "`docs/conformance/services.md`",
         "Bad_MonitoredItemFilterUnsupported", "Bad_MonitoredItemFilterInvalid", "malformed filter"},
        {"DataChangeFilter definition", "OPC-10000-4 section 7.22.2", "DataChangeFilter",
         "`src/core/service_dispatch.c`", "`tests/unit/test_subscriptions_errors.c`", "`docs/conformance/services.md`",
         "Bad_MonitoredItemFilterUnsupported", "Bad_MonitoredItemFilterInvalid", "percent deadband"},
        {"CreateMonitoredItems AggregateFilter", "OPC-10000-4 section 5.13.2", "AggregateFilter",
         "`src/core/service_dispatch.c`", "`tests/unit/test_aggregate.c`", "`docs/conformance/services.md`",
         "Bad_MonitoredItemFilterUnsupported", NULL, "Unsupported aggregate"},
        {"AggregateFilter definition", "OPC-10000-4 section 7.22.4", "AggregateFilter", "`src/core/service_dispatch.c`",
         "`tests/unit/test_aggregate.c`", "`docs/conformance/services.md`", "Bad_MonitoredItemFilterUnsupported", NULL,
         "malformed filter"},
    };
    static const struct {
        const char *status_code;
        const char *required_evidence_path;
        const char *required_secondary_evidence_path;
    } negative_path_status_rows[] = {
        {"Bad_MonitoredItemFilterUnsupported", "`tests/unit/test_subscriptions_errors.c`",
         "`tests/unit/test_aggregate.c`"},
        {"Bad_MonitoredItemFilterInvalid", "`tests/unit/test_subscriptions_errors.c`", NULL},
    };
    char path[1024];
    char failures[8192];
    const char *negative_path_row;
    const char *negative_path_section;
    const char *negative_path_section_end;
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    negative_path_row = find_markdown_table_row_containing(feature_023_content, "Negative-path evidence");
    if (!negative_path_row) {
        append_traceability_failure(failures, sizeof(failures), "US4 filter evidence",
                                    "missing Negative-path evidence lane");
    } else {
        if (!markdown_table_row_has_anchor(negative_path_row, "OPC-10000-4 §7.38.2")) {
            append_traceability_failure(failures, sizeof(failures), "US4 filter evidence",
                                        "negative-path evidence lane is missing OPC-10000-4 §7.38.2");
        }
        if (!markdown_table_row_has_anchor(negative_path_row, "`tests/unit/test_subscriptions_errors.c`")) {
            append_traceability_failure(failures, sizeof(failures), "US4 filter evidence",
                                        "negative-path evidence lane is missing DataChangeFilter unit evidence");
        }
        if (!markdown_table_row_has_anchor(negative_path_row, "`tests/unit/test_aggregate.c`")) {
            append_traceability_failure(failures, sizeof(failures), "US4 filter evidence",
                                        "negative-path evidence lane is missing AggregateFilter unit evidence");
        }
    }

    negative_path_section = strstr(feature_023_content, "## Negative-Path Evidence");
    negative_path_section_end = NULL;
    if (!negative_path_section) {
        append_traceability_failure(failures, sizeof(failures), "US4 filter evidence",
                                    "missing Negative-Path Evidence section");
    } else {
        negative_path_section_end = strstr(negative_path_section + strlen("## Negative-Path Evidence"), "\n## ");
    }

    for (i = 0; i < sizeof(filter_rows) / sizeof(filter_rows[0]); ++i) {
        const char *row = find_feature_mapping_row_for_section_with_text(
            feature_023_content, filter_rows[i].section_anchor, filter_rows[i].row_anchor);

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing OPC UA Section Mapping row");
            continue;
        }

        if (!feature_mapping_row_cell_has_text(row, 2, filter_rows[i].required_source_path)) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing required filter implementation source path");
        }
        if (!feature_mapping_row_cell_has_text(row, 3, filter_rows[i].required_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing required filter unit evidence path");
        }
        if (!feature_mapping_row_cell_has_text(row, 3, filter_rows[i].required_related_evidence_path)) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing required conformance documentation evidence path");
        }
        if (!feature_mapping_row_cell_has_text(row, 4, filter_rows[i].required_status_code)) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing required filter StatusCode in row notes");
        }
        if (filter_rows[i].required_secondary_status_code &&
            !feature_mapping_row_cell_has_text(row, 4, filter_rows[i].required_secondary_status_code)) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing required secondary filter StatusCode in row notes");
        }
        if (!feature_mapping_row_cell_has_text(row, 4, filter_rows[i].required_concept)) {
            append_traceability_failure(failures, sizeof(failures), filter_rows[i].label,
                                        "missing required filter evidence concept in row notes");
        }
    }

    if (negative_path_section) {
        for (i = 0; i < sizeof(negative_path_status_rows) / sizeof(negative_path_status_rows[0]); ++i) {
            const char *evidence_status = strstr(negative_path_section, negative_path_status_rows[i].status_code);

            if (evidence_status && negative_path_section_end && evidence_status >= negative_path_section_end)
                evidence_status = NULL;

            if (!evidence_status) {
                append_traceability_failure(failures, sizeof(failures), negative_path_status_rows[i].status_code,
                                            "missing StatusCode entry in Negative-Path Evidence section");
            } else {
                const char *item_start = line_start_for(feature_023_content, evidence_status);
                const char *item_end = markdown_list_item_end(item_start);

                if (!contains_between(item_start, item_end, negative_path_status_rows[i].required_evidence_path)) {
                    append_traceability_failure(failures, sizeof(failures), negative_path_status_rows[i].status_code,
                                                "negative-path StatusCode evidence is missing primary unit path");
                }
                if (negative_path_status_rows[i].required_secondary_evidence_path &&
                    !contains_between(item_start, item_end,
                                      negative_path_status_rows[i].required_secondary_evidence_path)) {
                    append_traceability_failure(failures, sizeof(failures), negative_path_status_rows[i].status_code,
                                                "negative-path StatusCode evidence is missing secondary unit path");
                }
            }
        }
    }

    if (failures[0] != '\0')
        TEST_FAIL_MESSAGE(failures);
}

void test_023_feature_traceability_has_story_evidence_rows(void) {
    static const struct {
        const char *story;
        const char *story_anchor;
        const char *evidence_row_anchor;
        const char *required_row_anchors[6];
    } story_rows[] = {
        {"US1 documentation cleanup",
         "## US1 Documentation Evidence",
         "Documentation cleanup",
         {"OPC-10000-7 §4.2/§4.3", "`docs/conformance/services.md`", "`tests/unit/test_conformance_docs.c`",
          "`docs/traceability/conformance-claims.md`", NULL, NULL}},
        {"US2 stale-claim and stale-number checks",
         "| US2 | OPC-10000-4 section 7.38.2",
         "Stale-claim and stale-number checks",
         {"OPC-10000-7 §4.2/§4.3", "OPC-10000-4 §7.38.2", "`tests/unit/test_conformance_docs.c`",
          "`tests/unit/test_traceability_docs.c`", "`docs/traceability/conformance-claims.md`", NULL}},
        {"US3 PubSub subscriber decoder",
         "| US3 | OPC-10000-14 section 7.2.4.4.2",
         "PubSub subscriber decoder",
         {"OPC-10000-14 §7.2.4.4.2", "OPC-10000-4 §7.38.2", "`include/micro_opcua/pubsub.h`",
          "`src/encoding/uadp_encoder.c`", "`tests/unit/test_uadp_encoding.c`", "`tests/unit/test_pubsub.c`"}},
        {"US4 negative-path evidence",
         "| US4 | OPC-10000-4 section 5.14.5.4",
         "Negative-path evidence",
         {"OPC-10000-4 §7.38.2", "`tests/unit/test_subscriptions_errors.c`", "`tests/integration/test_subscriptions.c`",
          "`tests/unit/test_aggregate.c`", "`tests/unit/test_traceability_docs.c`", NULL}},
    };
    char path[1024];
    char failures[4096];
    size_t i;

    failures[0] = '\0';
    snprintf(path, sizeof(path), "%s/docs/traceability/023-conformance-docs-subscriber.md", PROJECT_ROOT_DIR);
    read_file_content(path, feature_023_content, sizeof(feature_023_content));
    TEST_ASSERT_MESSAGE(strlen(feature_023_content) > 0, "Could not read 023-conformance-docs-subscriber.md");

    for (i = 0; i < sizeof(story_rows) / sizeof(story_rows[0]); ++i) {
        const char *row = find_markdown_table_row_containing(feature_023_content, story_rows[i].evidence_row_anchor);
        size_t j;

        if (!strstr(feature_023_content, story_rows[i].story_anchor)) {
            append_traceability_failure(failures, sizeof(failures), story_rows[i].story,
                                        "missing story row or section anchor");
        }

        if (!row) {
            append_traceability_failure(failures, sizeof(failures), story_rows[i].story,
                                        "missing Evidence Skeleton row");
            continue;
        }

        if (!feature_mapping_row_has_concrete_source_or_doc_path(row)) {
            append_traceability_failure(failures, sizeof(failures), story_rows[i].story,
                                        "evidence row has no concrete source/doc path");
        }
        if (!feature_mapping_row_has_test_or_doc_evidence(row)) {
            append_traceability_failure(failures, sizeof(failures), story_rows[i].story,
                                        "evidence row has no concrete test/doc evidence path");
        }

        for (j = 0; j < sizeof(story_rows[i].required_row_anchors) / sizeof(story_rows[i].required_row_anchors[0]);
             ++j) {
            if (!story_rows[i].required_row_anchors[j])
                continue;

            if (!markdown_table_row_has_anchor(row, story_rows[i].required_row_anchors[j])) {
                append_traceability_failure(failures, sizeof(failures), story_rows[i].story,
                                            "evidence row is missing a required OPC or evidence anchor");
            }
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
    RUN_TEST(test_023_feature_traceability_pubsub_rows_have_sections_sources_and_evidence);
    RUN_TEST(test_023_feature_traceability_documentation_and_conformance_check_rows_have_sources_and_evidence);
    RUN_TEST(test_023_feature_traceability_lists_negative_path_evidence);
    RUN_TEST(test_023_feature_traceability_invalid_id_rows_have_opc_refs_statuses_and_evidence);
    RUN_TEST(test_023_feature_traceability_publish_republish_rows_have_opc_refs_statuses_and_evidence);
    RUN_TEST(test_023_feature_traceability_filter_rows_have_opc_refs_statuses_and_evidence);
    RUN_TEST(test_023_feature_traceability_has_story_evidence_rows);
    return UNITY_END();
}
