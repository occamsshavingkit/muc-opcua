# tests/conformance/CMakeLists.txt
# Feature 028: register the claim->test map checker as a per-profile ctest. It
# fails the profile's test run when a claimed conformance unit has no backing test
# registered in that profile (OPC-10000-7 §4.2/§4.3).
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    # CTest registration - runs the checker during `ctest` with the same args.
    add_test(NAME test_claim_map
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_claim_map.py
                --manifest ${CMAKE_CURRENT_SOURCE_DIR}/claim_test_map.md
                --profile ${MUC_OPCUA_PROFILE}
                --build-dir ${CMAKE_BINARY_DIR})

    # Build target - lets `cmake --build <dir> --target test_claim_map` run the
    # same checker directly (identical command, same script/manifest/profile/build-dir).
    add_custom_target(test_claim_map
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/check_claim_map.py
                --manifest ${CMAKE_CURRENT_SOURCE_DIR}/claim_test_map.md
                --profile ${MUC_OPCUA_PROFILE}
                --build-dir ${CMAKE_BINARY_DIR}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/check_claim_map.py
                ${CMAKE_CURRENT_SOURCE_DIR}/claim_test_map.md
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Running claim->test map checker")
else()
    message(WARNING "Python3 not found; claim->test map checker (test_claim_map) not registered")
endif()
