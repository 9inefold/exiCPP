cmake_minimum_required(VERSION 3.12)
include_guard(DIRECTORY)

project(
  exi-cpp
  VERSION 0.6.27
  LANGUAGES CXX C
)

include(Setup.cmake)
include(ListUtils)
include(SetFlags)
include(SetOptions)
include(SetVersion)

cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0079 NEW)

set_version(EXI PROJECT)
set(EFL_NAMESPACE exi) # Unused... for now

option(EXI_USE_EXIP     "Enables the old version of exicpp." OFF)
option(EXI_DRIVER       "If the driver should be built (always ON at top level)." OFF)
option(EXI_TESTS        "If tests should be run." OFF)

option(EXI_EXCEPTIONS   "If exceptions should be enabled." OFF)
option(EXI_XML_EXCEPTIONS "If rapidxml exceptions should be enabled." ON)
option(EXI_USE_THREADS  "Enable multithreading?" OFF)
option(EXI_USE_MIMALLOC "If allocation should be done through mimalloc." ON)

option(EXI_DEBUG        "If debug printing should be enabled." ON)
option(EXI_INVARIANTS   "Adds extra invariant checking." ON)
option(EXI_LOGGING      "If logging should be enabled." ON)

option(EXI_ENABLE_NODISCARD "If nodiscard should be enabled." ON)
option(EXI_ENABLE_DUMP  "If dump methods shouldn't be stripped." OFF)

option(EXI_FAST_DEBUG   "If parts of debug builds should be optimized." OFF)
option(EXI_ENABLE_EXPENSIVE_CHECKS "If additional expensive checks should be enabled." OFF)
option(EXI_OPTIMIZE_UNREACHABLE "If exi_unreachable should be undefined behaviour in release builds." ON)

option(EXI_ANSI         "If ANSI should be enabled." ON) # DEPRECATED

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
else()
  # TODO: Add function for this?
  set(EXI_WARNING_FLAGS -Wall -Wextra
    -Wno-unused-variable -Wno-unused-function
    -Wno-unused-parameter)
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
    set(EXI_FLAGS "${EXI_FLAGS} -Wuninitialized=precise
      $<$<NOT:$<CONFIG:Debug>>:\"-ftrivial-auto-var-init=zero\">")
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

if(EXI_USE_EXIP)
  option(EXIP_FORCE_SCOPE  "If the exip namespace should be forced." ON)

  set_if_unset(EXICPP_WARNING_FLAGS ${EXI_WARNING_FLAGS})
  set_if_unset(EXICPP_INVARIANTS    ${EXI_INVARIANTS})
  set_if_unset(EXICPP_EXCEPTIONS    ${EXI_EXCEPTIONS})
  set_if_unset(EXICPP_TESTS         ${EXI_TESTS})
  set_if_unset(EXICPP_DEBUG         ${EXI_DEBUG})
  set_if_unset(EXICPP_ANSI          ${EXI_ANSI})
  set_if_unset(EXICPP_USE_MIMALLOC  ${EXI_USE_MIMALLOC})

  set(EXICPP_DEBUG_BUILD            ${EXI_DEBUG_BUILD})
  set(EXICPP_FAST_DEBUG             ${EXI_FAST_DEBUG})

  set(EXIP_USE_MIMALLOC             ${EXI_USE_MIMALLOC})
  set(EXIP_DEBUG                    ${EXI_DEBUG})
  if(EXI_ANSI)
    set(EXIP_ANSI ON CACHE BOOL "")
  endif()
endif()

message(STATUS "[exicpp] Tests: ${EXI_TESTS}")
message(STATUS "[exicpp] Invariants: ${EXI_INVARIANTS}")
message(STATUS "[exicpp] Debug printing: ${EXI_DEBUG}")
message(STATUS "[exicpp] ANSI printing: ${EXI_ANSI}")
message(STATUS "[exicpp] Exceptions: ${EXI_EXCEPTIONS}")
if(EXI_USE_MIMALLOC)
  message(STATUS "[exicpp] Allocator: mimalloc")
else()
  message(STATUS "[exicpp] Allocator: classic")
endif()
if(EXI_DEBUG_BUILD)
  message(STATUS "[exicpp] Fast debug: ${EXI_FAST_DEBUG}")
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

if(EXI_USE_EXIP)
  message(STATUS "[exicpp] Using exip.")
  add_subdirectory(vendored/exip)
  add_subdirectory(old)
else()
  include(NewLib.cmake)
endif()
