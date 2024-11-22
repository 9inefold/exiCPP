include_guard(DIRECTORY)

project(
  rapidxml
  VERSION 1.13.0
  LANGUAGES CXX
)

add_library(rapidxml INTERFACE)
add_library(rapidxml::rapidxml ALIAS rapidxml)
target_include_directories(rapidxml INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
