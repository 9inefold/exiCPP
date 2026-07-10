include_guard(DIRECTORY)
set_property(GLOBAL PROPERTY EXI_gdb_generate_script "${CMAKE_CURRENT_LIST_DIR}/generate_gdb_embed.py")

function(exi_generate_pretty_printers_asm outfile)
  if(NOT IS_ABSOLUTE "${outfile}")
    set(outfile "${CMAKE_CURRENT_BINARY_DIR}/${outfile}")
  endif()
  get_property(exi_gdb_generate_script GLOBAL PROPERTY EXI_gdb_generate_script)
  # Create a .bin file with a .debug_gdb_scripts section and generate a .o file from it
  add_custom_command(
    OUTPUT "${outfile}"
    COMMAND
      Python3::Interpreter "${exi_gdb_generate_script}" "${outfile}" ${ARGN}
    WORKING_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}"
    BYPRODUCTS "${outfile}"
    DEPENDS ${ARGN}
    COMMENT "Embedding GDB pretty-printer ${outfile}"
    VERBATIM)

  if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20.0")
    set_source_files_properties(${outfile} PROPERTIES GENERATED 1)
  endif()

  #add_custom_target("gdb_${target}" DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${outfile})
  #add_library(${target} STATIC ${CMAKE_CURRENT_BINARY_DIR}/${outfile})
  #add_dependencies(${target} "gdb_${target}")
endfunction(exi_generate_pretty_printers_asm)
