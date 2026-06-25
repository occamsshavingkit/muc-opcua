# tests/unit/CMakeLists.txt

function(add_unity_test test_name test_source)
    add_executable(${test_name} ${test_source})
    
    # Unity is provided by FetchContent in the parent directory
    target_link_libraries(${test_name} PRIVATE unity)
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/tests/support)
    
    # The core library will be added later
    if(TARGET micro_opcua_core)
        target_link_libraries(${test_name} PRIVATE micro_opcua_core)
    endif()
    
    # Register with CTest
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

# Discover all test files
file(GLOB UNIT_TEST_SOURCES "test_*.c")

foreach(TEST_SOURCE ${UNIT_TEST_SOURCES})
    get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
    add_unity_test(${TEST_NAME} ${TEST_SOURCE})
endforeach()
