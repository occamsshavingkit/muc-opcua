# tests/fuzz/CMakeLists.txt

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzzing requires Clang compiler")
    return()
endif()

add_executable(fuzz_binary_reader fuzz_binary_reader.c)
target_link_libraries(fuzz_binary_reader PRIVATE micro_opcua)
target_compile_options(fuzz_binary_reader PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_binary_reader PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_message_chunk fuzz_message_chunk.c)
target_link_libraries(fuzz_message_chunk PRIVATE micro_opcua)
target_compile_options(fuzz_message_chunk PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_message_chunk PRIVATE -fsanitize=fuzzer)

add_executable(fuzz_binary_types fuzz_binary_types.c)
target_link_libraries(fuzz_binary_types PRIVATE micro_opcua)
target_compile_options(fuzz_binary_types PRIVATE -fsanitize=fuzzer)
target_link_options(fuzz_binary_types PRIVATE -fsanitize=fuzzer)
