include_guard(DIRECTORY)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

include_items(EXI_GDB_SOURCES "gdb"
  "core_demangle.py"
)

if(EXI_ADD_DEBUG_VISUALIZERS)
  if(MSVC_IDE)
    message(WARNING ".natvis files not created!")
  else()
    find_package(Python3 QUIET COMPONENTS Interpreter)
    if(TARGET Python3::Interpreter)
      add_subdirectory(gdb)
      exi_generate_pretty_printers_asm(gdb-unified.c ${EXI_GDB_SOURCES})
      add_library(exi-visualizers INTERFACE)
      target_sources(exi-visualizers INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/gdb-unified.c)
      add_library(exi-visualizers-sink STATIC ${CMAKE_CURRENT_BINARY_DIR}/gdb-unified.c)
    endif()
  endif()
endif(EXI_ADD_DEBUG_VISUALIZERS)

if(NOT TARGET exi-visualizers)
  add_library(exi-visualizers INTERFACE)
endif()
add_library(exi::visualizers ALIAS exi-visualizers)
