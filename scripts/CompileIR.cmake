include_guard(DIRECTORY)

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")

include(DumpProperties)
set(_ir_message_type STATUS)

# Prints "[compile_ir] <message>".
macro(_ir_message message)
  message(${_ir_message_type} "[compile_ir] ${message}")
endmacro(_ir_message)
# Sends error <message> then returns.
macro(_ir_throw message)
  message(SEND_ERROR "[compile_ir] ${message}")
  return()
endmacro(_ir_throw)

# Checks required arguments.
macro(_ir_check_required)
  set(ir_excluded "")
  set(ir_required ${ARGV})
  foreach(required ${ir_required})
    if(NOT DEFINED ir_${required})
      list(APPEND ir_excluded "${required}")
    endif()
  endforeach()
  unset(ir_required)

  if(DEFINED ir_excluded AND ir_excluded)
    list(JOIN ir_excluded ", " ir_excluded_msg)
    _ir_throw("The following are required: ${ir_excluded_msg}")
  endif()
endmacro(_ir_check_required)

# Fixes the output folder.
function(_ir_update_destination folder)
  if(NOT DEFINED ${folder})
    set(ifolder "${CMAKE_CURRENT_BINARY_DIR}/IR")
    if(NOT IS_DIRECTORY ifolder)
      file(MAKE_DIRECTORY "${ifolder}")
    endif()
  else()
    set(ifolder "${${folder}}")
    if(IS_ABSOLUTE ifolder)
      cmake_path(NORMALIZE ifolder)
    else()
      cmake_path(ABSOLUTE_PATH ifolder
        BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" NORMALIZE)
      if(NOT IS_DIRECTORY ifolder)
        file(MAKE_DIRECTORY "${ifolder}")
      endif()
    endif()
  endif()
  set(${folder} "${ifolder}" PARENT_SCOPE)
endfunction(_ir_update_destination)

macro(__ir_get_property out target prop)
  get_target_property(${out} ${target} ${prop})
  if(${out} STREQUAL "${out}-NOTFOUND")
    unset(${out})
  endif()
endmacro(__ir_get_property)

# Sets options in the parent scope.
macro(__ir_set_ilib_vals target)
  __ir_get_property(_cpp_iincludes ${target} INTERFACE_INCLUDE_DIRECTORIES)
  __ir_get_property(_cpp_ioptions ${target} INTERFACE_COMPILE_OPTIONS)
  __ir_get_property(_cpp_idefs ${target} INTERFACE_COMPILE_DEFINITIONS)
  list(APPEND cpp_iincludes ${_cpp_iincludes})
  list(APPEND cpp_ioptions ${_cpp_ioptions})
  list(APPEND cpp_idefs ${_cpp_idefs})
endmacro(__ir_set_ilib_vals)

# Recursively visits and updates target properties
function(__ir_recurse_libs visited)
  set(curr_libs ${ARGN})
  foreach(lib ${curr_libs})
    if(lib IN_LIST visited)
      continue()
    elseif(NOT TARGET ${lib})
      list(APPEND visited ${lib})
      continue()
    endif()
    list(APPEND visited ${lib})
    __ir_set_ilib_vals(${lib})
    get_target_property(_sublibs ${lib} INTERFACE_LINK_LIBRARIES)
    if(_sublibs)
      __ir_recurse_libs("${visited}" ${_sublibs})
    endif()
  endforeach()
  set(cpp_iincludes ${cpp_iincludes} PARENT_SCOPE)
  set(cpp_ioptions ${cpp_ioptions} PARENT_SCOPE)
  set(cpp_idefs ${cpp_idefs} PARENT_SCOPE)
  set(visited ${visited} PARENT_SCOPE)
endfunction(__ir_recurse_libs)

# Recursively visits and updates target properties
macro(_ir_recurse_libs)
  if(ARGN)
    __ir_recurse_libs("" ${ARGN})
  endif()
endmacro(_ir_recurse_libs)

# Converts CMake name into native script name.
macro(_ir_script_fixup_varname var)
  string(TOUPPER "IR_${${var}}" ${var})
  string(REGEX REPLACE "[.-]" "_" ${var} "${${var}}")
endmacro(_ir_script_fixup_varname)

# Strips invalid characters from variable value.
macro(_ir_script_fixup_varvalue value)
  list(JOIN ${value} " " ${value})
  string(REGEX REPLACE "[\r\n]" " " ${value} "${${value}}")
endmacro(_ir_script_fixup_varvalue)

# Escapes strings when writing out.
macro(_ir_script_escape_str value)
  string(REGEX REPLACE "\"" "\\\\\"" ${value} "${${value}}")
endmacro(_ir_script_escape_str)

# API for generating script files.
if(CMAKE_HOST_WIN32)
  set(_ir_script_ext "bat" CACHE INTERNAL "")

  # Initializes a .bat file
  function(_ir_script_init filename)
    file(WRITE "${filename}" "@echo off\n\n")
  endfunction(_ir_script_init)

  # Adds a comment to a .bat file
  function(_ir_script_comment filename)
    set(var_value ${ARGN})
    _ir_script_fixup_varvalue(var_value)
    file(APPEND "${filename}" ":: ${var_value}\n")
  endfunction(_ir_script_comment)

  # Adds a variable to a .bat file
  function(_ir_script_var filename var)
    set(var_name "${${var}}")
    set(var_value ${ARGN})
    _ir_script_fixup_varname(var_name)
    _ir_script_fixup_varvalue(var_value)
    file(APPEND "${filename}" "set ${var_name}=${var_value}\n")
  endfunction(_ir_script_var)

  # Fixes variable name for a .bat file
  function(_ir_script_ref var)
    set(var_name "${${var}}")
    _ir_script_fixup_varname(var_name)
    set(${var} "%${var_name}%" PARENT_SCOPE)
  endfunction(_ir_script_ref)
else()
  set(_ir_script_ext "sh" CACHE INTERNAL "")

  # Initializes a .sh file
  function(_ir_script_init filename)
    file(WRITE "${filename}" "#!/bin/sh\n\n")
  endfunction(_ir_script_init)

  # Adds a comment to a .sh file
  function(_ir_script_comment filename)
    set(var_value ${ARGN})
    _ir_script_fixup_varvalue(var_value)
    file(APPEND "${filename}" "# ${var_value}\n")
  endfunction(_ir_script_comment)

  # Adds a variable to a .sh file
  function(_ir_script_var filename var)
    set(var_name "${${var}}")
    set(var_value ${ARGN})
    _ir_script_fixup_varname(var_name)
    _ir_script_fixup_varvalue(var_value)
    _ir_script_escape_str(var_value)
    file(APPEND "${filename}" "${var_name}=\"${var_value}\"\n")
  endfunction(_ir_script_var)

  # Fixes variable name for a .sh file
  function(_ir_script_ref var)
    set(var_name "${${var}}")
    _ir_script_fixup_varname(var_name)
    set(${var} "\$${var_name}" PARENT_SCOPE)
  endfunction(_ir_script_ref)
endif()

# Generates a target for the roots
function(_ir_handle_roots ir_target root)
  set(root_sources ${ARGN})
  set(root_folder "${${root}_FOLDER}")
  set(root_targets "${${root}_TARGETS}")
  
  set(map_FILE "")
  set(map_TARGET "")

  foreach(target ${root_targets})
    get_target_property(target_type ${target} TYPE)
    if(NOT target_type STREQUAL "STATIC_LIBRARY")
      message(WARNING "[compile_ir] '${target}' is not a static library.")
      continue()
    endif()

    get_target_property(target_src ${target} SOURCES)
    if(NOT target_src)
      continue()
    endif()
    foreach(src ${target_src})
      list(APPEND map_FILE "${src}")
      list(APPEND map_TARGET ${target})
    endforeach()
  endforeach()

  set(ir_cpp_sources "")
  list(TRANSFORM map_FILE REPLACE "^${root_folder}/" "")
  foreach(source ${root_sources})
    string(REGEX REPLACE "\\*" "[^/]*" source_regex ${source})
    string(REGEX REPLACE "\\." "/" source_regex ${source_regex})

    set(source_sources ${map_FILE})
    list(FILTER source_sources INCLUDE REGEX "^${source_regex}\\.(cpp)$")
    if(NOT source_sources)
      message(WARNING "No sources found for \"${source}\", check casing?")
      continue()
    endif()
    list(APPEND ir_cpp_sources ${source_sources})
  endforeach()
  list(REMOVE_DUPLICATES ir_cpp_sources)
  if(NOT ir_cpp_sources)
    _ir_throw("Unable to find sources for '${root}'!")
  endif()
  _ir_message("Found: ${ir_cpp_sources}")

  set(ir_cpp_targets "")
  foreach(cpp_source ${ir_cpp_sources})
    list(FIND map_FILE "${cpp_source}" idx)
    if(idx EQUAL -1)
      message(WARNING "Unknown source \"${cpp_source}\"")
      continue()
    endif()
    list(GET map_TARGET ${idx} cpp_target)
    list(APPEND ir_${cpp_target} "${cpp_source}")
    list(APPEND ir_cpp_targets ${cpp_target})
  endforeach()
  list(REMOVE_DUPLICATES ir_cpp_targets)

  string(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" CMAKE_CXX_FLAGS_mode)
  set(cpp_global_flags "-S -emit-llvm ${CMAKE_CXX_FLAGS} ${${CMAKE_CXX_FLAGS_mode}}")
  if(ir_OPTLEVEL)
    string(REPLACE "-O[0123szg]" "-O${ir_OPTLEVEL}"
      cpp_global_flags ${cpp_global_flags})
  endif()
  _ir_message("global_flags: ${cpp_global_flags}")

  set(ir_compile_commands "${ir_DESTINATION}/IRCompileCommands.${_ir_script_ext}")
  _ir_script_init("${ir_compile_commands}")
  foreach(cpp_target ${ir_cpp_targets})
    set(sources ${ir_${cpp_target}})
    set(outputs ${sources})
    list(TRANSFORM sources PREPEND "${root_folder}/")
    list(TRANSFORM outputs REPLACE "\\.(cpp)$" ".${ir_extension}")
    list(TRANSFORM outputs REPLACE "/" ".")
    
    __ir_get_property(cpp_standard ${cpp_target} CXX_STANDARD)
    __ir_get_property(cpp_includes ${cpp_target} INCLUDE_DIRECTORIES)
    __ir_get_property(cpp_options ${cpp_target} COMPILE_OPTIONS)
    __ir_get_property(cpp_defs ${cpp_target} COMPILE_DEFINITIONS)

    __ir_get_property(cpp_libs ${cpp_target} LINK_LIBRARIES)
    _ir_recurse_libs(${cpp_libs})

    list(APPEND cpp_includes ${cpp_iincludes})
    list(APPEND cpp_options ${cpp_ioptions} "-w")
    list(APPEND cpp_defs ${cpp_idefs})

    list(REMOVE_DUPLICATES cpp_includes)
    list(REMOVE_DUPLICATES cpp_defs)

    list(FILTER cpp_includes EXCLUDE REGEX "^\\$<INSTALL")
    list(TRANSFORM cpp_includes REPLACE "^\\$<[A-Z_]+:(.+)>$" "\\1")
    list(TRANSFORM cpp_includes REPLACE "^(.+)$" "-I\"\\1\"")
    list(FILTER cpp_options EXCLUDE REGEX "^-W[a-zA-Z0-9_-]+")
    list(TRANSFORM cpp_defs PREPEND "-D")

    if(ir_NODEBUG)
      list(FILTER cpp_options EXCLUDE REGEX "^-g$")
    endif()
    if(NOT cpp_standard)
      set(cpp_standard ${CMAKE_CXX_STANDARD})
    endif()

    set(clang_flags ${cpp_global_flags} "-std=c++${cpp_standard}"
      ${cpp_includes} ${cpp_options} ${cpp_defs})
    list(JOIN clang_flags " " clang_flags)

    set(ir_shell_var "${cpp_target}")
    _ir_script_comment("${ir_compile_commands}" "${cpp_target}: ${outputs}")
    _ir_script_var("${ir_compile_commands}" ir_shell_var ${clang_flags})
    _ir_script_ref(ir_shell_var)

    foreach(src out IN ZIP_LISTS sources outputs)
      set(clang_command "${CMAKE_CXX_COMPILER} ${clang_flags} ${src} -o ${out}\n")
      separate_arguments(clang_invocation NATIVE_COMMAND "${clang_command}")
      file(APPEND "${ir_compile_commands}"
        "${CMAKE_CXX_COMPILER} ${ir_shell_var} ${src} -o ${out}\n")
      add_custom_command(
        TARGET ${ir_target} POST_BUILD
        COMMAND ${clang_invocation}
        BYPRODUCTS "${ir_DESTINATION}/${out}"
        COMMENT "Building \"${out}\" IR for '${ir_target}'"
        WORKING_DIRECTORY "${ir_DESTINATION}"
        VERBATIM COMMAND_EXPAND_LISTS
      )
    endforeach()
    file(APPEND "${ir_compile_commands}" "\n")
  endforeach()
endfunction(_ir_handle_roots)

# Generates an ir target from a list of input sources.
#  compile_ir(<target>
#             SOURCES <sources>...
#             ROOTS
#              <ROOT <name>
#                 <folders>...
#                 TARGETS <targets>...
#              >...
#             [DESTINATION <folder>]
#             [OPTLEVEL (0|1|2|3|s|z|g)]
#             [FLAGS [flags...]]
#             [BITCODE]
#             [NODEBUG])
#
# SOURCES:
#  Sources are formatted like "<core|exi>.<folder>[.subfolder].[filename]",
#  eg. "exi.Decode.BodyDecoder"
#  
#  You can use glob patterns on subfolders,
#  eg.
#    "exi.Grammar.Decode.*"
#    "exi.Grammar.*.BuiltinSchema"
#    "exi.Decode.*Decoder"
#
#  However, globs are NOT valid as roots.
#
#  Future work:
#  You can also provide compiler arguments,
#  eg.
#    "exi.Decode.*Decoder[-DENABLE_X=0]"
#    "exi.Decode.*Decoder[-DENABLE_X=1]"
#
# DESTINATION:
#  The destination folder. Defaults to "${CMAKE_BINARY_DIR}/IR".
#  When relative, the destination will be "${CMAKE_BINARY_DIR}/<folder>".
function(compile_ir target_name)
  add_custom_target(${target_name})
  set(ir_opts   BITCODE NODEBUG)
  set(ir_single DESTINATION OPTLEVEL)
  set(ir_multi  ROOTS SOURCES FLAGS)
  cmake_parse_arguments(ir
    "${ir_opts}" "${ir_single}" "${ir_multi}"
    ${ARGN}
  )

  # Validate options
  _ir_check_required(ROOTS SOURCES)
  if(NOT ir_ROOTS)
    _ir_throw("No roots provided!")
  endif()
  if(NOT ir_SOURCES)
    _ir_throw("No sources provided!")
  endif()
  list(REMOVE_DUPLICATES ir_SOURCES)
  _ir_message("TARGET: ${target_name}")

  # Update ir_DESTINATION
  _ir_update_destination(ir_DESTINATION)
  _ir_message("DESTINATION: ${ir_DESTINATION}")

  if(ir_OPTLEVEL)
    string(TOLOWER "${ir_OPTLEVEL}" ir_OPTLEVEL)
    if(NOT ir_OPTLEVEL MATCHES "^[0123szg]$")
      message(WARNING "Invalid OPTLEVEL '${ir_OPTLEVEL}', setting to default.")
      unset(ir_OPTLEVEL)
    endif()
  endif()
  if(NOT ir_FLAGS)
    set(ir_FLAGS "")
  endif()

  if(DEFINED ir_BITCODE AND ir_BITCODE)
    set(ir_extension "bc")
  else()
    set(ir_extension "ll")
  endif()
  _ir_message("EXTENSION: .${ir_extension}")

  # Get the ROOT directories
  set(ir_roots "")
  set(ir_ROOTS_current "")
  set(ir_ROOTS_parse "")
  foreach(ROOT_arg IN LISTS ir_ROOTS)
    # COMMANDS:
    if(ROOT_arg STREQUAL "ROOT")
      if(ir_ROOTS_current)
        list(APPEND ir_roots ${ir_ROOTS_current})
        set(ir_ROOTS_current "")
      endif()
      set(ir_ROOTS_parse ROOT)
    elseif(ir_ROOTS_parse STREQUAL "")
      set(ir_ROOTS_parse "${ROOT_arg}")
    # STATEFUL:
    elseif(ir_ROOTS_parse STREQUAL "ROOT")
      set(ir_ROOTS_current "${ROOT_arg}")
      set(${ROOT_arg}_FOLDER "")
      set(${ROOT_arg}_TARGETS "")
      set(ir_ROOTS_parse "FOLDER")
    elseif(ir_ROOTS_parse STREQUAL "FOLDER")
      cmake_path(ABSOLUTE_PATH ROOT_arg NORMALIZE OUTPUT_VARIABLE root_dir)
      if(NOT IS_DIRECTORY "${root_dir}")
        _ir_throw("Invalid folder for '${ir_ROOTS_current}': \"${root_dir}\"")
      endif()
      set(${ir_ROOTS_current}_FOLDER "${root_dir}")
      set(ir_ROOTS_parse "")
    elseif(ir_ROOTS_parse STREQUAL "TARGETS")
      _deref_target(${ROOT_arg} ir_target)
      list(APPEND ${ir_ROOTS_current}_TARGETS ${ir_target})
    else()
      _ir_throw("Unknown argument '${ROOT_arg}'")
    endif()
  endforeach()

  if(NOT ir_ROOTS_current)
    _ir_throw("Unparsed root '${ir_ROOTS_current}'")
  else()
    list(APPEND ir_roots ${ir_ROOTS_current})
  endif()
  if(NOT ir_roots)
    _ir_throw("No roots provided!")
  endif()
  _ir_message("ROOTS: ${ir_roots}")

  set(_ir_message_type VERBOSE)
  list(SORT ir_SOURCES COMPARE STRING)
  foreach(root ${ir_roots})
    if(NOT (DEFINED ${root}_FOLDER AND ${root}_FOLDER))
      _ir_throw("No folder provided for '${root}'")
    endif()
    if(NOT (DEFINED ${root}_TARGETS AND ${root}_TARGETS))
      _ir_throw("No targets provided for '${root}'")
    endif()

    set(root_sources ${ir_SOURCES})
    list(FILTER root_sources INCLUDE REGEX "^${root}\..+$")
    if(NOT root_sources)
      continue()
    endif()
    list(TRANSFORM root_sources REPLACE "^${root}\." "")
    _ir_handle_roots(${target_name} ${root} ${root_sources})
  endforeach()
endfunction(compile_ir)

else()

# IR generation is not supported on this target.
function(compile_ir ir_target)
  add_custom_target(${ir_target})
  _ir_message("IR is not supported on this compiler!")
endfunction(compile_ir)

endif()
