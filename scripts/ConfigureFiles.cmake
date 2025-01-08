include_guard(DIRECTORY)

macro(set_parent)
  foreach(arg ${ARGN})
    set(${arg} ${${arg}} PARENT_SCOPE)
  endforeach()
endmacro()

function(include_items name folder)
  set(LIST_SRCS ${ARGN})
  string(REGEX REPLACE "^\/|\/$" "" folder ${folder})
  set(folder "${CMAKE_CURRENT_SOURCE_DIR}/${folder}")
  list(TRANSFORM LIST_SRCS PREPEND "${folder}/")
  set(${name} "${LIST_SRCS}" PARENT_SCOPE)
endfunction()

macro(list_get ls idx out)
  list(GET ${ls} ${idx} ${out})
endmacro(list_get)

function(list_get_or ls idx alt out)
  list(LENGTH ${ls} _LIST_LEN)
  if(${idx} LESS _LIST_LEN)
    list_get(${ls} ${idx} _LIST_OUT)
  else()
    set(_LIST_OUT ${alt})
  endif()
  set(${out} ${_LIST_OUT} PARENT_SCOPE)
endfunction(list_get_or)

# Calls `configure_file(...)` on the input name.
# Must have a `.in` extension, as it will be removed.
# Newline style is always LF.
#  configure_inline(<filename> [args...])
function(configure_inline name)
  cmake_path(NORMAL_PATH name)
  cmake_path(IS_ABSOLUTE name name_IS_ABSOLUTE)
  string(REGEX REPLACE "(.+)\.in$" "\\1" name_REPLACED ${name})

  if(NOT name_IS_ABSOLUTE)
    set(name_REPLACED "${CMAKE_CURRENT_SOURCE_DIR}/${name_REPLACED}")
  endif()
  
  configure_file(${name} ${name_REPLACED}
    ${ARGN}
    NEWLINE_STYLE LF
  )
endfunction(configure_inline)
