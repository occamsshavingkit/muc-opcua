#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: check_stack_usage.sh --build-dir <dir> [--threshold <bytes>]

Aggregates compiler-generated .su files from a CMake build compiled with
-fstack-usage and estimates the secured OpenSecureChannel request path stack.
EOF
}

fail_usage() {
    echo "error: $*" >&2
    usage >&2
    exit 2
}

build_dir=""
threshold=10240

while [ "$#" -gt 0 ]; do
    case "$1" in
        --build-dir)
            [ "$#" -ge 2 ] || fail_usage "--build-dir requires a value"
            build_dir=$2
            shift 2
            ;;
        --threshold)
            [ "$#" -ge 2 ] || fail_usage "--threshold requires a value"
            threshold=$2
            shift 2
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            fail_usage "unknown argument: $1"
            ;;
    esac
done

[ -n "$build_dir" ] || fail_usage "--build-dir is required"
[ -d "$build_dir" ] || fail_usage "build directory does not exist: $build_dir"
case "$threshold" in
    ''|*[!0-9]*)
        fail_usage "--threshold must be a non-negative integer"
        ;;
esac

su_files=()
while IFS= read -r -d '' file; do
    su_files+=("$file")
done < <(find "$build_dir" -type f -name '*.su' -print0)

if [ "${#su_files[@]}" -eq 0 ]; then
    echo "error: no .su files found under $build_dir; configure with MICRO_OPCUA_STACK_USAGE=ON and rebuild" >&2
    exit 2
fi

# Measurement method:
# - .su files report a stack frame per function, not a complete call graph.
# - For the secured OPN request path, handle_data_chunk_secure is always live
#   while the request is unwrapped, dispatched, and wrapped.
# - The OPN path has three sequential phases, so their stacks are not added
#   together. Instead, estimate:
#     handle_data_chunk_secure + max(asym unwrap/wrap phase,
#                                    service dispatch/OpenSecureChannel phase)
# - The asymmetric phase uses the larger of mu_asym_chunk_unwrap and
#   mu_asym_chunk_wrap plus the largest local helper frame in asym_chunk.c.
# - The dispatch phase includes mu_service_dispatch, handle_open_secure_channel,
#   and the larger nested OpenSecureChannel helper chain. Symmetric key
#   derivation includes both mu_sym_keys_derive and mu_p_sha256 because those
#   frames are live together while deriving Basic256Sha256 channel keys.
# - Crypto adapter callbacks are outside this library's stack budget; their
#   implementation must be measured separately by the platform integrator.
# - If an internal static root helper is optimized away, the emitted frame is
#   treated as 0 and reported; other required emitted functions still fail closed.

frame_for() {
    local source_suffix=$1
    local function_name=$2

    awk -v suffix="$source_suffix" -v want="$function_name" '
        function ends_with(value, suffix) {
            return length(value) >= length(suffix) &&
                   substr(value, length(value) - length(suffix) + 1) == suffix
        }
        function function_matches(value, want) {
            return value == want ||
                   index(value, want ".") == 1 ||
                   index(value, want "$") == 1
        }
        BEGIN { max = -1 }
        {
            field_count = split($0, fields, "\t")
            if (field_count < 2 || fields[2] !~ /^[0-9]+$/) {
                next
            }

            site = fields[1]
            function_seen = site
            sub(/^.*:/, "", function_seen)

            source = site
            sub(/:[^:]*$/, "", source)
            sub(/:[^:]*$/, "", source)
            sub(/:[^:]*$/, "", source)

            if (ends_with(source, suffix) && function_matches(function_seen, want)) {
                bytes = fields[2] + 0
                if (bytes > max) {
                    max = bytes
                }
            }
        }
        END {
            if (max >= 0) {
                print max
            }
        }
    ' "${su_files[@]}"
}

require_frame() {
    local source_suffix=$1
    local function_name=$2
    local value

    value=$(frame_for "$source_suffix" "$function_name")
    if [ -z "$value" ]; then
        echo "error: missing stack-usage entry for $source_suffix:$function_name" >&2
        exit 2
    fi
    printf '%s\n' "$value"
}

optional_frame() {
    local source_suffix=$1
    local function_name=$2
    local value

    value=$(frame_for "$source_suffix" "$function_name")
    if [ -z "$value" ]; then
        value=0
    fi
    printf '%s\n' "$value"
}

max_value() {
    local max=0
    local value

    for value in "$@"; do
        if [ "$value" -gt "$max" ]; then
            max=$value
        fi
    done

    printf '%s\n' "$max"
}

root_frame=$(frame_for "src/core/server.c" "handle_data_chunk_secure")
root_frame_note=""
if [ -z "$root_frame" ]; then
    root_frame=0
    root_frame_note=" (not emitted)"
fi
service_dispatch_frame=$(require_frame "src/core/service_dispatch.c" "mu_service_dispatch")
open_secure_channel_frame=$(require_frame "src/core/service_dispatch.c" "handle_open_secure_channel")
secure_channel_open_frame=$(require_frame "src/services/secure_channel.c" "mu_secure_channel_open")
asym_unwrap_frame=$(require_frame "src/security/asym_chunk.c" "mu_asym_chunk_unwrap")
asym_wrap_frame=$(require_frame "src/security/asym_chunk.c" "mu_asym_chunk_wrap")
sym_keys_derive_frame=$(require_frame "src/security/sym_chunk.c" "mu_sym_keys_derive")
kdf_frame=$(require_frame "src/security/key_derivation.c" "mu_p_sha256")

asym_key_bytes_frame=$(optional_frame "src/security/asym_chunk.c" "key_bytes")
asym_write_header_frame=$(optional_frame "src/security/asym_chunk.c" "write_header")
asym_wrap_none_frame=$(optional_frame "src/security/asym_chunk.c" "wrap_none")

asym_direct_frame=$(max_value "$asym_unwrap_frame" "$asym_wrap_frame")
asym_helper_frame=$(max_value "$asym_key_bytes_frame" "$asym_write_header_frame" "$asym_wrap_none_frame")
asym_phase=$((root_frame + asym_direct_frame + asym_helper_frame))

sym_kdf_chain=$((sym_keys_derive_frame + kdf_frame))
dispatch_nested_frame=$(max_value "$secure_channel_open_frame" "$sym_kdf_chain")
dispatch_phase=$((root_frame + service_dispatch_frame + open_secure_channel_frame + dispatch_nested_frame))

estimate=$(max_value "$asym_phase" "$dispatch_phase")

echo "Secured OPN stack estimate: ${estimate} bytes"
echo "Threshold: ${threshold} bytes"
echo "Method: handle_data_chunk_secure + max(asymmetric unwrap/wrap phase, OpenSecureChannel dispatch/KDF phase)"
echo "Frames:"
printf '  %-42s %8d%s\n' "src/core/server.c:handle_data_chunk_secure" "$root_frame" "$root_frame_note"
printf '  %-42s %8d\n' "src/security/asym_chunk.c:mu_asym_chunk_unwrap" "$asym_unwrap_frame"
printf '  %-42s %8d\n' "src/security/asym_chunk.c:mu_asym_chunk_wrap" "$asym_wrap_frame"
printf '  %-42s %8d\n' "src/security/asym_chunk.c:max_helper" "$asym_helper_frame"
printf '  %-42s %8d\n' "src/core/service_dispatch.c:mu_service_dispatch" "$service_dispatch_frame"
printf '  %-42s %8d\n' "src/core/service_dispatch.c:handle_open_secure_channel" "$open_secure_channel_frame"
printf '  %-42s %8d\n' "src/services/secure_channel.c:mu_secure_channel_open" "$secure_channel_open_frame"
printf '  %-42s %8d\n' "src/security/sym_chunk.c:mu_sym_keys_derive" "$sym_keys_derive_frame"
printf '  %-42s %8d\n' "src/security/key_derivation.c:mu_p_sha256" "$kdf_frame"
echo "Phases:"
printf '  %-42s %8d\n' "asymmetric unwrap/wrap" "$asym_phase"
printf '  %-42s %8d\n' "OpenSecureChannel dispatch/KDF" "$dispatch_phase"

if [ "$estimate" -gt "$threshold" ]; then
    echo "FAIL: secured OPN stack estimate exceeds threshold" >&2
    exit 1
fi

echo "PASS: secured OPN stack estimate is within threshold"
