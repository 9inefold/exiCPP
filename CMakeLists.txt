cmake_minimum_required(VERSION 3.15)
include_guard(DIRECTORY)

project(
  exi-cpp
  VERSION 0.8.5
  LANGUAGES CXX C
)

include(Setup.cmake)
include(ListUtils)
include(SetFlags)
include(SetOptions)
include(SetVersion)

cmake_policy(SET CMP0076 NEW)
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0079 NEW)

set_version(EXI PROJECT)
set(EFL_NAMESPACE exi) # Unused... for now

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")

define_property(TARGET PROPERTY EXI_SUBPROJECT_NAME)
define_property(TEST PROPERTY EXI_SUBPROJECT_NAME)

# TODO: Remove when complete?
option(EXI_DRIVER       "If the driver should be built (always ON at top level)." OFF)
option(EXI_TESTS        "If tests should be run." OFF)

option(EXI_EXCEPTIONS   "If exceptions should be enabled." OFF)
option(EXI_XML_EXCEPTIONS "If rapidxml exceptions should be enabled." ON)
option(EXI_USE_THREADS  "Enable multithreading?" OFF)
option(EXI_USE_MIMALLOC "If allocation should be done through mimalloc." ON)

option(EXI_DEBUG        "If debug printing should be enabled." ON)
option(EXI_INVARIANTS   "Adds extra invariant checking." ON)
option(EXI_LOGGING      "If logging should be enabled." ON)
option(EXI_DEV_ONLY     "Enables things just for development." OFF)

option(EXI_ENABLE_NODISCARD "If nodiscard should be enabled." ON)
option(EXI_ENABLE_DUMP  "If dump methods shouldn't be stripped." OFF)

option(EXI_FAST_DEBUG   "If parts of debug builds should be optimized." OFF)
option(EXI_ENABLE_EXPENSIVE_CHECKS "If additional expensive checks should be enabled." OFF)
option(EXI_ENABLE_STACKTRACES "If cpptrace should be used." OFF)
option(EXI_ENABLE_UNSTABLE_FEATURES "Enable features which may not have a stable ABI." ON)
option(EXI_OPTIMIZE_UNREACHABLE "If exi_unreachable should be undefined behaviour in release builds." ON)
option(EXI_NATIVE_CODEGEN  "If code should be optimized for the native architecture." OFF)

# TODO: Remove when complete?
option(EXI_USE_EXIP "[DEPRECATED] Enables the old version of exicpp." OFF)
option(EXI_ANSI     "[DEPRECATED] If ANSI should be enabled." ON)

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Currently only supported on 64bit!")
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
else()
  # TODO: Add function for this?
  set(EXI_WARNING_FLAGS -Wall -Wextra
    -Wno-unused-variable -Wno-unused-function
    -Wno-unused-parameter -Wno-ignored-qualifiers
    -Wmismatched-new-delete) # TODO: Remove -Wmismatched-new-delete
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
    # GCC only flags
    #set(EXI_FLAGS "${EXI_FLAGS} -Wuninitialized=precise")
    set(EXI_FLAGS "${EXI_FLAGS} $<$<NOT:$<CONFIG:Debug>>:\"-ftrivial-auto-var-init=zero\">")
  elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    # Clang only flags
    list(APPEND EXI_FLAGS -fdiagnostics-show-template-tree)
    if(NOT EXI_USE_THREADS)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
    endif()
  endif()
  if(EXI_NATIVE_CODEGEN)
    set(EXI_MARCH native CACHE STRING "" FORCE)
    message(DEPRECATION "EXI_NATIVE_CODEGEN is deprecated!")
  endif()
  if(DEFINED EXI_MARCH AND NOT (EXI_MARCH STREQUAL ""))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=${EXI_MARCH}")
    set(EXI_MARCH_ENABLED ON CACHE BOOL "")
  else()
    set(EXI_MARCH_ENABLED OFF CACHE BOOL "")
  endif()
endif()

if(WIN32)
  if(CYGWIN)
    set(EXI_ON_WIN32 0)
    set(EXI_ON_UNIX 1)
  else()
    set(EXI_ON_WIN32 1)
    set(EXI_ON_UNIX 0)
  endif()
elseif(FUCHSIA OR UNIX)
  set(EXI_ON_WIN32 0)
  set(EXI_ON_UNIX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")
  set(EXI_ON_WIN32 0)
  set(EXI_ON_UNIX 0)
else()
  message(SEND_ERROR "Unable to determine platform")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(EXI_DEBUG_BUILD ON)
else()
  set(EXI_DEBUG_BUILD OFF)
  set(EXI_FAST_DEBUG OFF CACHE BOOL "" FORCE)
endif()

if(EXI_FAST_DEBUG)
  set(MI_DEBUG_FAST ON)
endif()

if(DEFINED EXI_CODEGEN_TESTS)
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(SEND_ERROR "EXI_CODEGEN_TESTS is only available with Clang.")
    unset(EXI_CODEGEN_TESTS)
  elseif(NOT EXI_CODEGEN_TESTS)
    message(WARNING "EXI_CODEGEN_TESTS is empty, ignoring.")
    unset(EXI_CODEGEN_TESTS)
  endif()
endif()

# TODO: Remove when complete?
if(EXI_USE_EXIP)
  # Sets the variables required for exip to function
  include(old/SetupExip.cmake)
endif()

message(STATUS "Library name(s)           : exi-exicpp, exi::exicpp, exi::exi")
message(STATUS "Version                   : ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
message(STATUS "Build type                : ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Compiler              : ${CMAKE_CXX_COMPILER}")
message(STATUS "Compiler flags            : ${EXI_WARNING_FLAGS};${EXI_FLAGS}")
message(STATUS "[exicpp] Tests            : ${EXI_TESTS}")
message(STATUS "[exicpp] Invariants       : ${EXI_INVARIANTS}")
message(STATUS "[exicpp] Exceptions       : ${EXI_EXCEPTIONS}")
message(STATUS "[exicpp] Logging          : ${EXI_LOGGING}")
if(EXI_DEV_ONLY)
  message(STATUS "[exicpp] Dev Only         : ON")
endif()
if(EXI_USE_MIMALLOC)
  message(STATUS "[exicpp] Allocator        : mimalloc")
else()
  message(STATUS "[exicpp] Allocator        : classic")
endif()
message(STATUS "[exicpp] Stacktraces      : ${EXI_ENABLE_STACKTRACES}")
if(EXI_DEBUG_BUILD)
  message(STATUS "[exicpp] Fast debug       : ${EXI_FAST_DEBUG}")
endif()
message(STATUS "[exicpp] Unstable features: ${EXI_ENABLE_UNSTABLE_FEATURES}")
if(EXI_MARCH_ENABLED)
  message(STATUS "[exicpp] Arch (-march=...): ${EXI_MARCH}")
endif()
if(DEFINED EXI_CODEGEN_TESTS)
  message(STATUS "[exicpp] Codegen tests    : ${EXI_CODEGEN_TESTS}")
endif()

canonicalize_cmake_booleans(
  EXI_TESTS
  EXI_EXCEPTIONS
  EXI_XML_EXCEPTIONS
  EXI_USE_THREADS
  EXI_USE_MIMALLOC
  EXI_DEBUG
  EXI_DEBUG_BUILD
  EXI_FAST_DEBUG
  EXI_INVARIANTS
  EXI_LOGGING
  EXI_ENABLE_NODISCARD
  EXI_ENABLE_DUMP
  EXI_ENABLE_EXPENSIVE_CHECKS
  EXI_ENABLE_STACKTRACES
  EXI_ENABLE_UNSTABLE_FEATURES
  EXI_OPTIMIZE_UNREACHABLE
  EXI_ON_WIN32
  EXI_ON_UNIX
)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcolor-diagnostics")
	endif()
endif(CCACHE_FOUND)

if(EXI_ENABLE_STACKTRACES OR EXI_TESTS)
  include(scripts/CPM.cmake)
endif()

set(EXI_BASE_FOLDER ${CMAKE_CURRENT_SOURCE_DIR} CACHE FILEPATH "The root exi directory.")
set(EXI_VENDORED_FOLDER "${EXI_BASE_FOLDER}/vendored" CACHE FILEPATH "The vendored directory.")
set(EXI_MI_FOLDER "${EXI_VENDORED_FOLDER}/mimalloc" CACHE FILEPATH "The mimalloc directory.")
add_subdirectory(include/core) # Configure files

function(exi_minject tgt)
  # Does nothing...
endfunction(exi_minject)

if(EXI_USE_MIMALLOC)
  set(MI_OVERRIDE ON)
  set(MI_BUILD_TESTS OFF)
  set(MI_BUILD_SHARED ON)
  set(MI_BUILD_STATIC ON)
  # TODO: Work on this, we want it to be consistent on all platforms.
  set(MI_WIN_REDIRECT OFF)

  if(WIN32 AND (CMAKE_SIZEOF_VOID_P EQUAL 8) AND (NOT MSVC))
    if(NOT (DEFINED EXI_REDIRECT AND NOT EXI_REDIRECT))
      # Currently doesn't work on x64.
      set(EXI_REDIRECT ON CACHE BOOL "" FORCE)
    endif()
  endif()

  if(EXI_DEBUG_BUILD)
    if(EXI_FAST_DEBUG)
      set(MI_NO_PADDING ON)
    else()
      # set(MI_TRACK_ETW ON)
    endif()
  endif()

  if(EXI_REDIRECT)
    add_subdirectory(redirect)
  endif()
  add_subdirectory(vendored/mimalloc)
  if(EXI_REDIRECT)
    exi_clear_lib_prefix(mimalloc)
    target_link_libraries(mimalloc PRIVATE exi::redirect)
  endif()
endif()

add_subdirectory(vendored/fmt)
add_subdirectory(vendored/rapidxml)

add_library(exi-cpptrace INTERFACE)
if(EXI_ENABLE_STACKTRACES AND NOT EXI_USE_MIMALLOC)
  # TODO: Add check for debug symbols
  CPMAddPackage(
    URI "gh:jeremy-rifkin/cpptrace@1.0.4"
    OPTIONS "CPPTRACE_DISABLE_CXX_20_MODULES ON")
  target_link_libraries(exi-cpptrace INTERFACE cpptrace::cpptrace)
elseif(EXI_ENABLE_STACKTRACES)
  message(WARNING "cpptrace does not work with mimalloc!")
endif()

if(EXI_USE_EXIP)
  message(STATUS "[exicpp] Using exip.")
  add_subdirectory(vendored/exip)
  add_subdirectory(old)
else()
  include(NewLib.cmake)
endif()
