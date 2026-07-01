/* tests/unit/test_conformance_docs.c */
#define _POSIX_C_SOURCE 200809L
#include "micro_opcua/status.h"
#include "unity.h"
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PROJECT_ROOT_DIR
#define PROJECT_ROOT_DIR "."
#endif

#define ARRAY_COUNT(array) (sizeof(array) / sizeof((array)[0]))

void setUp(void) {}
void tearDown(void) {}

typedef struct {
    const char *name;
    opcua_statuscode_t status;
} statuscode_name_entry_t;

typedef struct {
    const char *description;
    const char *const *terms;
    size_t term_count;
} doc_evidence_requirement_t;

static const statuscode_name_entry_t known_statuscode_names[] = {
    {"Good", MU_STATUS_GOOD},
    {"Bad_UnexpectedError", MU_STATUS_BAD_UNEXPECTEDERROR},
    {"Bad_InternalError", MU_STATUS_BAD_INTERNALERROR},
    {"Bad_OutOfMemory", MU_STATUS_BAD_OUTOFMEMORY},
    {"Bad_EncodingError", MU_STATUS_BAD_ENCODINGERROR},
    {"Bad_DecodingError", MU_STATUS_BAD_DECODINGERROR},
    {"Bad_EncodingLimitsExceeded", MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED},
    {"Bad_Timeout", MU_STATUS_BAD_TIMEOUT},
    {"Bad_ServiceUnsupported", MU_STATUS_BAD_SERVICEUNSUPPORTED},
    {"Bad_NotSupported", MU_STATUS_BAD_NOTSUPPORTED},
    {"Bad_NotReadable", MU_STATUS_BAD_NOTREADABLE},
    {"Bad_SecurityChecksFailed", MU_STATUS_BAD_SECURITYCHECKSFAILED},
    {"Bad_RequestTooLarge", MU_STATUS_BAD_REQUESTTOOLARGE},
    {"Bad_ResponseTooLarge", MU_STATUS_BAD_RESPONSETOOLARGE},
    {"Bad_SessionIdInvalid", MU_STATUS_BAD_SESSIONIDINVALID},
    {"Bad_SessionNotActivated", MU_STATUS_BAD_SESSIONNOTACTIVATED},
    {"Bad_IdentityTokenInvalid", MU_STATUS_BAD_IDENTITYTOKENINVALID},
    {"Bad_IdentityTokenRejected", MU_STATUS_BAD_IDENTITYTOKENREJECTED},
    {"Bad_SecureChannelIdInvalid", MU_STATUS_BAD_SECURECHANNELIDINVALID},
    {"Bad_SecurityPolicyRejected", MU_STATUS_BAD_SECURITYPOLICYREJECTED},
    {"Bad_TooManySessions", MU_STATUS_BAD_TOOMANYSESSIONS},
    {"Bad_SessionClosed", MU_STATUS_BAD_SESSIONCLOSED},
    {"Bad_UserAccessDenied", MU_STATUS_BAD_USERACCESSDENIED},
    {"Bad_NodeIdUnknown", MU_STATUS_BAD_NODEIDUNKNOWN},
    {"Bad_NotFound", MU_STATUS_BAD_NOTFOUND},
    {"Bad_NodeClassInvalid", MU_STATUS_BAD_NODECLASSINVALID},
    {"Bad_NodeIdExists", MU_STATUS_BAD_NODEIDEXISTS},
    {"Bad_NodeIdRejected", MU_STATUS_BAD_NODEIDREJECTED},
    {"Bad_MonitoredItemIdInvalid", MU_STATUS_BAD_MONITOREDITEMIDINVALID},
    {"Bad_TooManyOperations", MU_STATUS_BAD_TOOMANYOPERATIONS},
    {"Bad_NodeIdInvalid", MU_STATUS_BAD_NODEIDINVALID},
    {"Bad_MethodInvalid", MU_STATUS_BAD_METHODINVALID},
    {"Bad_ArgumentsMissing", MU_STATUS_BAD_ARGUMENTSMISSING},
    {"Bad_TooManyArguments", MU_STATUS_BAD_TOOMANYARGUMENTS},
    {"Bad_InvalidArgument", MU_STATUS_BAD_INVALIDARGUMENT},
    {"Bad_NoContinuationPoints", MU_STATUS_BAD_NOCONTINUATIONPOINTS},
    {"Bad_ContinuationPointInvalid", MU_STATUS_BAD_CONTINUATIONPOINTINVALID},
    {"Bad_AttributeIdInvalid", MU_STATUS_BAD_ATTRIBUTEIDINVALID},
    {"Bad_NoMatch", MU_STATUS_BAD_NOMATCH},
    {"Bad_CertificateInvalid", MU_STATUS_BAD_CERTIFICATEINVALID},
    {"Bad_SecurityModeRejected", MU_STATUS_BAD_SECURITYMODEREJECTED},
    {"Bad_TooManyMonitoredItems", MU_STATUS_BAD_TOOMANYMONITOREDITEMS},
    {"Bad_TooManySubscriptions", MU_STATUS_BAD_TOOMANYSUBSCRIPTIONS},
    {"Bad_SubscriptionIdInvalid", MU_STATUS_BAD_SUBSCRIPTIONIDINVALID},
#if MICRO_OPCUA_SUBSCRIPTIONS
    {"Bad_MessageNotAvailable", MU_STATUS_BAD_MESSAGENOTAVAILABLE},
    {"Bad_SequenceNumberUnknown", MU_STATUS_BAD_SEQUENCENUMBERUNKNOWN},
    {"Bad_TooManyPublishRequests", MU_STATUS_BAD_TOOMANYPUBLISHREQUESTS},
#endif
    {"Bad_NothingToDo", MU_STATUS_BAD_NOTHINGTODO},
    {"Bad_NotWritable", MU_STATUS_BAD_NOTWRITABLE},
    {"Bad_WriteNotSupported", MU_STATUS_BAD_WRITENOTSUPPORTED},
    {"Bad_TypeMismatch", MU_STATUS_BAD_TYPEMISMATCH},
    {"Bad_HistoryOperationUnsupported", MU_STATUS_BAD_HISTORYOPERATIONUNSUPPORTED},
    {"Bad_MonitoredItemFilterUnsupported", MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED},
    {"Bad_MonitoredItemFilterInvalid", MU_STATUS_BAD_MONITOREDITEMFILTERINVALID},
    {"Bad_FilterNotAllowed", MU_STATUS_BAD_FILTERNOTALLOWED},
    {"Bad_TimestampsToReturnInvalid", MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID},
    {"Bad_BrowseDirectionInvalid", MU_STATUS_BAD_BROWSEDIRECTIONINVALID},
    {"Bad_NoData", MU_STATUS_BAD_NODATA},
    {"Bad_TcpServerTooBusy", MU_STATUS_BAD_TCPSERVERTOOBUSY},
    {"Bad_TcpMessageTypeInvalid", MU_STATUS_BAD_TCPMESSAGETYPEINVALID},
    {"Bad_TcpSecureChannelUnknown", MU_STATUS_BAD_TCPSECURECHANNELUNKNOWN},
    {"Bad_TcpMessageTooLarge", MU_STATUS_BAD_TCPMESSAGETOOLARGE},
    {"Bad_TcpNotEnoughResources", MU_STATUS_BAD_TCPNOTENOUGHRESOURCES},
    {"Bad_TcpInternalError", MU_STATUS_BAD_TCPINTERNALERROR},
    {"Bad_TcpEndpointUrlInvalid", MU_STATUS_BAD_TCPENDPOINTURLINVALID},
    {"Bad_CertificateUntrusted", MU_STATUS_BAD_CERTIFICATEUNTRUSTED},
    {"Bad_UserCertificateInvalid", MU_STATUS_BAD_USERCERTIFICATEINVALID},
    {"Bad_UserCertificateUntrusted", MU_STATUS_BAD_USERCERTIFICATEUNTRUSTED},
};

static const char *const feature_required_statuscode_names[] = {
    "Good",
    "Bad_DecodingError",
    "Bad_EncodingLimitsExceeded",
    "Bad_ServiceUnsupported",
    "Bad_NotSupported",
    "Bad_InvalidArgument",
    "Bad_MonitoredItemFilterUnsupported",
    "Bad_MonitoredItemFilterInvalid",
    "Bad_FilterNotAllowed",
    "Bad_TimestampsToReturnInvalid",
    "Bad_BrowseDirectionInvalid",
    "Bad_SessionNotActivated",
    "Bad_SessionIdInvalid",
    "Bad_SecureChannelIdInvalid",
    "Bad_SecurityChecksFailed",
    "Bad_IdentityTokenRejected",
    "Bad_ResponseTooLarge",
    "Bad_TcpMessageTypeInvalid",
    "Bad_TooManyOperations",
    "Bad_TypeMismatch",
    "Bad_ContinuationPointInvalid",
};

static const statuscode_name_entry_t feature_required_statuscode_values[] = {
    {"Good", MU_STATUS_GOOD},
    {"Bad_DecodingError", MU_STATUS_BAD_DECODINGERROR},
    {"Bad_EncodingLimitsExceeded", MU_STATUS_BAD_ENCODINGLIMITSEXCEEDED},
    {"Bad_ServiceUnsupported", MU_STATUS_BAD_SERVICEUNSUPPORTED},
    {"Bad_NotSupported", MU_STATUS_BAD_NOTSUPPORTED},
    {"Bad_InvalidArgument", MU_STATUS_BAD_INVALIDARGUMENT},
    {"Bad_MonitoredItemFilterUnsupported", MU_STATUS_BAD_MONITOREDITEMFILTERUNSUPPORTED},
    {"Bad_MonitoredItemFilterInvalid", MU_STATUS_BAD_MONITOREDITEMFILTERINVALID},
    {"Bad_FilterNotAllowed", MU_STATUS_BAD_FILTERNOTALLOWED},
    {"Bad_TimestampsToReturnInvalid", MU_STATUS_BAD_TIMESTAMPSTORETURNINVALID},
    {"Bad_BrowseDirectionInvalid", MU_STATUS_BAD_BROWSEDIRECTIONINVALID},
    {"Bad_SessionNotActivated", MU_STATUS_BAD_SESSIONNOTACTIVATED},
    {"Bad_SessionIdInvalid", MU_STATUS_BAD_SESSIONIDINVALID},
    {"Bad_SecureChannelIdInvalid", MU_STATUS_BAD_SECURECHANNELIDINVALID},
    {"Bad_SecurityChecksFailed", MU_STATUS_BAD_SECURITYCHECKSFAILED},
    {"Bad_IdentityTokenRejected", MU_STATUS_BAD_IDENTITYTOKENREJECTED},
    {"Bad_ResponseTooLarge", MU_STATUS_BAD_RESPONSETOOLARGE},
    {"Bad_TcpMessageTypeInvalid", MU_STATUS_BAD_TCPMESSAGETYPEINVALID},
    {"Bad_TooManyOperations", MU_STATUS_BAD_TOOMANYOPERATIONS},
    {"Bad_TypeMismatch", MU_STATUS_BAD_TYPEMISMATCH},
    {"Bad_ContinuationPointInvalid", MU_STATUS_BAD_CONTINUATIONPOINTINVALID},
};

static int contains_ci(const char *haystack, const char *needle) {
    size_t needle_len = strlen(needle);
    if (needle_len == 0u) {
        return 1;
    }

    for (const char *cursor = haystack; *cursor != '\0'; ++cursor) {
        size_t i = 0u;
        while (i < needle_len && cursor[i] != '\0' &&
               tolower((unsigned char)cursor[i]) == tolower((unsigned char)needle[i])) {
            ++i;
        }
        if (i == needle_len) {
            return 1;
        }
    }

    return 0;
}

static int contains_any_ci(const char *line, const char *const *terms, size_t term_count) {
    for (size_t i = 0u; i < term_count; ++i) {
        if (contains_ci(line, terms[i])) {
            return 1;
        }
    }
    return 0;
}

static int is_statuscode_name_char(char ch) {
    return isalnum((unsigned char)ch) || ch == '_';
}

static int statuscode_name_matches(const char *token, size_t token_len, const char *expected) {
    size_t expected_len = strlen(expected);
    return token_len == expected_len && strncmp(token, expected, token_len) == 0;
}

static int is_known_statuscode_name(const char *token, size_t token_len) {
    for (size_t i = 0u; i < ARRAY_COUNT(known_statuscode_names); ++i) {
        if (statuscode_name_matches(token, token_len, known_statuscode_names[i].name)) {
            return 1;
        }
    }
    return 0;
}

static int begins_statuscode_name_token(const char *cursor) {
    if (strncmp(cursor, "Bad_", 4u) == 0) {
        return isupper((unsigned char)cursor[4]);
    }
    if (strncmp(cursor, "Bad", 3u) == 0) {
        return isupper((unsigned char)cursor[3]);
    }
    if (strncmp(cursor, "Good", 4u) == 0) {
        char next = cursor[4];
        return next == '\0' || !is_statuscode_name_char(next) || next == '_' || isupper((unsigned char)next);
    }
    return 0;
}

static int line_contains_statuscode_name(const char *line, const char *expected) {
    size_t expected_len = strlen(expected);
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (cursor != line && is_statuscode_name_char(cursor[-1])) {
            continue;
        }
        if (strncmp(cursor, expected, expected_len) == 0 && !is_statuscode_name_char(cursor[expected_len])) {
            return 1;
        }
    }
    return 0;
}

static int parse_next_hex_u32(const char **cursor, opcua_statuscode_t *value) {
    const char *scan = *cursor;
    while (*scan != '\0') {
        if (scan[0] == '0' && (scan[1] == 'x' || scan[1] == 'X') && isxdigit((unsigned char)scan[2])) {
            char *end = NULL;
            unsigned long parsed = strtoul(scan + 2, &end, 16);
            if (end != scan + 2 && parsed <= 0xFFFFFFFFul) {
                *value = (opcua_statuscode_t)parsed;
                *cursor = end;
                return 1;
            }
        }
        ++scan;
    }

    *cursor = scan;
    return 0;
}

static int line_has_statuscode_hex_value(const char *line, opcua_statuscode_t expected, opcua_statuscode_t *first_value,
                                         int *saw_value) {
    const char *cursor = line;
    opcua_statuscode_t parsed = 0u;
    *saw_value = 0;

    while (parse_next_hex_u32(&cursor, &parsed)) {
        if (!*saw_value) {
            *first_value = parsed;
        }
        *saw_value = 1;
        if (parsed == expected) {
            return 1;
        }
    }

    return 0;
}

static void check_line_for_unknown_statuscode_names(const char *path, size_t line_number, const char *line,
                                                    size_t *unknown_names) {
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (cursor != line && is_statuscode_name_char(cursor[-1])) {
            continue;
        }
        if (!begins_statuscode_name_token(cursor)) {
            continue;
        }

        size_t token_len = 0u;
        while (is_statuscode_name_char(cursor[token_len])) {
            ++token_len;
        }

        if (!is_known_statuscode_name(cursor, token_len)) {
            printf("Unknown StatusCode name per OPC-10000-4 section 7.38.2: %s:%zu:%.*s\n", path, line_number,
                   (int)token_len, cursor);
            ++(*unknown_names);
        }
    }
}

static int has_markdown_extension(const char *name) {
    const char *ext = strrchr(name, '.');
    return ext != NULL && strcmp(ext, ".md") == 0;
}

static int has_c_extension(const char *name) {
    const char *ext = strrchr(name, '.');
    return ext != NULL && strcmp(ext, ".c") == 0;
}

static int has_profile_compliant_wording(const char *line) {
    static const char *const terms[] = {
        "profile-compliant",
        "profile compliant",
        "profile-compliance",
        "profile compliance",
    };
    return contains_any_ci(line, terms, sizeof(terms) / sizeof(terms[0]));
}

static int has_ctt_verified_wording(const char *line) {
    static const char *const terms[] = {
        "CTT-verified",
        "CTT verified",
        "CTT-certified",
        "CTT certified",
        "CTT-approved",
        "CTT approved",
        "CTT passed",
        "passed CTT",
        "UACTT-verified",
        "UACTT verified",
        "UACTT-certified",
        "UACTT certified",
        "UACTT passed",
        "passed UACTT",
        "verified by CTT",
        "verified by the CTT",
        "certified by CTT",
        "certified by the CTT",
        "verified by the OPC Foundation Compliance Test Tool",
        "certified by the OPC Foundation Compliance Test Tool",
    };
    return contains_any_ci(line, terms, sizeof(terms) / sizeof(terms[0]));
}

static int has_claim_wording(const char *line) {
    return has_profile_compliant_wording(line) || has_ctt_verified_wording(line);
}

static int has_negative_or_policy_context(const char *line) {
    static const char *const allowed_terms[] = {
        "not profile-compliant",
        "not profile compliant",
        "not profile-compliance",
        "not ctt-verified",
        "not ctt verified",
        "not ctt-certified",
        "not ctt certified",
        "no profile-compliant",
        "no profile compliant",
        "no profile-compliance",
        "no profile compliance",
        "no profile-compliance claim",
        "no profile compliance claim",
        "no-profile-compliance",
        "out of scope",
        "no ctt-verified",
        "no ctt verified",
        "no ctt-certified",
        "no ctt certified",
        "no ctt/profile",
        "no external ctt evidence",
        "without",
        "until",
        "unless",
        "before any",
        "before promoting",
        "only then",
        "do not claim",
        "must not claim",
        "does not claim",
        "may not claim",
        "do not overclaim",
        "withheld",
        "not yet",
        "not backed by",
        "pending",
        "unsupported",
        "reject",
        "rejection",
        "rather than",
        "profile-targeting",
        "claim grep",
        "after an external ctt run",
        "rg -n",
        "grep -rni",
    };
    return contains_any_ci(line, allowed_terms, sizeof(allowed_terms) / sizeof(allowed_terms[0]));
}

static int has_explicit_external_ctt_evidence(const char *line) {
    if (!contains_ci(line, "CTT")) {
        return 0;
    }
    if (!contains_ci(line, "external") && !contains_ci(line, "OPC Foundation")) {
        return 0;
    }
    if (!contains_ci(line, "evidence") && !contains_ci(line, "artifact") && !contains_ci(line, "report") &&
        !contains_ci(line, "results")) {
        return 0;
    }
    return 1;
}

static int claim_wording_is_supported(const char *line) {
    return has_negative_or_policy_context(line) || has_explicit_external_ctt_evidence(line);
}

static const char *matching_fuzz_placeholder_marker(const char *line) {
    static const char *const markers[] = {
        "TODO", "To be implemented", "placeholder", "not implemented", "implement later", "TBD", "FIXME", "XXX", "stub",
    };

    for (size_t i = 0u; i < ARRAY_COUNT(markers); ++i) {
        if (contains_ci(line, markers[i])) {
            return markers[i];
        }
    }
    return NULL;
}

static const char *skip_space(const char *cursor) {
    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
        ++cursor;
    }
    return cursor;
}

static int c_identifier_matches_at(const char *cursor, const char *identifier) {
    size_t identifier_len = strlen(identifier);
    return strncmp(cursor, identifier, identifier_len) == 0 && !isalnum((unsigned char)cursor[identifier_len]) &&
           cursor[identifier_len] != '_';
}

static int line_contains_c_identifier(const char *line, const char *identifier) {
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if ((cursor == line || (!isalnum((unsigned char)cursor[-1]) && cursor[-1] != '_')) &&
            c_identifier_matches_at(cursor, identifier)) {
            return 1;
        }
    }
    return 0;
}

static const char *skip_fuzz_input_discard(const char *cursor) {
    static const char *const input_names[] = {"data", "size", "Data", "Size"};

    cursor = skip_space(cursor);
    if (*cursor != '(') {
        return NULL;
    }
    cursor = skip_space(cursor + 1);
    if (strncmp(cursor, "void", 4u) != 0 || (isalnum((unsigned char)cursor[4]) || cursor[4] == '_')) {
        return NULL;
    }
    cursor = skip_space(cursor + 4);
    if (*cursor != ')') {
        return NULL;
    }
    cursor = skip_space(cursor + 1);

    for (size_t i = 0u; i < ARRAY_COUNT(input_names); ++i) {
        const char *name = input_names[i];
        size_t name_len = strlen(name);
        if (strncmp(cursor, name, name_len) == 0 && !isalnum((unsigned char)cursor[name_len]) &&
            cursor[name_len] != '_') {
            cursor = skip_space(cursor + name_len);
            return *cursor == ';' ? cursor + 1 : NULL;
        }
    }

    return NULL;
}

static int line_has_fuzz_input_discard(const char *line) {
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (skip_fuzz_input_discard(cursor) != NULL) {
            return 1;
        }
    }
    return 0;
}

static int line_only_discards_fuzz_input(const char *line) {
    const char *cursor = skip_space(line);
    int saw_discard = 0;
    while (*cursor != '\0') {
        const char *after_discard = skip_fuzz_input_discard(cursor);
        if (after_discard == NULL) {
            return 0;
        }
        saw_discard = 1;
        cursor = skip_space(after_discard);
    }
    return saw_discard;
}

static int line_uses_fuzzer_input_value(const char *line) {
    if (!line_contains_c_identifier(line, "data") && !line_contains_c_identifier(line, "size") &&
        !line_contains_c_identifier(line, "Data") && !line_contains_c_identifier(line, "Size")) {
        return 0;
    }
    return !line_only_discards_fuzz_input(line);
}

static int line_has_return_zero_statement(const char *line) {
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if ((cursor == line || (!isalnum((unsigned char)cursor[-1]) && cursor[-1] != '_')) &&
            c_identifier_matches_at(cursor, "return")) {
            const char *after_return = skip_space(cursor + strlen("return"));
            if (*after_return == '0') {
                const char *after_zero = skip_space(after_return + 1);
                if (*after_zero == ';') {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void strip_c_comments_from_line(const char *line, int *in_block_comment, char *code, size_t code_size) {
    size_t out = 0u;
    for (size_t i = 0u; line[i] != '\0' && out + 1u < code_size; ++i) {
        if (*in_block_comment) {
            if (line[i] == '*' && line[i + 1u] == '/') {
                *in_block_comment = 0;
                ++i;
            }
            continue;
        }
        if (line[i] == '/' && line[i + 1u] == '*') {
            *in_block_comment = 1;
            ++i;
            continue;
        }
        if (line[i] == '/' && line[i + 1u] == '/') {
            break;
        }
        if (line[i] == '\n' || line[i] == '\r') {
            break;
        }
        code[out++] = line[i];
    }
    code[out] = '\0';
}

static int update_brace_depth(const char *line, int brace_depth) {
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (*cursor == '{') {
            ++brace_depth;
        } else if (*cursor == '}') {
            --brace_depth;
        }
    }
    return brace_depth;
}

static void check_fuzz_file_for_unused_input_placeholders(const char *path, size_t *checked_files,
                                                          size_t *unused_input_placeholders) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open fuzz harness: %s\n", path);
        ++(*unused_input_placeholders);
        return;
    }

    ++(*checked_files);
    char line[4096];
    char code[4096];
    int in_block_comment = 0;
    int in_fuzz_entry = 0;
    int brace_depth = 0;
    int saw_input_discard = 0;
    int saw_input_use = 0;
    int saw_return_zero = 0;
    size_t entry_line_number = 0u;
    size_t first_discard_line = 0u;
    size_t first_return_zero_line = 0u;
    size_t line_number = 0u;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        strip_c_comments_from_line(line, &in_block_comment, code, sizeof(code));

        const char *body_code = code;
        const char *brace_scan = code;
        if (!in_fuzz_entry) {
            if (strstr(code, "LLVMFuzzerTestOneInput") == NULL) {
                continue;
            }
            in_fuzz_entry = 1;
            brace_depth = 0;
            saw_input_discard = 0;
            saw_input_use = 0;
            saw_return_zero = 0;
            entry_line_number = line_number;
            first_discard_line = 0u;
            first_return_zero_line = 0u;
        }

        if (brace_depth == 0) {
            body_code = strchr(code, '{');
            if (body_code == NULL) {
                continue;
            }
            brace_scan = body_code;
            ++body_code;
        }

        if (line_has_fuzz_input_discard(body_code)) {
            saw_input_discard = 1;
            if (first_discard_line == 0u) {
                first_discard_line = line_number;
            }
        }
        if (line_uses_fuzzer_input_value(body_code)) {
            saw_input_use = 1;
        }
        if (line_has_return_zero_statement(body_code)) {
            saw_return_zero = 1;
            if (first_return_zero_line == 0u) {
                first_return_zero_line = line_number;
            }
        }

        brace_depth = update_brace_depth(brace_scan, brace_depth);
        if (brace_depth <= 0) {
            if (saw_input_discard) {
                printf("Fuzz harness discards fuzzer input in entry point: %s:%zu "
                       "(LLVMFuzzerTestOneInput at %zu)\n",
                       path, first_discard_line, entry_line_number);
                ++(*unused_input_placeholders);
            } else if (saw_return_zero && !saw_input_use) {
                printf("Fuzz harness returns 0 without using fuzzer input: %s:%zu "
                       "(LLVMFuzzerTestOneInput at %zu)\n",
                       path, first_return_zero_line, entry_line_number);
                ++(*unused_input_placeholders);
            }
            in_fuzz_entry = 0;
            brace_depth = 0;
        }
    }

    if (in_fuzz_entry) {
        printf("Could not parse fuzz entry point body: %s:%zu\n", path, entry_line_number);
        ++(*unused_input_placeholders);
    }

    fclose(file);
}

static void check_fuzz_tree_for_unused_input_placeholders(const char *dir_path, size_t *checked_files,
                                                          size_t *unused_input_placeholders) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        printf("Could not open fuzz harness directory: %s\n", dir_path);
        ++(*unused_input_placeholders);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char path[1024];
        int written = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            printf("Fuzz harness path too long under: %s\n", dir_path);
            ++(*unused_input_placeholders);
            continue;
        }

        struct stat st;
        if (stat(path, &st) != 0) {
            printf("Could not stat fuzz harness path: %s\n", path);
            ++(*unused_input_placeholders);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            check_fuzz_tree_for_unused_input_placeholders(path, checked_files, unused_input_placeholders);
        } else if (S_ISREG(st.st_mode) && has_c_extension(entry->d_name)) {
            check_fuzz_file_for_unused_input_placeholders(path, checked_files, unused_input_placeholders);
        }
    }

    closedir(dir);
}

static void check_file_for_unsupported_claims(const char *path, int (*matches_claim)(const char *line),
                                              size_t *checked_files, size_t *unsupported_claims) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open conformance doc: %s\n", path);
        ++(*unsupported_claims);
        return;
    }

    ++(*checked_files);
    char line[4096];
    size_t line_number = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        if (matches_claim(line) && !claim_wording_is_supported(line)) {
            printf("Unsupported conformance claim: %s:%zu:%s", path, line_number, line);
            ++(*unsupported_claims);
        }
    }

    fclose(file);
}

static int file_contains_text(const char *path, const char *needle) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        return 0;
    }

    char line[4096];
    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

static int line_is_blank(const char *line) {
    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (!isspace((unsigned char)*cursor)) {
            return 0;
        }
    }
    return 1;
}

static int append_doc_line_to_block(char *block, size_t block_size, const char *line) {
    size_t used = strlen(block);

    for (const char *cursor = line; *cursor != '\0'; ++cursor) {
        if (used + 2u >= block_size) {
            return 0;
        }
        block[used++] = (*cursor == '\n' || *cursor == '\r') ? ' ' : *cursor;
    }

    if (used + 1u >= block_size) {
        return 0;
    }
    block[used++] = ' ';
    block[used] = '\0';
    return 1;
}

static int doc_block_contains_all_terms(const char *block, const char *const *terms, size_t term_count) {
    for (size_t i = 0u; i < term_count; ++i) {
        if (strstr(block, terms[i]) == NULL) {
            return 0;
        }
    }
    return 1;
}

static int services_doc_has_evidence_block(const char *path, const doc_evidence_requirement_t *requirement) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open services doc: %s\n", path);
        return 0;
    }

    char block[32768];
    char line[4096];
    block[0] = '\0';

    while (fgets(line, sizeof(line), file) != NULL) {
        if (line_is_blank(line)) {
            if (doc_block_contains_all_terms(block, requirement->terms, requirement->term_count)) {
                fclose(file);
                return 1;
            }
            block[0] = '\0';
            continue;
        }

        if (!append_doc_line_to_block(block, sizeof(block), line)) {
            printf("Services doc paragraph too long while checking evidence for %s in %s\n", requirement->description,
                   path);
            fclose(file);
            return 0;
        }
    }

    int found = doc_block_contains_all_terms(block, requirement->terms, requirement->term_count);
    fclose(file);
    return found;
}

static void print_missing_services_doc_evidence(const char *path, const doc_evidence_requirement_t *requirement) {
    printf("Missing services doc evidence block for %s. Required terms in one paragraph/block:\n",
           requirement->description);
    for (size_t i = 0u; i < requirement->term_count; ++i) {
        const char *term = requirement->terms[i];
        printf("  - %s%s\n", term, file_contains_text(path, term) ? " (present elsewhere)" : "");
    }
}

static int line_has_resource_evidence_context(const char *line) {
    return contains_ci(line, "Measured snapshot") || contains_ci(line, "Measured 2026") ||
           contains_ci(line, "reproduce with") || contains_ci(line, "Current ARM Cortex-M0+ profile matrix") ||
           strstr(line, "scripts/measure_size.sh all") != NULL;
}

static int line_has_public_profile_size_number(const char *line) {
    int public_profile_line = contains_ci(line, "complete Nano server") || contains_ci(line, "Micro is") ||
                              contains_ci(line, "Embedded 2017 is") || contains_ci(line, "| **nano** |") ||
                              contains_ci(line, "| **micro** |") || contains_ci(line, "| **embedded**") ||
                              contains_ci(line, "| **Nano** |") || contains_ci(line, "| **Micro** |") ||
                              contains_ci(line, "| **Embedded 2017**") || contains_ci(line, "| **Full Featured**");
    int archive_profile_line = (contains_ci(line, "| nano |") || contains_ci(line, "| micro |") ||
                                contains_ci(line, "| embedded |") || contains_ci(line, "| full-featured |")) &&
                               contains_ci(line, "src/libmicro_opcua.a");

    if (public_profile_line) {
        return contains_ci(line, "KiB");
    }

    return archive_profile_line && contains_ci(line, " B |");
}

static void check_file_for_unlabeled_resource_numbers(const char *path, size_t *checked_files,
                                                      size_t *checked_resource_lines,
                                                      size_t *unlabeled_resource_numbers) {
    enum { evidence_context_line_limit = 12 };

    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open public resource doc: %s\n", path);
        ++(*unlabeled_resource_numbers);
        return;
    }

    ++(*checked_files);
    char line[4096];
    size_t line_number = 0u;
    size_t lines_since_evidence = evidence_context_line_limit + 1u;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (line_has_resource_evidence_context(line)) {
            lines_since_evidence = 0u;
        } else if (lines_since_evidence <= evidence_context_line_limit) {
            ++lines_since_evidence;
        }

        if (!line_has_public_profile_size_number(line)) {
            continue;
        }

        ++(*checked_resource_lines);
        if (lines_since_evidence > evidence_context_line_limit) {
            printf("Measured public size number lacks nearby snapshot/reproduction context: %s:%zu:%s", path,
                   line_number, line);
            ++(*unlabeled_resource_numbers);
        }
    }

    fclose(file);
}

static void assert_file_lacks_text(const char *path, const char *needle) {
    FILE *file = fopen(path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, path);

    char line[4096];
    size_t line_number = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        if (strstr(line, needle) != NULL) {
            printf("Stale public documentation text: %s:%zu matched '%s': %s", path, line_number, needle, line);
            fclose(file);
            TEST_FAIL_MESSAGE("Public documentation contains stale support text");
        }
    }

    fclose(file);
}

static void assert_file_lacks_stale_aggregate_nodeid_context(const char *path) {
    FILE *file = fopen(path, "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(file, path);

    char line[4096];
    size_t line_number = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        if ((strstr(line, "11565") || strstr(line, "11569") || strstr(line, "11570")) &&
            (contains_ci(line, "Aggregate") || contains_ci(line, "Average") || contains_ci(line, "Minimum") ||
             contains_ci(line, "Maximum"))) {
            printf("Stale aggregate NodeId context: %s:%zu:%s", path, line_number, line);
            fclose(file);
            TEST_FAIL_MESSAGE(
                "OPC-10000-4 section 7.22.4 and OPC-10000-13 sections 4.2.2.4/4.2.2.9/4.2.2.10 require "
                "AggregateFunction identifiers; public docs must not present stale OperationLimits NodeIds "
                "11565/11569/11570 as Average/Minimum/Maximum identifiers");
        }
    }

    fclose(file);
}

static void check_tree_for_unsupported_claims(const char *dir_path, int (*matches_claim)(const char *line),
                                              size_t *checked_files, size_t *unsupported_claims) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        printf("Could not open docs directory: %s\n", dir_path);
        ++(*unsupported_claims);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char path[1024];
        int written = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            printf("Conformance doc path too long under: %s\n", dir_path);
            ++(*unsupported_claims);
            continue;
        }

        struct stat st;
        if (stat(path, &st) != 0) {
            printf("Could not stat conformance doc path: %s\n", path);
            ++(*unsupported_claims);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            check_tree_for_unsupported_claims(path, matches_claim, checked_files, unsupported_claims);
        } else if (S_ISREG(st.st_mode) && has_markdown_extension(entry->d_name)) {
            check_file_for_unsupported_claims(path, matches_claim, checked_files, unsupported_claims);
        }
    }

    closedir(dir);
}

static void check_fuzz_file_for_placeholder_markers(const char *path, size_t *checked_files,
                                                    size_t *placeholder_markers) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open fuzz harness: %s\n", path);
        ++(*placeholder_markers);
        return;
    }

    ++(*checked_files);
    char line[4096];
    size_t line_number = 0u;
    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;
        const char *marker = matching_fuzz_placeholder_marker(line);
        if (marker != NULL) {
            printf("Fuzz harness placeholder marker: %s:%zu matched '%s': %s", path, line_number, marker, line);
            ++(*placeholder_markers);
        }
    }

    fclose(file);
}

static void check_fuzz_tree_for_placeholder_markers(const char *dir_path, size_t *checked_files,
                                                    size_t *placeholder_markers) {
    DIR *dir = opendir(dir_path);
    if (dir == NULL) {
        printf("Could not open fuzz harness directory: %s\n", dir_path);
        ++(*placeholder_markers);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        char path[1024];
        int written = snprintf(path, sizeof(path), "%s/%s", dir_path, entry->d_name);
        if (written < 0 || (size_t)written >= sizeof(path)) {
            printf("Fuzz harness path too long under: %s\n", dir_path);
            ++(*placeholder_markers);
            continue;
        }

        struct stat st;
        if (stat(path, &st) != 0) {
            printf("Could not stat fuzz harness path: %s\n", path);
            ++(*placeholder_markers);
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            check_fuzz_tree_for_placeholder_markers(path, checked_files, placeholder_markers);
        } else if (S_ISREG(st.st_mode) && has_c_extension(entry->d_name)) {
            check_fuzz_file_for_placeholder_markers(path, checked_files, placeholder_markers);
        }
    }

    closedir(dir);
}

static void check_statuscode_doc_file(const char *path, int require_feature_status_names, size_t *checked_files,
                                      size_t *statuscode_name_problems) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open StatusCode doc: %s\n", path);
        ++(*statuscode_name_problems);
        return;
    }

    ++(*checked_files);
    int found_required_names[ARRAY_COUNT(feature_required_statuscode_names)] = {0};
    char line[4096];
    size_t line_number = 0u;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        if (require_feature_status_names) {
            for (size_t i = 0u; i < ARRAY_COUNT(feature_required_statuscode_names); ++i) {
                if (line_contains_statuscode_name(line, feature_required_statuscode_names[i])) {
                    found_required_names[i] = 1;
                }
            }
        }

        check_line_for_unknown_statuscode_names(path, line_number, line, statuscode_name_problems);
    }

    fclose(file);

    if (require_feature_status_names) {
        for (size_t i = 0u; i < ARRAY_COUNT(feature_required_statuscode_names); ++i) {
            if (!found_required_names[i]) {
                printf("Missing required StatusCode name in %s: %s\n", path, feature_required_statuscode_names[i]);
                ++(*statuscode_name_problems);
            }
        }
    }
}

static void check_statuscode_numeric_doc_file(const char *path, size_t *checked_files,
                                              size_t *statuscode_value_problems) {
    FILE *file = fopen(path, "r");
    if (file == NULL) {
        printf("Could not open StatusCode doc: %s\n", path);
        ++(*statuscode_value_problems);
        return;
    }

    ++(*checked_files);
    int found_expected_values[ARRAY_COUNT(feature_required_statuscode_values)] = {0};
    int found_status_names[ARRAY_COUNT(feature_required_statuscode_values)] = {0};
    int reported_value_problems[ARRAY_COUNT(feature_required_statuscode_values)] = {0};
    char line[4096];
    size_t line_number = 0u;

    while (fgets(line, sizeof(line), file) != NULL) {
        ++line_number;

        for (size_t i = 0u; i < ARRAY_COUNT(feature_required_statuscode_values); ++i) {
            const statuscode_name_entry_t *required = &feature_required_statuscode_values[i];
            if (!line_contains_statuscode_name(line, required->name)) {
                continue;
            }

            found_status_names[i] = 1;
            opcua_statuscode_t documented_value = 0u;
            int saw_hex_value = 0;
            if (line_has_statuscode_hex_value(line, required->status, &documented_value, &saw_hex_value)) {
                found_expected_values[i] = 1;
            } else if (saw_hex_value && !reported_value_problems[i]) {
                printf("Mismatched StatusCode numeric value per OPC-10000-4 section 7.38.2: "
                       "%s:%zu:%s documented 0x%08lX expected 0x%08lX\n",
                       path, line_number, required->name, (unsigned long)documented_value,
                       (unsigned long)required->status);
                reported_value_problems[i] = 1;
                ++(*statuscode_value_problems);
            }
        }
    }

    fclose(file);

    for (size_t i = 0u; i < ARRAY_COUNT(feature_required_statuscode_values); ++i) {
        const statuscode_name_entry_t *required = &feature_required_statuscode_values[i];
        if (found_expected_values[i] || reported_value_problems[i]) {
            continue;
        }

        if (found_status_names[i]) {
            printf("Missing required StatusCode numeric value in %s: %s = 0x%08lX\n", path, required->name,
                   (unsigned long)required->status);
        } else {
            printf("Missing required StatusCode numeric documentation in %s: %s = 0x%08lX\n", path, required->name,
                   (unsigned long)required->status);
        }
        ++(*statuscode_value_problems);
    }
}

void test_conformance_claims_have_evidence(void) {
    size_t checked_files = 0u;
    size_t unsupported_claims = 0u;
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/docs", has_claim_wording, &checked_files, &unsupported_claims);
    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files, "Expected to scan Markdown docs");
    TEST_ASSERT_EQUAL_MESSAGE(0u, unsupported_claims, "Found unsupported conformance claims without evidence");
}

void test_profile_compliant_claims_require_external_ctt_evidence_per_opc_10000_7_4_2_and_4_3(void) {
    size_t checked_files = 0u;
    size_t unsupported_claims = 0u;
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/docs/conformance", has_profile_compliant_wording,
                                      &checked_files, &unsupported_claims);
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/docs/validation", has_profile_compliant_wording,
                                      &checked_files, &unsupported_claims);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files,
                                     "Expected to scan docs/conformance and docs/validation Markdown docs");
    TEST_ASSERT_EQUAL_MESSAGE(
        0u, unsupported_claims,
        "OPC-10000-7 sections 4.2 and 4.3 require profile-compliant claims to have external CTT evidence");
}

void test_ctt_verified_claims_require_external_ctt_evidence_per_opc_10000_7_4_2_and_4_3(void) {
    TEST_ASSERT_TRUE_MESSAGE(has_ctt_verified_wording("Status: CTT-verified for the embedded profile."),
                             "Expected hyphenated CTT-verified claims to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(has_ctt_verified_wording("Status: CTT verified for the embedded profile."),
                             "Expected spaced CTT verified claims to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(has_ctt_verified_wording("Status: CTT-certified for the embedded profile."),
                             "Expected hyphenated CTT-certified claims to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(has_ctt_verified_wording("Status: CTT certified for the embedded profile."),
                             "Expected spaced CTT certified claims to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(has_ctt_verified_wording("The server passed CTT for the embedded profile."),
                             "Expected passed CTT claims to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(
        has_ctt_verified_wording("The server is verified by the OPC Foundation Compliance Test Tool."),
        "Expected equivalent affirmative Compliance Test Tool claims to be scanned");

    TEST_ASSERT_FALSE_MESSAGE(claim_wording_is_supported("Status: CTT verified for the embedded profile."),
                              "Unsupported affirmative CTT claims require external evidence");
    TEST_ASSERT_TRUE_MESSAGE(
        claim_wording_is_supported("Status: not CTT verified; no external CTT evidence is available."),
        "Negative CTT wording must remain allowed");
    TEST_ASSERT_TRUE_MESSAGE(claim_wording_is_supported("Status: CTT verified with an external CTT evidence report."),
                             "Explicit external CTT evidence wording must remain allowed");
    TEST_ASSERT_FALSE_MESSAGE(claim_wording_is_supported("Status: profile-compliance claim."),
                              "Bare profile-compliance claim wording is not negative or policy context");
    TEST_ASSERT_FALSE_MESSAGE(claim_wording_is_supported("Status: profile compliance claim."),
                              "Bare profile compliance claim wording is not negative or policy context");

    size_t checked_files = 0u;
    size_t unsupported_claims = 0u;
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/docs/conformance", has_ctt_verified_wording, &checked_files,
                                      &unsupported_claims);
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/docs/validation", has_ctt_verified_wording, &checked_files,
                                      &unsupported_claims);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files,
                                     "Expected to scan docs/conformance and docs/validation Markdown docs");
    TEST_ASSERT_EQUAL_MESSAGE(
        0u, unsupported_claims,
        "OPC-10000-7 sections 4.2 and 4.3 require CTT-verified claims to have external CTT evidence");
}

void test_statuscode_names_in_conformance_docs_match_opc_10000_4_7_38_2(void) {
    size_t checked_files = 0u;
    size_t statuscode_name_problems = 0u;

    check_statuscode_doc_file(PROJECT_ROOT_DIR "/docs/conformance/status.md", 1, &checked_files,
                              &statuscode_name_problems);
    check_statuscode_doc_file(PROJECT_ROOT_DIR "/docs/traceability/statuscodes.md", 0, &checked_files,
                              &statuscode_name_problems);
    check_statuscode_doc_file(PROJECT_ROOT_DIR "/docs/traceability/020-audit-hardening.md", 0, &checked_files,
                              &statuscode_name_problems);
    check_statuscode_doc_file(PROJECT_ROOT_DIR "/docs/validation/audit-hardening.md", 0, &checked_files,
                              &statuscode_name_problems);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files, "Expected to scan StatusCode documentation files");
    TEST_ASSERT_EQUAL_MESSAGE(0u, statuscode_name_problems,
                              "OPC-10000-4 section 7.38.2 StatusCode names must match implementation names");
}

void test_statuscode_numeric_values_in_status_conformance_doc_match_implementation_per_opc_10000_4_7_38_2(void) {
    size_t checked_files = 0u;
    size_t statuscode_value_problems = 0u;

    check_statuscode_numeric_doc_file(PROJECT_ROOT_DIR "/docs/conformance/status.md", &checked_files,
                                      &statuscode_value_problems);

    TEST_ASSERT_EQUAL_MESSAGE(1u, checked_files, "Expected to scan docs/conformance/status.md");
    TEST_ASSERT_EQUAL_MESSAGE(
        0u, statuscode_value_problems,
        "OPC-10000-4 section 7.38.2 StatusCode numeric values must match implementation constants");
}

void test_public_docs_do_not_contain_stale_profile_support_claims_per_opc_10000_7_4_2_and_4_3(void) {
    assert_file_lacks_text(PROJECT_ROOT_DIR "/README.md", "remaining Micro item");
    assert_file_lacks_text(PROJECT_ROOT_DIR "/Makefile", "remaining Micro item");
    assert_file_lacks_text(PROJECT_ROOT_DIR "/README.md",
                           "Not implemented yet: History, NodeManagement, and aggregate subscriptions");
}

void test_conformance_docs_use_current_query_and_nodemanagement_sections(void) {
    const char *services = PROJECT_ROOT_DIR "/docs/conformance/services.md";

    TEST_ASSERT_TRUE_MESSAGE(file_contains_text(services, "| QueryFirst / QueryNext | B.2.3 / B.2.4 |"),
                             "Query services must cite OPC-10000-4 Appendix B sections B.2.3/B.2.4");
    TEST_ASSERT_TRUE_MESSAGE(
        file_contains_text(services, "| AddNodes / DeleteNodes / AddReferences / DeleteReferences | 5.8 |"),
        "NodeManagement services must cite OPC-10000-4 section 5.8");
}

void test_public_docs_do_not_use_stale_aggregate_nodeids_per_opc_10000_4_7_22_4_and_opc_10000_13(void) {
    static const char *const public_docs[] = {
        PROJECT_ROOT_DIR "/README.md",
        PROJECT_ROOT_DIR "/docs/api-reference.md",
        PROJECT_ROOT_DIR "/docs/integration-guide.md",
        PROJECT_ROOT_DIR "/docs/conformance/services.md",
        PROJECT_ROOT_DIR "/docs/conformance/status.md",
        PROJECT_ROOT_DIR "/docs/traceability/012-opcua-pubsub.md",
    };

    for (size_t i = 0u; i < ARRAY_COUNT(public_docs); ++i) {
        assert_file_lacks_stale_aggregate_nodeid_context(public_docs[i]);
    }
}

void test_public_size_numbers_are_snapshot_labeled_or_reproducible(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        line_has_public_profile_size_number("| **nano** | core profile | **15.9 KiB** (16,278 B) | 1,280 B | 0 |"),
        "Expected README profile rows with KiB numbers to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(line_has_public_profile_size_number(
                                 "| **Full Featured** | **38.8 KiB** (39,768 B) | 63,240 B + 16 KiB | **0** |"),
                             "Expected integration-guide profile rows with KiB numbers to be scanned");
    TEST_ASSERT_TRUE_MESSAGE(
        line_has_public_profile_size_number(
            "| nano | 16,278 B | 0 B | 0 B | 16,278 B | `build/size-arm/nano/src/libmicro_opcua.a` |"),
        "Expected size-ledger byte-only profile rows to be scanned");
    TEST_ASSERT_FALSE_MESSAGE(
        line_has_public_profile_size_number("`MU_MIN_CHUNK_SIZE` (8192 bytes) is the protocol floor"),
        "Protocol constants are not measured profile-size evidence");
    TEST_ASSERT_TRUE_MESSAGE(line_has_resource_evidence_context("Measured snapshot (2026-06-30, reproduce with"),
                             "Expected measured snapshot wording to satisfy resource evidence context");
    TEST_ASSERT_TRUE_MESSAGE(line_has_resource_evidence_context("Reproduce with `scripts/measure_size.sh all`."),
                             "Expected reproduction command wording to satisfy resource evidence context");

    TEST_ASSERT_TRUE_MESSAGE(file_contains_text(PROJECT_ROOT_DIR "/README.md", "Measured snapshot"),
                             "README profile numbers must be labeled as a measured snapshot");
    TEST_ASSERT_TRUE_MESSAGE(file_contains_text(PROJECT_ROOT_DIR "/README.md", "scripts/measure_size.sh all"),
                             "README profile numbers must link to a reproduction command");
    TEST_ASSERT_TRUE_MESSAGE(file_contains_text(PROJECT_ROOT_DIR "/docs/integration-guide.md", "Measured 2026"),
                             "Integration-guide size numbers must include the measurement date");
    TEST_ASSERT_TRUE_MESSAGE(
        file_contains_text(PROJECT_ROOT_DIR "/docs/integration-guide.md", "scripts/measure_size.sh all"),
        "Integration-guide size numbers must link to a reproduction command");

    size_t checked_files = 0u;
    size_t checked_resource_lines = 0u;
    size_t unlabeled_resource_numbers = 0u;

    check_file_for_unlabeled_resource_numbers(PROJECT_ROOT_DIR "/README.md", &checked_files, &checked_resource_lines,
                                              &unlabeled_resource_numbers);
    check_file_for_unlabeled_resource_numbers(PROJECT_ROOT_DIR "/docs/integration-guide.md", &checked_files,
                                              &checked_resource_lines, &unlabeled_resource_numbers);
    check_file_for_unlabeled_resource_numbers(PROJECT_ROOT_DIR "/docs/size/feature-size-ledger.md", &checked_files,
                                              &checked_resource_lines, &unlabeled_resource_numbers);

    TEST_ASSERT_EQUAL_MESSAGE(3u, checked_files, "Expected to scan public resource docs");
    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_resource_lines,
                                     "Expected to scan public profile-size resource numbers");
    TEST_ASSERT_EQUAL_MESSAGE(
        0u, unlabeled_resource_numbers,
        "Measured public profile-size numbers must be snapshot-labeled or linked to reproduction commands");
}

void test_pubsub_subscriber_docs_name_borrowed_input_buffer_lifetime(void) {
    static const char *const lifetime_docs[] = {
        PROJECT_ROOT_DIR "/include/micro_opcua/pubsub.h",
        PROJECT_ROOT_DIR "/docs/api-reference.md",
        PROJECT_ROOT_DIR "/docs/integration-guide.md",
        PROJECT_ROOT_DIR "/specs/023-conformance-docs-subscriber/contracts/pubsub-subscriber.md",
    };

    for (size_t i = 0u; i < ARRAY_COUNT(lifetime_docs); ++i) {
        TEST_ASSERT_TRUE_MESSAGE(file_contains_text(lifetime_docs[i], "input buffer must outlive"),
                                 "PubSub subscriber docs must state borrowed decoded-field buffer lifetime");
        assert_file_lacks_text(lifetime_docs[i], "does not retain pointers");
        assert_file_lacks_text(lifetime_docs[i], "does not retain buffer pointers");
    }
}

void test_services_doc_names_negative_path_evidence_tests(void) {
    const char *services = PROJECT_ROOT_DIR "/docs/conformance/services.md";
    static const char *const invalid_id_terms[] = {
        "tests/unit/test_subscriptions_errors.c",
        "5.13.3",
        "5.13.6",
        "5.14.3",
        "5.14.8",
        "Bad_MonitoredItemIdInvalid",
        "Bad_SubscriptionIdInvalid",
    };
    static const char *const publish_republish_terms[] = {
        "tests/integration/test_subscriptions.c",
        "5.14.5.4",
        "5.14.6.3",
        "Bad_SequenceNumberUnknown",
        "Bad_MessageNotAvailable",
    };
    static const char *const datachange_filter_terms[] = {
        "tests/unit/test_subscriptions_errors.c", "DataChangeFilter",  "7.22.2", "5.13.2", "5.13.3",
        "Bad_MonitoredItemFilterUnsupported",     "Bad_DecodingError",
    };
    static const char *const aggregate_filter_terms[] = {
        "tests/unit/test_aggregate.c",        "AggregateFilter",   "7.22.4",
        "Bad_MonitoredItemFilterUnsupported", "Bad_DecodingError",
    };
    static const doc_evidence_requirement_t requirements[] = {
        {"Subscription invalid MonitoredItemId/SubscriptionId negative paths", invalid_id_terms,
         ARRAY_COUNT(invalid_id_terms)},
        {"Publish acknowledgement and Republish invalid sequence negative paths", publish_republish_terms,
         ARRAY_COUNT(publish_republish_terms)},
        {"DataChangeFilter unsupported/malformed negative paths", datachange_filter_terms,
         ARRAY_COUNT(datachange_filter_terms)},
        {"AggregateFilter unsupported/malformed negative paths", aggregate_filter_terms,
         ARRAY_COUNT(aggregate_filter_terms)},
    };

    size_t missing_evidence = 0u;
    for (size_t i = 0u; i < ARRAY_COUNT(requirements); ++i) {
        if (!services_doc_has_evidence_block(services, &requirements[i])) {
            print_missing_services_doc_evidence(services, &requirements[i]);
            ++missing_evidence;
        }
    }

    TEST_ASSERT_EQUAL_MESSAGE(
        0u, missing_evidence,
        "docs/conformance/services.md must name the tests, OPC-10000-4 refs, and StatusCodes covering selected "
        "Subscription/DataChange/Aggregate negative paths");
}

void test_fuzz_harnesses_do_not_contain_todo_placeholder_markers_per_fr_020_sc_006(void) {
    TEST_ASSERT_NOT_NULL_MESSAGE(matching_fuzz_placeholder_marker("/* TODO */"),
                                 "Expected TODO markers to be rejected");
    TEST_ASSERT_NOT_NULL_MESSAGE(matching_fuzz_placeholder_marker("/* To be implemented in T148 */"),
                                 "Expected deferred implementation markers to be rejected");
    TEST_ASSERT_NOT_NULL_MESSAGE(matching_fuzz_placeholder_marker("/* placeholder harness */"),
                                 "Expected placeholder markers to be rejected");

    size_t checked_files = 0u;
    size_t placeholder_markers = 0u;
    check_fuzz_tree_for_placeholder_markers(PROJECT_ROOT_DIR "/tests/fuzz", &checked_files, &placeholder_markers);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files, "Expected to scan fuzz harness C files");
    TEST_ASSERT_EQUAL_MESSAGE(
        0u, placeholder_markers,
        "FR-020/SC-006 require fuzz gates to reject TODO placeholder harness text before validation claims pass");
}

void test_fuzz_harnesses_do_not_discard_or_ignore_input_per_fr_020_sc_006(void) {
    TEST_ASSERT_TRUE_MESSAGE(line_has_fuzz_input_discard("(void)data;"),
                             "Expected lowercase data discard to be rejected");
    TEST_ASSERT_TRUE_MESSAGE(line_has_fuzz_input_discard("(void)size;"),
                             "Expected lowercase size discard to be rejected");
    TEST_ASSERT_TRUE_MESSAGE(line_has_fuzz_input_discard("(void)Data;"),
                             "Expected uppercase Data discard to be rejected");
    TEST_ASSERT_TRUE_MESSAGE(line_has_fuzz_input_discard("(void)Size;"),
                             "Expected uppercase Size discard to be rejected");
    TEST_ASSERT_TRUE_MESSAGE(line_has_return_zero_statement("return 0;"),
                             "Expected immediate return-zero placeholder bodies to be scanned");

    size_t checked_files = 0u;
    size_t unused_input_placeholders = 0u;
    check_fuzz_tree_for_unused_input_placeholders(PROJECT_ROOT_DIR "/tests/fuzz", &checked_files,
                                                  &unused_input_placeholders);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files, "Expected to scan fuzz harness C files");
    TEST_ASSERT_EQUAL_MESSAGE(0u, unused_input_placeholders,
                              "FR-020/SC-006 require fuzz gates to reject harnesses that discard or ignore fuzzer "
                              "input before validation claims pass");
}

void test_docs_and_specs_do_not_claim_profile_compliance_without_ctt_evidence(void) {
    size_t checked_files = 0u;
    size_t unsupported_claims = 0u;
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/docs", has_claim_wording, &checked_files, &unsupported_claims);
    check_tree_for_unsupported_claims(PROJECT_ROOT_DIR "/specs", has_claim_wording, &checked_files,
                                      &unsupported_claims);

    TEST_ASSERT_GREATER_THAN_MESSAGE(0u, checked_files, "Expected to scan Markdown docs and specs");
    TEST_ASSERT_EQUAL_MESSAGE(0u, unsupported_claims, "Found profile-compliance claims without CTT evidence");
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_conformance_claims_have_evidence);
    RUN_TEST(test_profile_compliant_claims_require_external_ctt_evidence_per_opc_10000_7_4_2_and_4_3);
    RUN_TEST(test_ctt_verified_claims_require_external_ctt_evidence_per_opc_10000_7_4_2_and_4_3);
    RUN_TEST(test_statuscode_names_in_conformance_docs_match_opc_10000_4_7_38_2);
    RUN_TEST(test_statuscode_numeric_values_in_status_conformance_doc_match_implementation_per_opc_10000_4_7_38_2);
    RUN_TEST(test_public_docs_do_not_contain_stale_profile_support_claims_per_opc_10000_7_4_2_and_4_3);
    RUN_TEST(test_conformance_docs_use_current_query_and_nodemanagement_sections);
    RUN_TEST(test_public_docs_do_not_use_stale_aggregate_nodeids_per_opc_10000_4_7_22_4_and_opc_10000_13);
    RUN_TEST(test_public_size_numbers_are_snapshot_labeled_or_reproducible);
    RUN_TEST(test_pubsub_subscriber_docs_name_borrowed_input_buffer_lifetime);
    RUN_TEST(test_services_doc_names_negative_path_evidence_tests);
    RUN_TEST(test_fuzz_harnesses_do_not_contain_todo_placeholder_markers_per_fr_020_sc_006);
    RUN_TEST(test_fuzz_harnesses_do_not_discard_or_ignore_input_per_fr_020_sc_006);
    RUN_TEST(test_docs_and_specs_do_not_claim_profile_compliance_without_ctt_evidence);
    return UNITY_END();
}
