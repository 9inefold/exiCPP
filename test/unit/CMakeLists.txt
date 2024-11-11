include_guard(DIRECTORY)

set(UNITTEST_SRC
  "Comparison.cpp"
)

if(NOT DEFINED EXICPP_EXIFICIENT_DIR)
  message(FATAL_ERROR "'EXICPP_EXIFICIENT_DIR' must be defined!")
endif()

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
  set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Qunused-arguments -fcolor-diagnostics")
	endif()
endif(CCACHE_FOUND)

add_executable(unittests #Driver.cpp 
  ${UNITTEST_SRC}
)
target_link_libraries(unittests ${TEST_LIBS} exicpp)

target_compile_definitions(unittests PUBLIC
  EXICPP_EXIFICIENT_DIR="${EXICPP_EXIFICIENT_DIR}"
  EXICPP_EXIFICIENT="${EXICPP_EXIFICIENT_DIR}/exificient-jar-with-dependencies.jar"
)

add_dependencies(${EXICPP_TESTS} unittests)

add_test(NAME unittests
  COMMAND unittests # --gtest_color=yes
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/test
)

gtest_discover_tests(unittests)
