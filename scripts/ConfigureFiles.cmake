include_guard(DIRECTORY)

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
