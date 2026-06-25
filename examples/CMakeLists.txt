# examples/CMakeLists.txt
add_executable(minimal_server minimal_server/main.c)
target_link_libraries(minimal_server PRIVATE micro_opcua)
