# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  "CMakeFiles\\MekanikTesisatDraw_autogen.dir\\AutogenUsed.txt"
  "CMakeFiles\\MekanikTesisatDraw_autogen.dir\\ParseCache.txt"
  "MekanikTesisatDraw_autogen"
  )
endif()
