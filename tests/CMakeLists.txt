include_guard(DIRECTORY)

if(NOT TARGET gtest)
  message(FATAL_ERROR "gtest must be included before tests!")
endif()

if(NOT DEFINED EXI_JAVA_EXECUTABLE)
  find_package(Java 1.8 REQUIRED QUIET COMPONENTS Runtime)
  if(NOT Java_FOUND)
    message(FATAL_ERROR "Java could not be found!")
  elseif(Java_VERSION VERSION_LESS "8.0.0")
    message(FATAL_ERROR "Java must be at least version 8.0.0, got ${Java_VERSION}!")
  endif()
  set(EXI_JAVA_EXECUTABLE "${Java_JAVA_EXECUTABLE}")
endif()

find_package(Java QUIET COMPONENTS Development)
if(NOT Java_Development_FOUND)
  message(FATAL_ERROR "Javac could not be found!")
endif()

if(NOT DEFINED EXI_CLASS_BINARY_DIR)
  set(EXI_CLASS_BINARY_DIR "${CMAKE_BINARY_DIR}/java" CACHE FILEPATH "")
endif()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(NOT TARGET Python3::Interpreter)
  message(FATAL_ERROR "Python3 could not be found!")
endif()

if(NOT DEFINED EXIFICIENT_DIR)
  set(EXIFICIENT_JAR "${EXI_BASE_DIR}/resources/exificient-jar-with-dependencies.jar")
endif()

# TODO: Test the EXI jar is runnable with the java version?
set(TEST_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/s")
set(TEST_OUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/o")

# Build our custom classes
add_subdirectory(jarvis)
add_subdirectory(exiconf)

configure_file(
  run-tests.py.in
  ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py
  @ONLY
)

add_custom_target(exi-tests-target)
#add_custom_command(
#  TARGET tests-target POST_BUILD
#  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py
#  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
#)

add_executable(xml-test-driver driver/old/xml-test-driver.cpp)
target_link_libraries(xml-test-driver PUBLIC exi::exicpp)

add_executable(exi-test-driver driver/old/exi-test-driver.cpp)
target_link_libraries(exi-test-driver PUBLIC exi::exicpp)

add_executable(dual-test-driver driver/TestDriver.cpp)
target_link_libraries(dual-test-driver PUBLIC exi::exicpp)

#add_dependencies(exi-exicpp exi-tests-target)

add_test(
  NAME roundtrip
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/run-tests.py
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMAND_EXPAND_LISTS
)
