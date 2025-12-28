# CMake generated Testfile for 
# Source directory: D:/dev/ktvlv/ktvlv/build_ninja2/_deps/cjson-src
# Build directory: D:/dev/ktvlv/ktvlv/build_ninja2/_deps/cjson-build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(cJSON_test "D:/dev/ktvlv/ktvlv/build_ninja2/_deps/cjson-build/cJSON_test")
set_tests_properties(cJSON_test PROPERTIES  _BACKTRACE_TRIPLES "D:/dev/ktvlv/ktvlv/build_ninja2/_deps/cjson-src/CMakeLists.txt;248;add_test;D:/dev/ktvlv/ktvlv/build_ninja2/_deps/cjson-src/CMakeLists.txt;0;")
subdirs("tests")
subdirs("fuzzing")
