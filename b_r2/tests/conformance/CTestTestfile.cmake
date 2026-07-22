# CMake generated Testfile for 
# Source directory: /home/quackdcs/micro-opcua/tests/conformance
# Build directory: /home/quackdcs/micro-opcua/b_r2/tests/conformance
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[test_claim_map]=] "/usr/bin/python3" "/home/quackdcs/micro-opcua/tests/conformance/check_claim_map.py" "--manifest" "/home/quackdcs/micro-opcua/tests/conformance/claim_test_map.md" "--profile" "full" "--build-dir" "/home/quackdcs/micro-opcua/b_r2")
set_tests_properties([=[test_claim_map]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/quackdcs/micro-opcua/tests/conformance/CMakeLists.txt;8;add_test;/home/quackdcs/micro-opcua/tests/conformance/CMakeLists.txt;0;")
