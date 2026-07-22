# CMake generated Testfile for 
# Source directory: /home/quackdcs/micro-opcua/tests
# Build directory: /home/quackdcs/micro-opcua/b_push/tests
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[speed_benchmark_tool]=] "/usr/bin/python3" "/home/quackdcs/micro-opcua/tests/tools/test_speed_benchmark.py")
set_tests_properties([=[speed_benchmark_tool]=] PROPERTIES  _BACKTRACE_TRIPLES "/home/quackdcs/micro-opcua/tests/CMakeLists.txt;33;add_test;/home/quackdcs/micro-opcua/tests/CMakeLists.txt;0;")
subdirs("support")
subdirs("../_deps/unity-build")
subdirs("unit")
subdirs("integration")
subdirs("conformance")
subdirs("benchmark")
