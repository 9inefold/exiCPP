include_guard(GLOBAL)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/scripts")
if (PROJECT_IS_TOP_LEVEL)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

function(exi_set_lib_prefix target value)
  if(TARGET ${target})
    set_target_properties(${target} PROPERTIES
      IMPORT_PREFIX "${value}"  
      PREFIX "${value}"
    )
  else()
    message(WARNING "Target ${target} does not exist!")
  endif()
endfunction(exi_set_lib_prefix)

function(exi_clear_lib_prefix target)
  exi_set_lib_prefix(${target} "")
endfunction(exi_clear_lib_prefix)
