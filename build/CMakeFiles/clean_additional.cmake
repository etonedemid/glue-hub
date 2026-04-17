# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "CMakeFiles/rexglue-hub_autogen.dir/AutogenUsed.txt"
  "CMakeFiles/rexglue-hub_autogen.dir/ParseCache.txt"
  "rexglue-hub_autogen"
  )
endif()
