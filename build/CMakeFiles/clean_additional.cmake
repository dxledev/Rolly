# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  "CMakeFiles/archive_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/archive_tests_autogen.dir/ParseCache.txt"
  "CMakeFiles/rolly_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/rolly_autogen.dir/ParseCache.txt"
  "CMakeFiles/rolly_core_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/rolly_core_autogen.dir/ParseCache.txt"
  "CMakeFiles/rolly_ui_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/rolly_ui_autogen.dir/ParseCache.txt"
  "CMakeFiles/theme_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/theme_tests_autogen.dir/ParseCache.txt"
  "CMakeFiles/ui_tests_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/ui_tests_autogen.dir/ParseCache.txt"
  "archive_tests_autogen"
  "rolly_autogen"
  "rolly_core_autogen"
  "rolly_ui_autogen"
  "theme_tests_autogen"
  "ui_tests_autogen"
  )
endif()
