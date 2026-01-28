include_guard(GLOBAL)
include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        b514bdc898e2951020cbdca1304b75f5950d1f59 # 1.15.2
)

include(CTest)
enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
if(WIN32 AND (NOT CYGWIN) AND (NOT MINGW))
  set(gtest_disable_pthreads ON)
endif()

FetchContent_MakeAvailable(googletest)
include(GoogleTest)

function(add_unittest test_suite test_name)
  set(test_target_name "${test_name}Tests")
  get_target_property(test_suite_NAME ${test_suite} EXI_SUBPROJECT_NAME)

  add_executable(${test_target_name} ${EXI_BASE_DIR}/unittests/TestDriver.cpp ${ARGN})
  target_link_libraries(${test_target_name} PRIVATE gtest)
  #add_executable(${test_target_name} ${ARGN})
  #target_link_libraries(${test_target_name} PRIVATE gtest gtest_main)
  target_compile_options(${test_target_name} PRIVATE ${EXI_WARNING_FLAGS})
  set_target_properties(${test_target_name} PROPERTIES
    #FOLDER "${test_suite_NAME}/Tests/Unit"
    EXI_SUBPROJECT_NAME "${test_suite_NAME}"
  )

  #get_target_property(test_DIR ${test_target_name} FOLDER)
  #message(STATUS "${test_name}-FOLDER: ${test_DIR}")

  #add_test(NAME ${test_name} COMMAND $<TARGET_FILE:${test_target_name}>)
  #set_property(TEST ${test_name} APPEND PROPERTY LABELS ${test_suite_NAME})
  #set_tests_properties(${test_name} PROPERTIES LABELS "${test_suite_NAME}")
  add_dependencies(${test_suite} ${test_target_name})
  gtest_discover_tests(${test_target_name}
    TEST_PREFIX "${test_suite_NAME}.${test_name}."
    PROPERTIES LABELS "${test_suite_NAME}"
  )

  #get_test_property(${test_name} LABELS test_LABELS)
  #message(STATUS "${test_name}-LABELS: '${test_LABELS}'")
endfunction(add_unittest test_suite test_name)
