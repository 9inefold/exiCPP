include_guard(DIRECTORY)

project(
  exi-redirect
  VERSION 0.2.2
  LANGUAGES CXX C
)

if(NOT WIN32)
  message(SEND_ERROR "Redirect should not be used on non-Windows targets!")
endif()

##########################################################################
## Patches

add_library(exi-redirect-patches STATIC lib/Patches.cpp)

if(MSVC)
  target_compile_options(exi-redirect-patches
    PRIVATE /permissive /w
    PUBLIC
    /Zc:preprocessor
    /Zc:threadSafeInit-
    /EHcs- /EHr- /GR-
  )
else()
  target_compile_options(exi-redirect-patches
    PRIVATE -fpermissive -w
    PUBLIC
    -fno-threadsafe-statics
    -fno-asynchronous-unwind-tables
    -fno-exceptions -fno-rtti
  )
endif()

target_include_directories(
  exi-redirect-patches PUBLIC
  ${EXI_BASE_FOLDER}/include/core/Config
  include
)

##########################################################################
## Redirect

add_library(exi-redirect SHARED Driver.cpp)
add_library(exi::redirect ALIAS exi-redirect)

exi_clear_lib_prefix(exi-redirect)
set_target_properties(exi-redirect PROPERTIES
  OUTPUT_NAME "mimalloc-redirect"
)

target_link_libraries(exi-redirect PRIVATE ntdll exi-redirect-patches)
if(MSVC)
  target_link_options(exi-redirect PRIVATE /NODEFAULTLIB)
else()
  target_compile_options(exi-redirect PRIVATE
    -shared
    -fno-threadsafe-statics
    -fno-asynchronous-unwind-tables
    -fno-exceptions -fno-rtti
  )
  target_link_options(exi-redirect PRIVATE -nostdlib -nodefaultlibs)
endif()

target_sources(exi-redirect PRIVATE
  lib/ArrayRef.cpp
  lib/Buf.cpp
  lib/Detours.cpp
  lib/DetoursImpl.cpp
  lib/DetoursSetup.cpp
  lib/Env.cpp
  lib/Globals.cpp
  lib/Logging.cpp
  lib/NtImports.cpp
  # lib/Patches.cpp
  lib/RVA.cpp
  lib/Strings.cpp
  lib/Version.cpp
  lib/VMemcpy.cpp
  lib/VMemset.cpp
)

install(TARGETS exi-redirect EXPORT exi-redirect
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(EXPORT exi-redirect
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/exi-redirect")

##########################################################################

function(exi_minject tgt)
  cmake_parse_arguments(
    PARSE_ARGV 1 arg
    "COPY;BACKUP;CLASSIC" "" "")
  
  get_target_property(prop_TYPE ${tgt} TYPE)
  if(NOT prop_TYPE STREQUAL "EXECUTABLE")
    message(SEND_ERROR
      "exi_minject should only be called on an EXECUTABLE (got ${prop_TYPE})")
  endif()

  if(EXI_REDIRECT)
    if(arg_CLASSIC)
      target_link_libraries(${tgt}
        ${EXI_MI_FOLDER}/bin/mimalloc-redirect.lib)
      add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        ${EXI_MI_FOLDER}/bin/mimalloc-redirect.dll
        $<TARGET_FILE_DIR:${tgt}>/mimalloc-redirect.dll)
    else()
      target_link_libraries(${tgt} exi::redirect)
    endif()
  endif()

  if(arg_COPY OR arg_BACKUP)
    add_custom_command(TARGET ${tgt} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${tgt}>
      $<TARGET_FILE_DIR:${tgt}>/backup-$<TARGET_FILE_NAME:${tgt}>)
  endif()

  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(MINJECT_FILE minject.exe)
  else()
    set(MINJECT_FILE minject32.exe)
  endif()

  get_target_property(prop_OUTPUT_NAME mimalloc OUTPUT_NAME)
  string(REGEX REPLACE "mimalloc\-?" "" var_POSTFIX ${prop_OUTPUT_NAME})
  if(var_POSTFIX STREQUAL "")
    set_target_properties(mimalloc PROPERTIES
      OUTPUT_NAME "mimalloc-override")
    set(var_POSTFIX "override")
  endif()

  # message(STATUS "Postfix: ${var_POSTFIX}")
  add_custom_command(TARGET ${tgt} POST_BUILD
    COMMAND ${EXI_MI_FOLDER}/bin/${MINJECT_FILE}
    --inplace --force
    --postfix=${var_POSTFIX} $<TARGET_FILE:${tgt}>)
endfunction(exi_minject)
