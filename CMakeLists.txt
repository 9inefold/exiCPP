cmake_minimum_required(VERSION 3.12)
include_guard(DIRECTORY)

project(
  exi-cpp
  VERSION 0.6.11
  LANGUAGES CXX C
)

include(Setup.cmake)
include(ListUtils)
include(SetFlags)
include(SetOptions)
include(SetVersion)
cmake_policy(SET CMP0077 NEW)

set_version(EXI PROJECT)
set(EFL_NAMESPACE exi) # Unused... for now

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
else()
  # TODO: Add function for this?
  set(EXICPP_WARNING_FLAGS -Wall -Wextra
    -Wno-unused-variable -Wno-unused-function
    -Wno-unused-parameter
  )
endif()

option(EXICPP_USE_EXIP   "Enables the old version of exicpp." OFF)
option(EXICPP_DRIVER     "If the driver should be built (always ON at top level)." OFF)
option(EXICPP_TESTS      "If tests should be run." OFF)

option(EXICPP_INVARIANTS "Adds extra invariant checking." ON)
option(EXICPP_FAST_DEBUG "If parts of debug builds should be optimized." OFF)
option(EXICPP_EXCEPTIONS "If exceptions should be enabled." OFF)
option(EXICPP_DEBUG      "If debug printing should be enabled." ON)
option(EXICPP_ANSI       "If ANSI should be enabled." ON)

option(EXICPP_USE_MIMALLOC "If allocation should be done through mimalloc" ON)
if(EXICPP_USE_EXIP)
  option(EXIP_FORCE_SCOPE  "If the exip namespace should be forced." ON)
  set(EXIP_USE_MIMALLOC ${EXICPP_USE_MIMALLOC})
  set(EXIP_DEBUG ${EXICPP_DEBUG})
  if(EXICPP_ANSI)
    set(EXIP_ANSI ON CACHE BOOL "")
  endif()
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(EXICPP_DEBUG_BUILD ON)
else()
  set(EXICPP_DEBUG_BUILD OFF)
  set(EXICPP_FAST_DEBUG OFF CACHE BOOL "" FORCE)
endif()

message(STATUS "[exicpp] Tests: ${EXICPP_TESTS}")
message(STATUS "[exicpp] Invariants: ${EXICPP_INVARIANTS}")
message(STATUS "[exicpp] Debug printing: ${EXICPP_DEBUG}")
message(STATUS "[exicpp] ANSI printing: ${EXICPP_ANSI}")
message(STATUS "[exicpp] Exceptions: ${EXICPP_EXCEPTIONS}")
if(EXICPP_DEBUG_BUILD)
  message(STATUS "[exicpp] Fast debug: ${EXICPP_FAST_DEBUG}")
endif()

if(EXICPP_USE_MIMALLOC)
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
add_subdirectory(vendored/fmt)
add_subdirectory(vendored/rapidxml)

if(EXICPP_USE_EXIP)
  message(STATUS "[exicpp] Using exip.")
  add_subdirectory(vendored/exip)
  add_subdirectory(old)
else()
  include(NewLib.cmake)
endif()
