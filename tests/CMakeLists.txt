# tests/CMakeLists.txt

include(CTest)

if(MICRO_OPCUA_BUILD_TESTS OR MICRO_OPCUA_BUILD_BENCHMARKS)
    add_subdirectory(support)
endif()

if(MICRO_OPCUA_BUILD_TESTS)
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
endif()

if((MICRO_OPCUA_BUILD_TESTS OR MICRO_OPCUA_BUILD_BENCHMARKS) AND MICRO_OPCUA_IS_HOST)
    add_subdirectory(benchmark)
endif()

if(MICRO_OPCUA_BUILD_TESTS)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_test(NAME speed_benchmark_tool
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/tools/test_speed_benchmark.py
        )
    endif()
endif()

if(MICRO_OPCUA_BUILD_FUZZERS AND MICRO_OPCUA_IS_HOST)
    add_subdirectory(fuzz)
endif()
