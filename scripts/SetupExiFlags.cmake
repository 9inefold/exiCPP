include_guard(DIRECTORY)

set(EXI_ON_GCC   0)
set(EXI_ON_CLANG 0)
set(EXI_ON_MSVC  0)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set(EXI_ON_MSVC 1)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GCC")
  set(EXI_ON_GCC 1)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
  set(EXI_ON_CLANG 1)
else()
  message(WARNING "[exicpp] Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

##======================================================================##
## Compiler Flags
##======================================================================##

if(NOT DEFINED EXI_FLAGS)
  set(EXI_FLAGS)
endif()
set(EXI_WARNING_FLAGS -Wall -Wextra
  -Wno-unused-variable
  -Wno-unused-function
  -Wno-unused-parameter
  -Wno-ignored-qualifiers
  -Wmismatched-new-delete
) # TODO: Remove -Wmismatched-new-delete

if(EXI_ON_GCC)
  #list(APPEND EXI_FLAGS -Wuninitialized=precise)
  list(APPEND EXI_FLAGS "$<$<NOT:$<CONFIG:Debug>>:\"-ftrivial-auto-var-init=zero\">")
elseif(EXI_ON_CLANG)
  list(APPEND EXI_WARNING_FLAGS -Werror=undefined-inline)
  list(APPEND EXI_FLAGS -fdiagnostics-show-template-tree)
  if(NOT EXI_USE_THREADS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-threadsafe-statics")
  endif()
endif()

if(WIN32 AND EXI_ON_CLANG)
  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.18.0")
    include(CheckLinkerFlag)
    # CMAKE_REQUIRED_QUIET?
    check_linker_flag(CXX "-g;-Wl,--pdb=" HAVE_PDB_FLAGS)
  elseif(CMAKE_LINKER_TYPE STREQUAL "LLD")
    # TODO: Check this...
    set(HAVE_PDB_FLAGS 1)
    set(EXI_PDB_FLAGS "-g;-Wl,--pdb=")
  else()
    set(HAVE_PDB_FLAGS 0)
  endif()
else()
  set(HAVE_PDB_FLAGS 0)
endif(WIN32 AND EXI_ON_CLANG)

if(HAVE_PDB_FLAGS)
  # Generates PDB for a target.
  #  _exi_add_pdb(<target> <target-type>)
  macro(_exi_add_pdb target target_type)
    if(${target_type} MATCHES "EXECUTABLE|MODULE_LIBRARY")
      target_link_options(${target} PUBLIC ${EXI_PDB_FLAGS})
    elseif(${target_type} STREQUAL "SHARED_LIBRARY")
      target_link_options(${target} PRIVATE ${EXI_PDB_FLAGS})
    elseif(${target_type} STREQUAL "INTERFACE_LIBRARY")
      message(WARNING "Cannot add PDB to INTERFACE target '${target}'")
    endif()
  endmacro(_exi_add_pdb)
else()
  macro(_exi_add_pdb target target_type)
  endmacro(_exi_add_pdb)
endif(HAVE_PDB_FLAGS)

# Generates PDB for a target (if possible).
#  exi_add_pdb(<target>)
function(exi_add_pdb target)
  get_target_property(target_type ${target} TYPE)
  _exi_add_pdb(${target} ${target_type})
endfunction(exi_add_pdb)

# Applies the default options to a target.
#  exi_add_target_options(<target> [PDB])
function(exi_add_target_options target)
  cmake_parse_arguments(arg "PDB" "" "" ${ARGN})
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Target '${target}' does not exist!")
  endif()
  # Check if aliased
  get_target_property(alias_target ${target} ALIASED_TARGET)
  #get_property(alias_target TARGET ${target} PROPERTY ALIASED_TARGET)
  if(NOT (alias_target STREQUAL "alias_target-NOTFOUND"))
    #message(STATUS "${target} -> ${alias_target}")
    set(target ${alias_target})
  endif()
  # Add target stuff
  get_target_property(target_type ${target} TYPE)
  if(target_type STREQUAL "INTERFACE_LIBRARY")
    target_compile_features(${target} INTERFACE cxx_std_20)
    target_compile_options(${target} INTERFACE ${EXI_FLAGS})
  else()
    target_compile_features(${target} PUBLIC cxx_std_20)
    target_compile_options(${target} PUBLIC ${EXI_FLAGS})
    target_compile_options(${target} PRIVATE ${EXI_WARNING_FLAGS})
  endif()
  # Add PDB
  if(arg_PDB)
    _exi_add_pdb(${target} ${target_type})
  endif(arg_PDB)
endfunction(exi_add_target_options)

# Creates a new executable with exi_add_target_options.
#  exi_add_executable(<target> sources...)
macro(exi_add_executable target)
  add_executable(${target} ${ARGN})
  exi_add_target_options(${target} PDB)
endmacro(exi_add_executable)

##======================================================================##
## Platform/Build Flags
##======================================================================##

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
