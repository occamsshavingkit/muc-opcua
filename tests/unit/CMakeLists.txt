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
    if(TARGET micro_opcua)
        target_link_libraries(${test_name} PRIVATE micro_opcua)
    endif()
    if(TARGET micro_opcua_test_support)
        target_link_libraries(${test_name} PRIVATE micro_opcua_test_support)
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Discover all test files
file(GLOB UNIT_TEST_SOURCES "test_*.c")

foreach(TEST_SOURCE ${UNIT_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    
    set(RUN_TEST ON)
    if(TEST_NAME STREQUAL "test_node_management" OR TEST_NAME STREQUAL "test_node_management_errors")
        if(NOT MICRO_OPCUA_SERVICE_NODEMANAGEMENT)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_query_service" OR TEST_NAME STREQUAL "test_query_encoding")
        if(NOT MICRO_OPCUA_SERVICE_QUERY)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_history")
        if(NOT MICRO_OPCUA_SERVICE_HISTORY)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_alarms_conditions")
        if(NOT MICRO_OPCUA_EVENTS)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_pubsub" OR TEST_NAME STREQUAL "test_uadp_encoding")
        if(NOT MICRO_OPCUA_PUBSUB)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_aggregate")
        if(NOT MICRO_OPCUA_SUBSCRIPTIONS_STANDARD)
            set(RUN_TEST OFF)
        endif()
    elseif(TEST_NAME STREQUAL "test_write_decoder" OR TEST_NAME STREQUAL "test_write_errors" OR TEST_NAME STREQUAL "test_write_service")
        if(NOT MICRO_OPCUA_SERVICE_WRITE)
            set(RUN_TEST OFF)
        endif()
    endif()

    if(RUN_TEST)
        add_unity_test(${TEST_NAME} ${TEST_SOURCE})
    endif()
endforeach()

add_test(NAME test_size_report_script COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/measure_size.sh)
