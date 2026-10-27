# CMake generated Testfile for 
# Source directory: /home/dxle/builds/Rolly
# Build directory: /home/dxle/builds/Rolly/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test("theme" "/home/dxle/builds/Rolly/build/theme_tests")
set_tests_properties("theme" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/dxle/builds/Rolly/CMakeLists.txt;35;add_test;/home/dxle/builds/Rolly/CMakeLists.txt;0;")
add_test("archive" "/home/dxle/builds/Rolly/build/archive_tests")
set_tests_properties("archive" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/dxle/builds/Rolly/CMakeLists.txt;35;add_test;/home/dxle/builds/Rolly/CMakeLists.txt;0;")
add_test("ui" "/home/dxle/builds/Rolly/build/ui_tests")
set_tests_properties("ui" PROPERTIES  ENVIRONMENT "QT_QPA_PLATFORM=offscreen" _BACKTRACE_TRIPLES "/home/dxle/builds/Rolly/CMakeLists.txt;35;add_test;/home/dxle/builds/Rolly/CMakeLists.txt;0;")
add_test("cli" "bash" "/home/dxle/builds/Rolly/tests/cli_test.sh" "/home/dxle/builds/Rolly/build/rolly")
set_tests_properties("cli" PROPERTIES  _BACKTRACE_TRIPLES "/home/dxle/builds/Rolly/CMakeLists.txt;38;add_test;/home/dxle/builds/Rolly/CMakeLists.txt;0;")
