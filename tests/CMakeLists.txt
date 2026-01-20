include_guard(DIRECTORY)

if(NOT TARGET gtest)
  message(FATAL_ERROR "gtest must be included before tests!")
endif()

find_package(Java QUIET COMPONENTS Runtime)
if(NOT Java_FOUND)
  message(FATAL_ERROR "Java could not be found!")
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(NOT TARGET Python3::Interpreter)
  message(FATAL_ERROR "Python3 could not be found!")
endif()

if(NOT DEFINED EXIFICIENT_DIR)
  set(EXIFICIENT_JAR "${EXI_BASE_FOLDER}/resources/exificient-jar-with-dependencies.jar")
endif()

#add_subdirectory(ordered)

configure_file(
  run-tests.py.in
  ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py
  @ONLY
)

add_custom_target(tests-target)
#add_custom_command(
#  TARGET tests-target POST_BUILD
#  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py
#  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#)

add_executable(xml-test-driver xml-test-driver.cpp)
target_link_libraries(xml-test-driver PUBLIC exi::exicpp)

add_executable(exi-test-driver exi-test-driver.cpp)
target_link_libraries(exi-test-driver PUBLIC exi::exicpp)

#add_dependencies(exi-exicpp tests-target)

add_test(
  NAME roundtrip
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMAND_EXPAND_LISTS
)
