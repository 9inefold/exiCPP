include_guard(DIRECTORY)

set(exi_output_jar "${CMAKE_BINARY_DIR}/bin/exicpp-custom.jar")
set(exi_java_sources)
set(exi_java_outputs)

macro(_exi_do_java_classname_replacements path)
  string(REPLACE "@xerces" "org/apache/xerces" ${path} "${${path}}")
  string(REPLACE "@exicpp" "org/exicpp" ${path} "${${path}}")
  string(REPLACE "@openexi" "org/openexi" ${path} "${${path}}")
endmacro(_exi_do_java_classname_replacements)

# Adds java class to list of sources/outputs. Path should match the package.
#  exi_add_java_class(<classname>)
function(exi_add_java_class classname)
  string(REPLACE "." "/" path ${classname})
  _exi_do_java_classname_replacements(path)
  if(path MATCHES "^([A-Za-z0-9]+(/[A-Za-z0-9]+)*)/([A-Za-z0-9$]+)$")
    # 0: match, 1: package, 2: last slice, 3: class
    set(path ${CMAKE_MATCH_1})
    set(name ${CMAKE_MATCH_3})
  else()
    message(SEND_ERROR "Failed to match on \"${path}\"")
  endif()
  set(exi_java_sources ${exi_java_sources}
    "${path}/${name}.java" PARENT_SCOPE)
  set(exi_java_outputs ${exi_java_outputs}
    "${EXI_CLASS_BINARY_DIR}/${path}/${name}.class" PARENT_SCOPE)
endfunction(exi_add_java_class)

# Adds java classes to list of sources/outputs.
#  exi_add_java_source(<package> <classname> [classnames...])
function(exi_add_java_classes package)
  string(REPLACE "." "/" path ${package})
  _exi_do_java_classname_replacements(path)
  foreach(name IN LISTS ARGN)
    list(APPEND exi_java_sources "${path}/${name}.java")
    list(APPEND exi_java_outputs "${EXI_CLASS_BINARY_DIR}/${path}/${name}.class")
  endforeach()
  set(exi_java_sources ${exi_java_sources} PARENT_SCOPE)
  set(exi_java_outputs ${exi_java_outputs} PARENT_SCOPE)
endfunction(exi_add_java_classes)

exi_add_java_class(@exicpp.util.EvilDocumentHijacker)
exi_add_java_class(@exicpp.util.Log)
exi_add_java_class(@exicpp.util.LogLevel)
exi_add_java_class(@exicpp.util.ReflectionHelpers)
#exi_add_java_class(@exicpp.util.StringTranslator)
exi_add_java_class(@exicpp.util.XConstants)

#exi_add_java_class(@exicpp.openexi.CustomSAXParserFactory)
exi_add_java_class(@exicpp.openexi.DirectSAXHandler)
#exi_add_java_class(@exicpp.openexi.LoggingSAXHandlerWrapper)
exi_add_java_class(@openexi.sax.Transmogrifier2)

exi_add_java_class(@xerces.impl.XMLEEDocumentScanner)
exi_add_java_class(@xerces.impl.XMLEENSDocumentScanner)
exi_add_java_class(@xerces.jaxp.SAXParserImpl2)
exi_add_java_class(@xerces.jaxp.SAXParserFactoryImpl2)
exi_add_java_class(@xerces.parsers.EEAwareParserConfiguration)

set(exi_java_binaries
  ${exi_java_binaries} "exificient.jar"
  "nagasena.jar" "nagasena-rta.jar"
  "xercesImpl.jar" "xml-apis.jar"
)
# Get absolute location
list(TRANSFORM exi_java_binaries PREPEND "${EXI_BASE_DIR}/bin/")
# On windows, it's -cp A;B;C;D
# On other systems, it's -cp A:B:C:D
if(NOT WIN32)
  list(JOIN exi_java_binaries ":" exi_java_binaries)
endif()

add_custom_command(
  OUTPUT ${exi_java_outputs}
  COMMAND Python3::Interpreter clean.py
    -q --files "${exi_output_jar}" #${exi_java_outputs}
  COMMAND "${Java_JAVAC_EXECUTABLE}"
    -d "${EXI_CLASS_BINARY_DIR}"
    -cp "${exi_java_binaries}"
    -Xlint:-options -Xlint:unchecked -g #-verbose
    "--add-exports=java.base/jdk.internal.vm.annotation=ALL-UNNAMED"
    ${exi_java_sources}
  BYPRODUCTS ${exi_java_outputs}
  DEPENDS ${exi_java_sources}
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMENT "Compiling java files")

if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.20.0")
  foreach(classfile in LISTS exi_java_outputs)
    set_source_files_properties(${classfile} PROPERTIES GENERATED 1)
  endforeach()
endif()

add_custom_target(exi-java-targets DEPENDS ${exi_java_outputs})
add_dependencies(exi-exicpp exi-java-targets)

add_custom_command(
  OUTPUT "${exi_output_jar}"
  COMMAND "${Java_JAR_EXECUTABLE}"
    --create --file="${exi_output_jar}"
    -C "${EXI_CLASS_BINARY_DIR}/" .
  BYPRODUCTS "${exi_output_jar}"
  DEPENDS exi-java-targets
  WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
  COMMAND_EXPAND_LISTS
  COMMENT "Bundling java files")

add_custom_target(exi-jar-targets DEPENDS "${exi_output_jar}")
add_dependencies(exi-exicpp exi-jar-targets)
