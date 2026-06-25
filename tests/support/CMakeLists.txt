# tests/support/CMakeLists.txt
add_library(micro_opcua_test_support STATIC
    fake_platform.c
)

target_include_directories(micro_opcua_test_support PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(micro_opcua_test_support PUBLIC micro_opcua)
