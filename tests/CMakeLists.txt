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

if(DEFINED EXIFICIENT_DIR)
  set(EXIFICIENT_JAR "${EXIFICIENT_DIR}/exificient-jar-with-dependencies.jar")
  if (NOT EXISTS "${EXIFICIENT_JAR}")
    message(FATAL_ERROR "Could not find exificient-jar-with-dependencies.jar in EXIFICIENT_DIR!")
  endif()
else()
  set(EXIFICIENT_JAR "${EXI_BASE_FOLDER}/resources/exificient-jar-with-dependencies.jar")
endif()

add_subdirectory(ordered)

add_test(
  NAME roundtrip
  COMMAND Python3::Interpreter ${CMAKE_CURRENT_SOURCE_DIR}/Tests.py
    --java ${Java_JAVA_EXECUTABLE}
    --jar ${EXIFICIENT_JAR}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMAND_EXPAND_LISTS
)
