# tests/CMakeLists.txt

include(CTest)

if(MUC_OPCUA_BUILD_TESTS OR MUC_OPCUA_BUILD_BENCHMARKS)
    add_subdirectory(support)
endif()

if(MUC_OPCUA_BUILD_TESTS)
    # Fetch Unity reproducibly
    include(FetchContent)
    FetchContent_Declare(
      unity
      GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
      GIT_TAG        v2.6.0
    )
    FetchContent_MakeAvailable(unity)

    # We will add these as they are implemented
    add_subdirectory(unit)
    add_subdirectory(integration)
    # Feature 028: claim->test map enforcement (per-profile).
    add_subdirectory(conformance)
endif()

if((MUC_OPCUA_BUILD_TESTS OR MUC_OPCUA_BUILD_BENCHMARKS) AND MUC_OPCUA_IS_HOST)
    add_subdirectory(benchmark)
endif()

if(MUC_OPCUA_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME speed_benchmark_tool
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/tools/test_speed_benchmark.py
        )
        # The scripts/profile_manifest unittest suite guards the conformance
        # manifest itself (CU reconciliation links, completion arithmetic).
        # It previously ran only when a developer invoked it by hand, so a
        # malformed satisfied_by link reached main unnoticed; register it with
        # ctest so CI executes it like any other test.
        add_test(NAME profile_manifest_tests
            COMMAND ${Python3_EXECUTABLE} -m unittest discover
                    -s ${CMAKE_SOURCE_DIR}/scripts/profile_manifest
                    -t ${CMAKE_SOURCE_DIR}
                    -p "test_*.py"
        )
    endif()
endif()

if(MUC_OPCUA_BUILD_FUZZERS AND MUC_OPCUA_IS_HOST)
    add_subdirectory(fuzz)
endif()
