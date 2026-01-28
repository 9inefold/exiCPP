include_guard(DIRECTORY)

#message(STATUS "javac: ${Java_JAVAC_EXECUTABLE}")
#message(STATUS "bindir: ${EXI_CLASS_BINARY_DIR}")

set(JAVA_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/com/exicpp")
set(JAVA_DST_DIR "${EXI_CLASS_BINARY_DIR}/com/exicpp")

set(exi_java_sources)
set(exi_java_outputs)

macro(exi_add_java_source path)
  list(APPEND exi_java_sources "com/exicpp/${path}")
  get_filename_component(_path_name "${path}" NAME_WLE)
  get_filename_component(_dir_name "${path}" DIRECTORY)
  #list(APPEND exi_java_outputs "${EXI_CLASS_BINARY_DIR}/${_path_name}.class")
  list(APPEND exi_java_outputs "${JAVA_DST_DIR}/${_dir_name}/${_path_name}.class")
  unset(_path_name)
  unset(_dir_name)
endmacro(exi_add_java_source)

exi_add_java_source(openexi/CustomSAXParserFactory.java)
exi_add_java_source(openexi/LoggingSAXHandlerWrapper.java)
exi_add_java_source(openexi/Transmogrifier2.java)

set(exi_java_binaries
  "exificient.jar"
  "nagasena.jar" "nagasena-rta.jar"
  "xercesImpl.jar" "xml-apis.jar"
)
# Get absolute location
list(TRANSFORM exi_java_binaries PREPEND "${EXI_BASE_FOLDER}/bin/")
# On windows, it's -cp A;B;C;D
# On other systems, it's -cp A:B:C:D
if(NOT WIN32)
  list(JOIN exi_java_binaries ":" exi_java_binaries)
endif()

add_custom_command(
  OUTPUT ${exi_java_outputs}
  COMMAND Python3::Interpreter clean.py
    ${exi_java_outputs} "${EXI_BASE_FOLDER}/bin/exicpp-custom.jar"
  COMMAND "${Java_JAVAC_EXECUTABLE}"
    -d "${EXI_CLASS_BINARY_DIR}"
    -cp "${exi_java_binaries}"
    -Xlint:-options -g #-verbose
    ${exi_java_sources}
  BYPRODUCTS ${exi_java_outputs}
  DEPENDS ${exi_java_sources}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Compiling java files")

add_custom_target(exi-java-targets DEPENDS ${exi_java_outputs})
add_dependencies(exi-exicpp exi-java-targets)

add_custom_command(
  OUTPUT "${EXI_BASE_FOLDER}/bin/exicpp-custom.jar"
  COMMAND "${Java_JAR_EXECUTABLE}"
    --create --file="${EXI_BASE_FOLDER}/bin/exicpp-custom.jar"
    -C "${EXI_CLASS_BINARY_DIR}/" .
  BYPRODUCTS "${EXI_BASE_FOLDER}/bin/exicpp-custom.jar"
  DEPENDS exi-java-targets
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMAND_EXPAND_LISTS
  COMMENT "Bundling java files")

add_custom_target(exi-jar-targets DEPENDS "${EXI_BASE_FOLDER}/bin/exicpp-custom.jar")
add_dependencies(exi-exicpp exi-jar-targets)
