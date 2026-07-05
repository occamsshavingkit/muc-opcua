# cmake/MucOpcUaStaticAnalysis.cmake

set(MUC_OPCUA_STATIC_ANALYSIS_DIRS
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/tests"
)

# clang-format
find_program(CLANG_FORMAT clang-format)
if(CLANG_FORMAT)
    # Using a simple script or find command might not be perfectly cross-platform in CMake,
    # but it's acceptable for host builds on Linux/macOS.
    add_custom_target(format-check
        COMMAND bash -c "find \"$@\" -type f \\( -name '*.c' -o -name '*.h' \\) -print0 | xargs -0 --no-run-if-empty \"${CLANG_FORMAT}\" --dry-run --Werror" bash ${MUC_OPCUA_STATIC_ANALYSIS_DIRS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Check formatting with clang-format"
        VERBATIM
    )
else()
    message(WARNING "clang-format not found. The 'format-check' target will not be available.")
endif()

# cppcheck
find_program(CPPCHECK cppcheck)
if(CPPCHECK)
    add_custom_target(cppcheck
        COMMAND ${CPPCHECK} --enable=warning,performance,portability --std=c11 --suppress=missingIncludeSystem --inline-suppr --error-exitcode=1 -I ${CMAKE_SOURCE_DIR}/include -I ${CMAKE_SOURCE_DIR}/src -I ${CMAKE_SOURCE_DIR}/tests/support ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Static analysis with cppcheck"
        VERBATIM
    )
else()
    message(WARNING "cppcheck not found. The 'cppcheck' target will not be available.")
endif()

# clang-tidy
find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE BOOL "Export compile_commands.json for clang-tidy" FORCE)
    add_custom_target(clang-tidy
        COMMAND bash -c "find \"$@\" -type f -name '*.c' -print0 | xargs -0 --no-run-if-empty \"${CLANG_TIDY}\" -p \"${CMAKE_BINARY_DIR}\"" bash ${MUC_OPCUA_STATIC_ANALYSIS_DIRS}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Static analysis with clang-tidy"
        VERBATIM
    )
else()
    message(WARNING "clang-tidy not found. The 'clang-tidy' target will not be available.")
endif()
