# tests/fuzz/CMakeLists.txt

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzzing requires Clang compiler")
    return()
endif()

add_executable(fuzz_binary_reader fuzz_binary_reader.c)
target_link_libraries(fuzz_binary_reader PRIVATE muc_opcua)
target_compile_options(fuzz_binary_reader PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_binary_reader PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_message_chunk fuzz_message_chunk.c)
target_link_libraries(fuzz_message_chunk PRIVATE muc_opcua)
target_compile_options(fuzz_message_chunk PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_message_chunk PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_binary_types fuzz_binary_types.c)
target_link_libraries(fuzz_binary_types PRIVATE muc_opcua)
target_compile_options(fuzz_binary_types PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_binary_types PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_expanded_nodeid fuzz_expanded_nodeid.c)
target_link_libraries(fuzz_expanded_nodeid PRIVATE muc_opcua)
target_compile_options(fuzz_expanded_nodeid PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_expanded_nodeid PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_activate_session fuzz_activate_session.c)
target_link_libraries(fuzz_activate_session PRIVATE muc_opcua)
target_compile_options(fuzz_activate_session PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_activate_session PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_subscription_filters fuzz_subscription_filters.c)
target_link_libraries(fuzz_subscription_filters PRIVATE muc_opcua)
target_compile_options(fuzz_subscription_filters PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_subscription_filters PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_call fuzz_call.c)
target_link_libraries(fuzz_call PRIVATE muc_opcua)
target_compile_options(fuzz_call PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_call PRIVATE -fsanitize=fuzzer)
