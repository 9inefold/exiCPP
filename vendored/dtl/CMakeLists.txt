include_guard(DIRECTORY)

project(
  dtl
  VERSION 1.21.0
  LANGUAGES CXX
)

add_library(dtl INTERFACE)
add_library(dtl::dtl ALIAS dtl)
target_include_directories(dtl INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
