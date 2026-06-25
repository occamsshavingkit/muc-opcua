# tests/CMakeLists.txt

include(CTest)

if(MICRO_OPCUA_BUILD_TESTS)
    # Fetch Unity reproducibly
    include(FetchContent)
    FetchContent_Declare(
      unity
      GIT_REPOSITORY https://github.com/ThrowTheSwitch/Unity.git
      GIT_TAG        v2.6.0
    )
    FetchContent_MakeAvailable(unity)

    add_subdirectory(support)

    # We will add these as they are implemented
    add_subdirectory(unit)
    # add_subdirectory(integration)
endif()

if(MICRO_OPCUA_BUILD_FUZZERS AND MICRO_OPCUA_IS_HOST)
    # add_subdirectory(fuzz)
endif()
