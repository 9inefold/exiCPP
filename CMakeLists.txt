cmake_minimum_required(VERSION 3.12)
include_guard(DIRECTORY)
include(Setup.cmake)

project(
  exi-cpp
  VERSION 0.5.0
  LANGUAGES CXX C
)

include(ListUtils)
include(SetFlags)
include(SetOptions)
cmake_policy(SET CMP0077 NEW)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
endif()

option(EXICPP_DRIVER     "If the driver should be built (always ON at top level)." OFF)
option(EXICPP_TESTS      "If tests should be run." OFF)

option(EXICPP_FAST_DEBUG "If parts of debug builds should be optimized." OFF)
option(EXICPP_EXCEPTIONS "If exceptions should be enabled." OFF)
option(EXICPP_DEBUG      "If debug printing should be enabled." ON)
option(EXICPP_ANSI       "If ANSI should be enabled." ON)

option(EXIP_FORCE_SCOPE  "If the exip namespace should be forced." ON)
option(EXIP_USE_MIMALLOC "If allocation should be done through mimalloc" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(EXICPP_DEBUG_BUILD ON)
else()
  set(EXICPP_DEBUG_BUILD OFF)
  set(EXICPP_FAST_DEBUG OFF)
endif()

message(STATUS "[exicpp] Tests: ${EXICPP_TESTS}")
message(STATUS "[exicpp] Debug printing: ${EXICPP_DEBUG}")
message(STATUS "[exicpp] ANSI printing: ${EXICPP_ANSI}")
message(STATUS "[exicpp] Exceptions: ${EXICPP_EXCEPTIONS}")
if(EXICPP_DEBUG_BUILD)
  message(STATUS "[exicpp] Fast debug: ${EXICPP_FAST_DEBUG}")
endif()

set(EXIP_DEBUG ${EXICPP_DEBUG})
if(EXICPP_ANSI)
  set(EXIP_ANSI ON)
endif()

if(EXIP_USE_MIMALLOC)
  set(MI_OVERRIDE ON)
  set(MI_BUILD_TESTS OFF)
  set(MI_BUILD_SHARED ON)
  set(MI_BUILD_STATIC ON)
  # Currently doesn't work on windows.
  set(MI_WIN_REDIRECT OFF)

  if(EXICPP_DEBUG_BUILD)
    if(EXICPP_FAST_DEBUG)
      set(MI_NO_PADDING ON)
    else()
      set(MI_TRACK_ETW ON)
    endif()
  endif()
  add_subdirectory(vendored/mimalloc)
endif()

add_subdirectory(vendored/exip)
add_subdirectory(vendored/fmt)

include_items(EXICPP_SRC "src"
  BinaryBuffer.cpp
  Errors.cpp
  Filesystem.cpp
  Writer.cpp
  XML.cpp

  Debug/CheckFlags.cpp
  Debug/Format.cpp
  Debug/RapidxmlHandler.cpp
  Debug/Terminal.cpp
)

add_library(exicpp STATIC ${EXICPP_SRC})
add_library(exicpp::exicpp ALIAS exicpp)
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
  target_compile_options(exicpp PRIVATE
    -Wall -Wpedantic -Wno-unused-variable -Wno-unused-function
  )
endif()

if(EXICPP_TESTS OR (PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER))
  add_library(exicpp-util STATIC
    utils/CompareXml.cpp
  )
  add_library(exicpp::util ALIAS exicpp-util)
  target_include_directories(exicpp-util PUBLIC utils)
  target_link_libraries(exicpp-util PUBLIC exicpp)
endif()

if(EXICPP_TESTS)
  include(CTest)
  add_subdirectory(test)
endif()

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exicpp::exicpp exicpp::util)
endif()
