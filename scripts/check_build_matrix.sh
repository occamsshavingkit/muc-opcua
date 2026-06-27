#!/usr/bin/env bash
set -euo pipefail

SCRIPT_NAME=$(basename "$0")
SCRIPT_DIR=$(CDPATH= cd "$(dirname "$0")" && pwd -P)
ROOT_DIR=$(CDPATH= cd "$SCRIPT_DIR/.." && pwd -P)

CMAKE_BIN=${CMAKE:-cmake}
NM_BIN=${NM:-nm}

toggles=()
summary_options=()
summary_statuses=()
summary_details=()
matrix_root=""
have_nm=0
found_toggles_in_options_file=0

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME [--help]

Configures and builds the micro_opcua static library once per optional service
or feature toggle, with exactly one toggle forced OFF per build.

Environment:
  CMAKE      CMake executable to use (default: cmake)
  BUILD_DIR  Parent directory for temporary build directories (default: TMPDIR or /tmp)
  NM         nm executable for optional symbol absence checks (default: nm)

CMake also honors standard environment overrides such as CMAKE_GENERATOR and
CMAKE_BUILD_PARALLEL_LEVEL.
EOF
}

fail_usage() {
    echo "error: $*" >&2
    usage >&2
    exit 2
}

cleanup() {
    local status=$?

    if [ -n "${matrix_root:-}" ] && [ -d "$matrix_root" ]; then
        rm -rf "$matrix_root" || echo "warning: failed to remove temporary build root: $matrix_root" >&2
    fi

    return "$status"
}

trap cleanup EXIT

is_matrix_toggle() {
    case "$1" in
        MICRO_OPCUA_SERVICE_*|MICRO_OPCUA_SUBSCRIPTIONS|MICRO_OPCUA_SECURITY)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

contains_toggle() {
    local needle=$1
    local item

    for item in "${toggles[@]}"; do
        if [ "$item" = "$needle" ]; then
            return 0
        fi
    done

    return 1
}

discover_toggles() {
    local option_file="$ROOT_DIR/cmake/MicroOpcUaOptions.cmake"
    local top_file="$ROOT_DIR/CMakeLists.txt"
    local file
    local line
    local option_name

    for file in "$option_file" "$top_file"; do
        [ -f "$file" ] || continue

        while IFS= read -r line || [ -n "$line" ]; do
            if [[ $line =~ ^[[:space:]]*[Oo][Pp][Tt][Ii][Oo][Nn][[:space:]]*\([[:space:]]*([A-Za-z_][A-Za-z0-9_]*) ]]; then
                option_name=${BASH_REMATCH[1]}
                if is_matrix_toggle "$option_name" && ! contains_toggle "$option_name"; then
                    toggles+=("$option_name")
                    if [ "$file" = "$option_file" ]; then
                        found_toggles_in_options_file=1
                    fi
                fi
            fi
        done < "$file"
    done
}

join_by() {
    local separator=$1
    shift
    local output=""
    local item

    for item in "$@"; do
        if [ -z "$output" ]; then
            output=$item
        else
            output="$output$separator$item"
        fi
    done

    printf '%s' "$output"
}

symbols_for_option() {
    case "$1" in
        MICRO_OPCUA_SERVICE_READ)
            printf '%s\n' \
                handle_read \
                mu_read_request_decode \
                mu_read_process \
                mu_read_response_encode
            ;;
        MICRO_OPCUA_SERVICE_BROWSE)
            printf '%s\n' \
                handle_browse \
                handle_browse_next \
                handle_translate_browse_paths \
                mu_browse_request_decode \
                mu_browse_process \
                mu_browse_response_encode
            ;;
        MICRO_OPCUA_SERVICE_DISCOVERY)
            printf '%s\n' \
                handle_find_servers \
                handle_get_endpoints
            ;;
        MICRO_OPCUA_SERVICE_REGISTER_NODES)
            printf '%s\n' \
                handle_register_nodes \
                handle_unregister_nodes
            ;;
        MICRO_OPCUA_SUBSCRIPTIONS)
            printf '%s\n' \
                handle_create_subscription \
                handle_publish \
                handle_republish \
                mu_subscriptions_init \
                mu_subscription_create \
                mu_publish_request_enqueue
            ;;
        MICRO_OPCUA_SECURITY)
            printf '%s\n' \
                handle_data_chunk_secure \
                mu_asym_chunk_wrap \
                mu_asym_chunk_unwrap \
                mu_sym_chunk_wrap \
                mu_sym_chunk_unwrap \
                mu_p_sha256
            ;;
    esac
}

find_static_library() {
    local build_dir=$1
    local candidate

    while IFS= read -r -d '' candidate; do
        printf '%s\n' "$candidate"
        return 0
    done < <(find "$build_dir" -type f \( -name 'libmicro_opcua.a' -o -name 'micro_opcua.lib' \) -print0)

    return 1
}

symbol_exists_in_nm_output() {
    local nm_output=$1
    local symbol=$2

    awk -v want="$symbol" '
        NF > 0 && ($NF == want || $NF == "_" want) {
            found = 1
        }
        END {
            exit found ? 0 : 1
        }
    ' "$nm_output"
}

SYMBOL_NOTE=""

check_symbols_absent() {
    local option_name=$1
    local build_dir=$2
    local library
    local nm_output
    local nm_error
    local symbol
    local symbols=()
    local found_symbols=()

    while IFS= read -r symbol; do
        [ -n "$symbol" ] && symbols+=("$symbol")
    done < <(symbols_for_option "$option_name")

    if [ "${#symbols[@]}" -eq 0 ]; then
        SYMBOL_NOTE="WARN no symbol mapping for $option_name; skipped nm check"
        return 2
    fi

    if [ "$have_nm" -ne 1 ]; then
        SYMBOL_NOTE="WARN $NM_BIN not found; skipped nm check for $(join_by ', ' "${symbols[@]}")"
        return 2
    fi

    if ! library=$(find_static_library "$build_dir"); then
        SYMBOL_NOTE="WARN static library not found; skipped nm check for $(join_by ', ' "${symbols[@]}")"
        return 2
    fi

    nm_output="$build_dir/micro_opcua.nm"
    nm_error="$build_dir/micro_opcua.nm.err"
    if ! "$NM_BIN" "$library" >"$nm_output" 2>"$nm_error"; then
        SYMBOL_NOTE="WARN $NM_BIN failed on $library; skipped nm check"
        return 2
    fi

    for symbol in "${symbols[@]}"; do
        if symbol_exists_in_nm_output "$nm_output" "$symbol"; then
            found_symbols+=("$symbol")
        fi
    done

    if [ "${#found_symbols[@]}" -gt 0 ]; then
        SYMBOL_NOTE="FAIL symbols still present: $(join_by ', ' "${found_symbols[@]}")"
        return 1
    fi

    SYMBOL_NOTE="PASS symbols absent: $(join_by ', ' "${symbols[@]}")"
    return 0
}

record_result() {
    summary_options+=("$1")
    summary_statuses+=("$2")
    summary_details+=("$3")
}

print_log_tail() {
    local log_file=$1

    if [ -f "$log_file" ]; then
        echo "---- last 80 lines of $log_file ----" >&2
        tail -n 80 "$log_file" >&2 || true
        echo "---- end log tail ----" >&2
    fi
}

run_one_config() {
    local option_name=$1
    local build_dir="$matrix_root/$option_name"
    local configure_log="$build_dir/configure.log"
    local build_log="$build_dir/build.log"
    local configure_args
    local detail
    local symbol_status

    mkdir -p "$build_dir"

    configure_args=(
        -S "$ROOT_DIR"
        -B "$build_dir"
        -DMICRO_OPCUA_PLATFORM=host
        -DMICRO_OPCUA_BUILD_TESTS=OFF
        -DMICRO_OPCUA_BUILD_EXAMPLES=OFF
        -DMICRO_OPCUA_BUILD_FUZZERS=OFF
        -DCMAKE_COMPILE_WARNING_AS_ERROR=ON
        "-D${option_name}=OFF"
    )

    echo
    echo "=== $option_name=OFF ==="

    if "$CMAKE_BIN" "${configure_args[@]}" >"$configure_log" 2>&1; then
        echo "configure: PASS"
    else
        echo "configure: FAIL"
        print_log_tail "$configure_log"
        record_result "$option_name" "FAIL" "configure failed"
        return 1
    fi

    if "$CMAKE_BIN" --build "$build_dir" --target micro_opcua >"$build_log" 2>&1; then
        echo "build: PASS"
    else
        echo "build: FAIL"
        print_log_tail "$build_log"
        record_result "$option_name" "FAIL" "build failed"
        return 1
    fi

    symbol_status=0
    check_symbols_absent "$option_name" "$build_dir" || symbol_status=$?
    echo "symbol: $SYMBOL_NOTE"

    case "$symbol_status" in
        0)
            detail=$SYMBOL_NOTE
            record_result "$option_name" "PASS" "$detail"
            echo "result: PASS"
            return 0
            ;;
        1)
            detail=$SYMBOL_NOTE
            record_result "$option_name" "FAIL" "$detail"
            echo "result: FAIL"
            return 1
            ;;
        *)
            detail=$SYMBOL_NOTE
            record_result "$option_name" "PASS" "$detail"
            echo "result: PASS with warning"
            return 0
            ;;
    esac
}

print_summary() {
    local index
    local failed=0

    echo
    echo "Build matrix summary:"
    for index in "${!summary_options[@]}"; do
        printf '  %-5s %s=OFF (%s)\n' \
            "${summary_statuses[$index]}" \
            "${summary_options[$index]}" \
            "${summary_details[$index]}"
        if [ "${summary_statuses[$index]}" = "FAIL" ]; then
            failed=$((failed + 1))
        fi
    done

    if [ "$failed" -eq 0 ]; then
        echo "Overall: PASS (${#summary_options[@]} configurations)"
    else
        echo "Overall: FAIL ($failed of ${#summary_options[@]} configurations failed)"
    fi
}

main() {
    local arg
    local build_parent
    local option_name
    local overall=0

    for arg in "$@"; do
        case "$arg" in
            -h|--help)
                usage
                exit 0
                ;;
            *)
                fail_usage "unknown argument: $arg"
                ;;
        esac
    done

    [ -f "$ROOT_DIR/CMakeLists.txt" ] || {
        echo "error: cannot find repository CMakeLists.txt from $SCRIPT_DIR" >&2
        exit 2
    }

    if ! command -v "$CMAKE_BIN" >/dev/null 2>&1; then
        echo "error: CMake executable not found: $CMAKE_BIN" >&2
        exit 2
    fi

    if command -v "$NM_BIN" >/dev/null 2>&1; then
        have_nm=1
    else
        echo "warning: nm executable not found: $NM_BIN; symbol checks will be skipped" >&2
    fi

    discover_toggles
    if [ "${#toggles[@]}" -eq 0 ]; then
        echo "error: no optional MICRO_OPCUA service/feature toggles discovered" >&2
        exit 2
    fi

    if [ "$found_toggles_in_options_file" -eq 0 ]; then
        echo "warning: no matrix toggles found in cmake/MicroOpcUaOptions.cmake; using top-level CMake option definitions" >&2
    fi

    build_parent=${BUILD_DIR:-${TMPDIR:-/tmp}}
    mkdir -p "$build_parent"
    matrix_root=$(mktemp -d "$build_parent/micro-opcua-build-matrix.XXXXXX")

    echo "Discovered ${#toggles[@]} toggles: $(join_by ', ' "${toggles[@]}")"
    echo "Temporary build root: $matrix_root"

    for option_name in "${toggles[@]}"; do
        if ! run_one_config "$option_name"; then
            overall=1
        fi
    done

    print_summary
    exit "$overall"
}

main "$@"
