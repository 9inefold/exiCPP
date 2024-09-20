include_guard(DIRECTORY)
add_subdirectory(src)

project(
  exip
  VERSION 0.5.4
  LANGUAGES C
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  message(FATAL_ERROR "MSVC currently unsupported!")
endif()

valued_option(EXI_CONFIG_TYPE "The target (pc or contiki)" "pc")
option(EXI_GRAMMAR_GENERATION "" ON)
option(EXI_SCHEMA_GRAMMAR_GENERATION "" ON)

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
  PRIVATE ${PRIVATE_INC}
)

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
  target_compile_options(exip PUBLIC -Wpacked -Wall)
  target_include_directories(exip PUBLIC "build/gcc/${EXI_CONFIG_TYPE}")
endif()

add_library(exip::exip ALIAS exip)
