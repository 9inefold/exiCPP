cmake_minimum_required(VERSION 3.18)
include_guard(DIRECTORY)
include(Setup.cmake)

project(
  exi-cpp
  VERSION 0.0.1
  LANGUAGES CXX C
)

include(ListUtils)
include(SetFlags)
include(SetOptions)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
endif()

add_subdirectory(vendored/exip)

if(PROJECT_IS_TOP_LEVEL)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exip::exip)
endif()
