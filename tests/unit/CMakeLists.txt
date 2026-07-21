# tests/unit/CMakeLists.txt

function(add_unity_test test_name test_source)
    add_executable(${test_name} ${test_source})
    
    # Unity is provided by FetchContent in the parent directory
    target_link_libraries(${test_name} PRIVATE unity)
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
    # White-box unit tests may reach internal headers (e.g. the host crypto adapter).
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_compile_definitions(${test_name} PRIVATE PROJECT_ROOT_DIR="${CMAKE_SOURCE_DIR}")
    
    # The core library will be added later
    if(TARGET muc_opcua)
        target_link_libraries(${test_name} PRIVATE muc_opcua)
    endif()
    if(TARGET muc_opcua_test_support)
        target_link_libraries(${test_name} PRIVATE muc_opcua_test_support)
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Discover all test files
file(GLOB UNIT_TEST_SOURCES "test_*.c")

foreach(TEST_SOURCE ${UNIT_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    set(RUN_TEST ON)
    if(TEST_NAME STREQUAL "test_node_management" OR TEST_NAME STREQUAL "test_node_management_errors" OR
       TEST_NAME STREQUAL "test_address_space_dynamic")
        # These force-#define MUC_OPCUA_CU_NODEMANAGEMENT 1 before including
        # server_internal.h so the test's own translation unit can see the
        # NodeManagement-only struct fields/prototypes -- safe ONLY when the
        # muc_opcua library itself is also built with this option on; otherwise
        # struct mu_server's layout and mu_browse_process_with_user_index's
        # signature disagree between the test and the library, corrupting the
        # call (this crashed test_address_space_dynamic before it was added here).
        if(NOT MUC_OPCUA_CU_NODEMANAGEMENT)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_query_service" OR TEST_NAME STREQUAL "test_query_encoding")
        if(NOT MUC_OPCUA_CU_QUERY)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_history")
        if(NOT MUC_OPCUA_CU_HISTORICAL_ACCESS_SERVER_FACET)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_alarms_conditions")
        if(NOT MUC_OPCUA_CU_EVENTS)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_pubsub" OR TEST_NAME STREQUAL "test_uadp_encoding")
        if(NOT MUC_OPCUA_CU_PUBSUB)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_aggregate")
        if(NOT MUC_OPCUA_CU_SUBSCRIPTION_STANDARD)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_write_decoder" OR TEST_NAME STREQUAL "test_write_service")
        if(NOT MUC_OPCUA_WRITE_VALUE_ENABLED)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_cert_token_decoder" OR TEST_NAME STREQUAL "test_asym_chunk" OR
           TEST_NAME STREQUAL "test_certificate" OR TEST_NAME STREQUAL "test_crypto" OR
           TEST_NAME STREQUAL "test_mbedtls_adapter" OR TEST_NAME STREQUAL "test_security_rsa_oaep" OR
           TEST_NAME STREQUAL "test_security_rsa_oaep_errors" OR TEST_NAME STREQUAL "test_security_rsa_pss" OR
           TEST_NAME STREQUAL "test_security_rsa_pss_errors" OR TEST_NAME STREQUAL "test_security_trustlist" OR
           TEST_NAME STREQUAL "test_sym_chunk" OR TEST_NAME STREQUAL "test_wolfssl_adapter" OR
           TEST_NAME STREQUAL "test_security_policy_selection" OR TEST_NAME STREQUAL "test_certificate_validity")
        # These exercise the secure-channel crypto TUs (sym/asym chunk,
        # certificate, trustlist) and the host crypto adapter, now compiled under
        # MUC_OPCUA_SECURE_CHANNEL_CRYPTO (spec 072), not the facet umbrella -- see
        # src/CMakeLists.txt's `if(MUC_OPCUA_SECURE_CHANNEL_CRYPTO)` block. Gated
        # off (with the crypto) on the SecurityPolicy-None-only nano profile.
        if(NOT MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_ecc_crypto")
        # ECC SecurityPolicy primitives (spec 059): only meaningful when the ECC
        # feature is built (CU_SECURITY_ECC).
        if(NOT MUC_OPCUA_CU_SECURITY_ECC)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_user_token_decoder")
        # mu_binary_read_username_identity_token is gated on
        # `defined(MUC_OPCUA_USER_AUTH) || defined(MUC_OPCUA_SECURITY)`
        # (encoding/binary_reader.c).  Security code is always compiled now
        # (FACET_CORE_2022_SERVER), so this test runs when either is on.
        if(NOT MUC_OPCUA_CU_USER_AUTH AND NOT MUC_OPCUA_FACET_CORE_2022_SERVER)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_password_decrypt_zeroize")
        # Decrypts an encrypted UserName token password -> needs the secure-channel
        # crypto stack (spec 072). mu_secure_zero itself is now always compiled
        # (secure_util.c), but the decrypt path is crypto-gated.
        if(NOT MUC_OPCUA_SECURE_CHANNEL_CRYPTO)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_read_browsename_namespace" OR
           TEST_NAME STREQUAL "test_read_timestamps_to_return")
        # Exercise browse/read service APIs.
        if(NOT MUC_OPCUA_CU_ATTRIBUTE_READ)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_nano_client")
        if(NOT MUC_OPCUA_CLIENT_NANO)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_time_sync")
        # Exercises mu_opn_time_sync_allows (osc_handler.c), which is compiled
        # only under MUC_OPCUA_CU_TIME_SYNC (spec 075). Enabled on every profile
        # today, but gate defensively so a custom/flag-off build does not fail to
        # link the test against the compiled-out helper.
        if(NOT MUC_OPCUA_CU_TIME_SYNC)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_key_credential")
        # spec 094 (CU 2113): the KeyCredential handlers are compiled only
        # under MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE (which itself depends on
        # MUC_OPCUA_CU_METHOD_SERVER). Skip on profiles without the facet.
        if(NOT MUC_OPCUA_CU_KEY_CREDENTIAL_SERVICE)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_role_management")
        if(NOT MUC_OPCUA_CU_USER_ROLE_MANAGEMENT)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_certificate_management")
        if(NOT MUC_OPCUA_CU_CERTIFICATE_MANAGEMENT)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_certificate_manager")
        if(NOT MUC_OPCUA_CU_CERTIFICATE_MANAGER_PULL)
            set(RUN_TEST OFF)
        endif()
    endif()

    if(RUN_TEST)
        add_unity_test(${TEST_NAME} ${TEST_SOURCE})
        if(TEST_NAME STREQUAL "test_write_value_gate" AND MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES)
            target_compile_definitions(${TEST_NAME} PRIVATE MUC_OPCUA_CU_ATTRIBUTE_WRITE_VALUES=1)
        endif()
        if(TEST_NAME STREQUAL "test_history_memstore")
            target_sources(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/src/services/history_memstore.c)
        endif()
    endif()
endforeach()

add_test(NAME test_size_report_script COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/measure_size.sh)
