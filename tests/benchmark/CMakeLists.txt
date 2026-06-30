# tests/benchmark/CMakeLists.txt

add_executable(audit_latency_benchmark audit_latency_benchmark.c)
target_link_libraries(audit_latency_benchmark PRIVATE micro_opcua micro_opcua_test_support)
target_include_directories(audit_latency_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/support
)

add_executable(hotpath_benchmark hotpath_benchmark.c)
target_link_libraries(hotpath_benchmark PRIVATE micro_opcua micro_opcua_test_support)
target_include_directories(hotpath_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/tests/support
)
