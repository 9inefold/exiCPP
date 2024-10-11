include_guard(DIRECTORY)

project(
  exip
  VERSION 0.5.5
  LANGUAGES C
)

valued_option(EXIP_CONFIG_TYPE "The target (pc or contiki)" "pc")
set(CONFIG_FOLDER "build/gcc/${EXIP_CONFIG_TYPE}")

option(EXIP_DEBUG "Enable debug printing." ON)
option(EXIP_ANSI "Enable ANSI codes." OFF)
valued_option(EXIP_DEBUG_LEVEL "The debugging level (INFO, WARNING or ERROR)" "INFO")

option(EXIP_NEW_HASHTABLE "If new hashtable should be used." ON)
option(EXIP_INLINE_HASHKEY "If keys should be inlined for better cache utilization." ON)

option(EXI_GRAMMAR_GENERATION "" ON)
option(EXI_SCHEMA_GRAMMAR_GENERATION "" ON)

message(STATUS "[exip] Config type: ${EXIP_CONFIG_TYPE}")
if(EXIP_DEBUG)
  message(STATUS "[exip] Debug level: ${EXIP_DEBUG_LEVEL}")
else()
  message(STATUS "[exip] Debug level: FATAL")
endif()
message(STATUS "[exip] New hashtable: ${EXIP_NEW_HASHTABLE}")
message(STATUS "[exip] Grammar generation: ${EXI_GRAMMAR_GENERATION}")
message(STATUS "[exip] Schema grammar generation: ${EXI_SCHEMA_GRAMMAR_GENERATION}")

add_subdirectory(src)

include_items(PRIVATE_INC "src"
  "common/include"
  "contentIO/include"
  "grammar/include"
  "grammarGen/include"
  "streamIO/include"
  "stringTables/include"
)

add_library(exip STATIC
  ${COMMON_SRC}
  ${CONTENT_IO_SRC}
  ${GRAMMAR_SRC}
  ${STREAM_IO_SRC}
  ${STRING_TABLES_SRC}
)

target_include_directories(exip
  PUBLIC include
  PRIVATE include/exip ${PRIVATE_INC}
)

target_compile_features(exip PRIVATE c_std_99)
target_forward_options(exip
  PUBLIC
    EXIP_DEBUG EXIP_DEBUG_LEVEL EXIP_ANSI
  PRIVATE
    EXIP_NEW_HASHTABLE
)

if(EXIP_USE_MIMALLOC)
  target_link_libraries(exip PUBLIC mimalloc-static)
  target_compile_definitions(exip PUBLIC -DEXIP_USE_MIMALLOC=1)
endif()

if(EXI_GRAMMAR_GENERATION AND NOT EXI_CONFIG_TYPE STREQUAL "contiki")
  # target_include_directories(exip PRIVATE "src/grammarGen/include")
  target_sources(exip PRIVATE ${GRAMMAR_GEN_SRC})
  if (EXI_SCHEMA_GRAMMAR_GENERATION)
    target_sources(exip PRIVATE ${GRAMMAR_GEN_SCHEMA_SRC})
    target_compile_definitions(exip PUBLIC GRAMMAR_GEN_SCHEMA=1)
  endif()
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # ...
else()
  target_compile_options(exip PUBLIC -Wpacked PRIVATE -Wall)
  target_include_directories(exip PUBLIC ${CONFIG_FOLDER})
  if(EXI_CONFIG_TYPE STREQUAL "contiki")
    target_sources(exip PRIVATE ${CONFIG_FOLDER}/d_mem.c)
  endif()
endif()

if(EXIP_FORCE_SCOPE)
  target_compile_definitions(exip PUBLIC EXIP_SCOPE_NS=1)
endif()

add_library(exip::exip ALIAS exip)
