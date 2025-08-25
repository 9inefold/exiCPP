include_guard(DIRECTORY)

if(NOT TARGET gtest)
  message(FATAL_ERROR "gtest must be included before tests!")
endif()

##########################################################################
## Core
##########################################################################

add_custom_target(core-unittests)
function(add_exi_unittest test_dirname)
  message(STATUS "Adding ${test_dirname}")
  add_unittest(core-unittests ${test_dirname} ${ARGN})
  # set_target_properties(${test_dirname}Tests PROPERTIES FOLDER "Tests/${core}/Unit")
endfunction()

add_subdirectory(Common)
add_subdirectory(Support)

##########################################################################
## Exicpp
##########################################################################

add_custom_target(exi-unittests)
function(add_exi_unittest test_dirname)
  message(STATUS "Adding ${test_dirname}")
  add_unittest(exi-unittests ${test_dirname} ${ARGN})
endfunction()
