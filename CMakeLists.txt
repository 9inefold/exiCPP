cmake_minimum_required(VERSION 3.12)
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

option(EXIP_FORCE_SCOPE "If the exip namespace should be forced." ON)

add_subdirectory(vendored/exip)

add_library(exicpp STATIC src/Empty.cpp)
target_include_directories(exicpp PUBLIC include vendored/rapidxml)
target_link_libraries(exicpp PUBLIC exip::exip)
target_compile_features(exicpp PUBLIC cxx_std_17)

if(PROJECT_IS_TOP_LEVEL)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exicpp)
endif()
