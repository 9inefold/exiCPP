cmake_minimum_required(VERSION 3.12)
include_guard(DIRECTORY)
include(Setup.cmake)

project(
  exi-cpp
  VERSION 0.1.0
  LANGUAGES CXX C
)

include(ListUtils)
include(SetFlags)
include(SetOptions)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
endif()

option(EXICPP_EXCEPTIONS "If exceptions should be enabled." OFF)
option(EXICPP_DEBUG  "If debug printing should be enabled." ON)
option(EXICPP_ANSI   "If ANSI should be enabled." ON)
option(EXICPP_FORMAT "If fmtlib should be enabled." ON)
option(EXIP_FORCE_SCOPE "If the exip namespace should be forced." ON)

if(EXICPP_ANSI)
  set(EXIP_ANSI ON)
endif()

add_subdirectory(vendored/exip)
add_subdirectory(vendored/fmt)

include_items(EXICPP_SRC "src"
  BinaryBuffer.cpp
  Errors.cpp
  Filesystem.cpp
  Writer.cpp
  XML.cpp
  Debug/Format.cpp
  Debug/RapidxmlHandler.cpp
)

add_library(exicpp STATIC ${EXICPP_SRC})
target_include_directories(exicpp
  PUBLIC include vendored/rapidxml
  PRIVATE include/exicpp 
)
target_link_libraries(exicpp PUBLIC exip::exip fmt::fmt)
target_compile_features(exicpp PUBLIC cxx_std_17)
target_forward_options(exicpp PUBLIC
  EXICPP_DEBUG
  EXICPP_ANSI
  EXICPP_FORMAT
)

if(NOT EXICPP_EXCEPTIONS)
  target_compile_definitions(exicpp PUBLIC
    EXICPP_NO_EXCEPTIONS=1
    RAPIDXML_NO_EXCEPTIONS=1
  )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # ...
else()
  target_compile_options(exip PRIVATE -Wall -Wpedantic)
endif()

if(PROJECT_IS_TOP_LEVEL)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exicpp)
endif()
