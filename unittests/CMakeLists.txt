include_guard(DIRECTORY)

if(NOT TARGET gtest)
  message(FATAL_ERROR "gtest must be included before tests!")
endif()

add_custom_target(core-unittests)
function(add_exi_unittest test_dirname)
  add_unittest(core-unittests ${test_dirname} ${ARGN})
endfunction()

add_subdirectory(Common)
add_subdirectory(Support)

add_custom_target(exi-unittests)
function(add_exi_unittest test_dirname)
  add_unittest(exi-unittests ${test_dirname} ${ARGN})
endfunction()
