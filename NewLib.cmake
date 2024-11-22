# Temporary file, when I remove exip I'll move this into the main file.

set(EXI_INVARIANTS ${EXICPP_INVARIANTS})
set(EXI_EXCEPTIONS ${EXICPP_EXCEPTIONS})
set(EXI_DEBUG      ${EXICPP_DEBUG})
set(EXI_ANSI       ${EXICPP_ANSI})
set(EXI_USE_MIMALLOC ${EXICPP_USE_MIMALLOC})

include_items(EXICPP_CORE "lib/core"
  Support/Alloc.cpp
)

include_items(EXICPP_SRC "lib/exi")

add_library(exicpp STATIC ${EXICPP_CORE} ${EXICPP_SRC})
add_library(exicpp::exicpp ALIAS exicpp)
target_include_directories(exicpp PUBLIC include include/core)

target_link_libraries(exicpp PUBLIC fmt::fmt rapidxml::rapidxml)
if(EXI_USE_MIMALLOC)
  target_link_libraries(exicpp PUBLIC mimalloc-static)
endif()

target_compile_features(exicpp PUBLIC cxx_std_20)
target_forward_options(exicpp PUBLIC
  EXI_ANSI
  EXI_DEBUG
  EXI_INVARIANTS
  EXI_USE_MIMALLOC
)

if(NOT EXI_EXCEPTIONS)
  target_compile_definitions(exicpp PUBLIC
    EXI_NO_EXCEPTIONS=1
    RAPIDXML_NO_EXCEPTIONS=1
  )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # ...
else()
  target_compile_options(exicpp PRIVATE ${EXICPP_WARNING_FLAGS})
endif()

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exicpp::exicpp)
endif()
