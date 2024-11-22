include_guard(DIRECTORY)

include_items(EXICPP_SRC "src"
  BinaryBuffer.cpp
  Errors.cpp
  Filesystem.cpp
  Writer.cpp
  XML.cpp

  Debug/CheckFlags.cpp
  Debug/Format.cpp
  Debug/RapidxmlHandler.cpp
  Debug/Terminal.cpp
)

add_library(exicpp STATIC ${EXICPP_SRC})
add_library(exicpp::exicpp ALIAS exicpp)
target_include_directories(exicpp
  PUBLIC include
  PRIVATE include/exicpp 
)
target_link_libraries(exicpp PUBLIC
  exip::exip fmt::fmt rapidxml::rapidxml
)
target_compile_features(exicpp PUBLIC cxx_std_17)
target_forward_options(exicpp PUBLIC
  EXICPP_DEBUG
  EXICPP_ANSI
)

if(NOT EXICPP_EXCEPTIONS)
  target_compile_definitions(exicpp PUBLIC
    EXICPP_NO_EXCEPTIONS=1
    RAPIDXML_NO_EXCEPTIONS=1
  )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # ...
else()
  target_compile_options(exicpp PRIVATE ${EXICPP_WARNING_FLAGS})
endif()

if(EXICPP_TESTS OR (PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER))
  add_library(exicpp-util STATIC
    utils/CompareXml.cpp
    utils/ExiToXml.cpp
  )
  add_library(exicpp::util ALIAS exicpp-util)
  target_include_directories(exicpp-util PUBLIC utils)
  target_link_libraries(exicpp-util PUBLIC exicpp)
endif()

if(EXICPP_TESTS)
  include(CTest)
  add_subdirectory(test)
endif()

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exicpp::exicpp exicpp::util)
endif()
