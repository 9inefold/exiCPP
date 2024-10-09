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
cmake_policy(SET CMP0077 NEW)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
endif()

option(EXICPP_EXCEPTIONS "If exceptions should be enabled." OFF)
option(EXICPP_DEBUG  "If debug printing should be enabled." ON)
option(EXICPP_ANSI   "If ANSI should be enabled." ON)
option(EXIP_FORCE_SCOPE "If the exip namespace should be forced." ON)

set(EXIP_DEBUG ${EXICPP_DEBUG})
if(EXICPP_ANSI)
  set(EXIP_ANSI ON)
endif()

message(STATUS "[exicpp] Debug printing: ${EXICPP_DEBUG}")
message(STATUS "[exicpp] ANSI printing: ${EXICPP_ANSI}")
message(STATUS "[exicpp] Exceptions: ${EXICPP_EXCEPTIONS}")

add_subdirectory(vendored/exip)
add_subdirectory(vendored/fmt)

include_items(EXICPP_SRC "src"
  Debug/CheckFlags.cpp
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
