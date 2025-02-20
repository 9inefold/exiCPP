include_guard(DIRECTORY)

project(
  rapidxml
  VERSION 1.13.1
  LANGUAGES CXX
)

add_library(rapidxml INTERFACE)
add_library(rapidxml::rapidxml ALIAS rapidxml)
if(EXI_USE_EXIP)
  target_include_directories(rapidxml INTERFACE old)
else()
  target_include_directories(rapidxml INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
endif()
