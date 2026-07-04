/* tests/unit/test_no_stale_project_name.c
 *
 * Feature 024 (rename to muc-opcua) regression guard: FR-008/SC-006. Walks the
 * tracked tree and fails if any non-excluded file still contains the old
 * project name in one of its five literal forms (research.md Decision 1).
 *
 * Exclusions (all frozen/historical, self-referential, or generated/untracked
 * build output, not "current documentation a reader relies on"):
 *   - build output directories (including .NET's bin/obj) and VCS metadata
 *   - the entire specs/ tree: every specs/NNN-feature directory is a spec-kit
 *     planning/history record for one feature (already-shipped for 001-023,
 *     and this rename's own record for 024 itself, which necessarily quotes
 *     the old name throughout as before/after examples) — see spec.md's edge
 *     case on this exact point.
 *   - the per-feature docs/traceability/NNN-*.md files (frozen history for
 *     already-shipped features; the living index files in the same directory
 *     are NOT excluded).
 *   - one explicit, path-based allow-list entry for the migration note.
 *   - two self-referential test files (this one, and test_traceability_docs.c)
 *     whose job requires quoting the old name verbatim.
 */
#define _POSIX_C_SOURCE 200809L
#include "unity.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

void setUp(void) {}
void tearDown(void) {}

static const char *const forbidden_literals[] = {
    "MICRO_OPCUA", "micro_opcua", "micro-opcua", "MicroOpcUa", "Micro-OPCUA", "Micro OPC UA",
};

/* Directory names, matched by exact basename, that are skipped entirely. */
static const char *const excluded_dir_names[] = {
    ".git", "specs", /* every specs/NNN-feature directory is a spec-kit planning/history record */
    "obj",           /* .NET build intermediates (tests/interop/dotnet/obj), gitignored */
    "bin",           /* .NET build output (tests/interop/dotnet/bin), gitignored */
};

/* "build" prefixed directories are matched by prefix, not exact name (build,
   build-san, build-fuzz, build/size-arm/... all live under a "build" prefix). */
static int is_build_dir_name(const char *name) {
    return strncmp(name, "build", 5) == 0;
}

/* docs/traceability/NNN-*.md files that are frozen history for an
   already-shipped feature (research.md Decision 4/5). The living index files
   in the same directory (sections-to-files.md, files-to-sections.md,
   conformance-claims.md) are intentionally NOT in this list. */
static const char *const excluded_traceability_files[] = {
    "docs/traceability/004-optimization-fixes.md",
    "docs/traceability/005-embedded-profile-completion.md",
    "docs/traceability/007-optional-write-service.md",
    "docs/traceability/008-user-identity-auth.md",
    "docs/traceability/012-opcua-pubsub.md",
    "docs/traceability/013-advanced-security.md",
    "docs/traceability/019-fix-conformance-size.md",
    "docs/traceability/022-optimize-hot-paths.md",
    "docs/traceability/023-conformance-docs-subscriber.md",
};

/* The single allow-list entry: the migration note naming the old project name
   for historical/migration purposes (FR-007). Path is relative to
   PROJECT_ROOT_DIR. Kept as exactly one entry, per
   contracts/regression-guard-contract.md. */
static const char *const allow_listed_files[] = {
    "docs/migration-024-muc-opcua-rename.md",
};

/* Test files whose job is to search for or quote the old name, distinct from
   the migration-note allow-list above (that one names the FR-007 exception
   budget; this one is a structural necessity of self-referential tests):
   - this guard's own source unavoidably contains the old-name literals it
     searches for, in forbidden_literals[] and its failure message;
   - test_traceability_docs.c quotes include/micro_opcua/... paths verbatim
     because those quotes must match the frozen docs/traceability/023-*.md
     content byte-for-byte (see that file's own comments). */
static const char *const self_referential_test_files[] = {
    "tests/unit/test_no_stale_project_name.c",
    "tests/unit/test_traceability_docs.c",
};

/* Binary/non-text extensions skipped so the scan never mis-decodes bytes;
   none of these are expected to contain the literal ASCII old-name strings. */
static const char *const skipped_extensions[] = {
    ".bin", ".o", ".a", ".so", ".exe", ".dll", ".png", ".jpg", ".jpeg", ".ico", ".pdf", ".elf", ".uf2",
};

static int has_suffix(const char *name, const char *suffix) {
    size_t name_len = strlen(name);
    size_t suffix_len = strlen(suffix);
    return name_len >= suffix_len && strcmp(name + (name_len - suffix_len), suffix) == 0;
}

static int is_skipped_extension(const char *name) {
    for (size_t i = 0u; i < ARRAY_COUNT(skipped_extensions); ++i) {
        if (has_suffix(name, skipped_extensions[i])) {
            return 1;
        }
    }
    return 0;
}

static int relative_path_matches_any(const char *relative_path, const char *const *list, size_t count) {
    for (size_t i = 0u; i < count; ++i) {
        if (strcmp(relative_path, list[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_excluded_dir_name(const char *name) {
    if (is_build_dir_name(name)) {
        return 1;
    }
    for (size_t i = 0u; i < ARRAY_COUNT(excluded_dir_names); ++i) {
        if (strcmp(name, excluded_dir_names[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static void check_file_for_forbidden_literals(const char *full_path, const char *relative_path, size_t *checked_files,
                                              size_t *forbidden_matches) {
    if (relative_path_matches_any(relative_path, excluded_traceability_files,
                                  ARRAY_COUNT(excluded_traceability_files))) {
        return;
    }
    if (relative_path_matches_any(relative_path, allow_listed_files, ARRAY_COUNT(allow_listed_files))) {
        return;
    }
    if (relative_path_matches_any(relative_path, self_referential_test_files,
                                  ARRAY_COUNT(self_referential_test_files))) {
        return;
    }
    if (is_skipped_extension(full_path)) {
        return;
    }

    FILE *file = fopen(full_path, "r");
    if (file == NULL) {
        return; /* unreadable (e.g. broken symlink); not this guard's concern */
    }

    ++(*checked_files);
    char line[8192];
    size_t line_number = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        for (size_t i = 0u; i < ARRAY_COUNT(forbidden_literals); ++i) {
            if (strstr(line, forbidden_literals[i]) != NULL) {
                printf("Stale project-name literal '%s' in %s:%zu:%s", forbidden_literals[i], relative_path,
                       line_number, line);
                ++(*forbidden_matches);
            }
        }
    }

    fclose(file);
}

static void walk_tree_for_forbidden_literals(const char *root, const char *relative_prefix, size_t *checked_files,
                                             size_t *forbidden_matches) {
    DIR *dir = opendir(root);
    if (dir == NULL) {
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue; /* covers .git and other dotfiles/dotdirs uniformly */
        }

        char full_path[2048];
        int written = snprintf(full_path, sizeof(full_path), "%s/%s", root, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(full_path)) {
            continue;
        }

        char relative_path[2048];
        if (relative_prefix[0] == '\0') {
            written = snprintf(relative_path, sizeof(relative_path), "%s", entry->d_name);
        } else {
            written = snprintf(relative_path, sizeof(relative_path), "%s/%s", relative_prefix, entry->d_name);
        }
        if (written < 0 || (size_t)written >= sizeof(relative_path)) {
            continue;
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (is_excluded_dir_name(entry->d_name)) {
                continue;
            }
            walk_tree_for_forbidden_literals(full_path, relative_path, checked_files, forbidden_matches);
        } else if (S_ISREG(st.st_mode)) {
            check_file_for_forbidden_literals(full_path, relative_path, checked_files, forbidden_matches);
        }
    }

    closedir(dir);
}

void test_no_stale_project_name_outside_frozen_history(void) {
    size_t checked_files = 0u;
    size_t forbidden_matches = 0u;

    walk_tree_for_forbidden_literals(PROJECT_ROOT_DIR, "", &checked_files, &forbidden_matches);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files, "Expected to scan a non-trivial number of tracked files");
    TEST_ASSERT_EQUAL_MESSAGE(0u, forbidden_matches,
                              "Found stale micro-opcua/micro_opcua/MICRO_OPCUA/MicroOpcUa references outside "
                              "frozen history (specs/**, docs/traceability/NNN-*.md) and the migration note "
                              "allow-list; see research.md Decision 1/4/6");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_no_stale_project_name_outside_frozen_history);
    return UNITY_END();
}
