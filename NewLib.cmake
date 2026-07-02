# Temporary file, when I remove exip I'll move this into the main file.

##########################################################################
## Core
##########################################################################

include_items(CORE_COMMON "lib/core"
  Common/APInt.cpp
  Common/APSInt.cpp
  Common/BitSpan.cpp
  Common/FoldingSet.cpp
  Common/InlineArray.cpp
  Common/Option.cpp
  Common/SmallVec.cpp
  Common/StringExtras.cpp
  Common/StringMap.cpp
  Common/StrRef.cpp
  Common/Twine.cpp
)

include_items(CORE_CONFIG "lib/core"
  Config/ABIBreak.cpp
)

include_items(CORE_DEMANGLE "lib/core"
  Demangle/Demangle.cpp
  Demangle/ItaniumDemangle.cpp
  Demangle/MicrosoftDemangle.cpp
  Demangle/MicrosoftDemangleNodes.cpp
  Demangle/RustDemangle.cpp
  Demangle/DLangDemangle.cpp
)

include_items(CORE_SUPPORT "lib/core"
  Support/Alloc.cpp
  Support/Allocator.cpp
  Support/AutoConvert.cpp
  Support/BinaryToText.cpp
  Support/BuryPointer.cpp
  Support/Chrono.cpp
  Support/ConvertUTF.cpp
  Support/ConvertUTFWrappers.cpp
  Support/Debug.cpp
  Support/Error.cpp
  Support/ErrorHandle.cpp
  Support/ExtensibleRTTI.cpp
  Support/FmtBuffer.cpp
  Support/Format.cpp
  Support/InitDriver.cpp
  Support/IntCast.cpp
  Support/ManagedStatic.cpp
  Support/MD5.cpp
  Support/MemoryBuffer.cpp
  Support/MemoryBufferRef.cpp
  Support/NativeFormatting.cpp
  Support/Path.cpp
  Support/Process.cpp
  Support/Program.cpp
  Support/PureVirtual.cpp
  Support/RTTI.cpp
  Support/SafeAlloc.cpp
  Support/Signals.cpp
  Support/Stacktrace.cpp
  Support/StringSaver.cpp
  Support/TokenizeCmd.cpp
  Support/VersionTuple.cpp
  Support/VirtualFilesystem.cpp
  Support/WithColor.cpp
  Support/circular_raw_ostream.cpp
  Support/rapidhash.cpp
  Support/raw_ostream.cpp
)

add_library(exi-core INTERFACE)
add_library(exi::core ALIAS exi-core)

add_library(exi-core-config STATIC ${CORE_CONFIG})
add_library(exi-core::config ALIAS exi-core-config)
exi_add_target_options(exi-core-config)

target_include_directories(exi-core-config PUBLIC include include/core)
target_link_libraries(exi-core-config PUBLIC fmt::fmt exi-cpptrace)
target_link_libraries(exi-core INTERFACE exi-core-config)

if(EXI_USE_MIMALLOC)
  target_link_libraries(exi-core-config PUBLIC mimalloc)
endif(EXI_USE_MIMALLOC)
if(NOT EXI_EXCEPTIONS)
  target_compile_definitions(exi-core-config PUBLIC EXI_NO_EXCEPTIONS=1)
endif(NOT EXI_EXCEPTIONS)

# Generate other core libraries
function(exi_core_add_library lib src)
  # Create the library
  set(LIBNAME exi-core-${lib})
  add_library(${LIBNAME} STATIC ${${src}})
  add_library(exi-core::${lib} ALIAS ${LIBNAME})
  target_link_libraries(exi-core INTERFACE ${LIBNAME})
  # Add options
  exi_add_target_options(${LIBNAME})
  target_link_libraries(${LIBNAME} PUBLIC exi-core-config)
  target_include_directories(${LIBNAME} PRIVATE lib/core)
  if(EXI_USE_MIMALLOC AND EXI_REDIRECT)
    target_compile_definitions(${LIBNAME} PRIVATE EXI_REDIRECT_GLOBAL_NEW=1)
  endif()
  #if(WIN32)
  #  target_link_libraries(${LIBNAME} PRIVATE
  #    ntdll psapi shell32 ole32 uuid advapi32 ws2_32)
  #endif(WIN32)
endfunction(exi_core_add_library)

exi_core_add_library(common CORE_COMMON)
exi_core_add_library(demangle CORE_DEMANGLE)
exi_core_add_library(support CORE_SUPPORT)

target_link_libraries(exi-core-common PRIVATE exi-core::support)
target_link_libraries(exi-core-demangle PRIVATE exi-core::common exi-core::support)
target_link_libraries(exi-core-support PUBLIC exi-core::demangle PRIVATE exi-core::common)

if(WIN32)
  target_link_libraries(exi-core-support PRIVATE
    ntdll psapi shell32 ole32 uuid advapi32 ws2_32)
endif(WIN32)

#add_library(exi-core STATIC ${CORE_COMMON} ${CORE_CONFIG} ${CORE_SUPPORT})
#add_library(exi::core ALIAS exi-core)
#exi_add_target_options(exi-core)
#
#target_include_directories(exi-core
#  PUBLIC include include/core
#  PRIVATE lib/core)
#target_link_libraries(exi-core PUBLIC fmt::fmt exi-cpptrace)
#
#if(EXI_USE_MIMALLOC)
#  target_link_libraries(exi-core PUBLIC mimalloc)
#  if(EXI_REDIRECT)
#    target_compile_definitions(exi-core
#      PRIVATE EXI_REDIRECT_GLOBAL_NEW=1)
#  endif(EXI_REDIRECT)
#endif()
#if(WIN32)
#  target_link_libraries(exi-core PRIVATE
#    ntdll psapi shell32 ole32 uuid advapi32 ws2_32)
#endif()
#
#if(NOT EXI_EXCEPTIONS)
#  target_compile_definitions(exi-core
#    PUBLIC EXI_NO_EXCEPTIONS=1)
#endif()

##########################################################################
## Exicpp
##########################################################################

include_items(EXICPP_BASIC "lib/exi"
  Basic/ErrorCodes.cpp
  Basic/EventTerms.cpp
  Basic/ExiHeader.cpp
  Basic/ExiOptions.cpp
  #Basic/FileEntry.cpp
  #Basic/FileManager.cpp
  Basic/FilesystemStatCache.cpp
  Basic/NBitInt.cpp
  Basic/Runes.cpp
  Basic/XML.cpp
  Basic/XMLCompare.cpp
  Basic/XMLContainer.cpp
  Basic/XMLDumper.cpp
  Basic/XMLManager.cpp
)

include_items(EXICPP_DECODE "lib/exi"
  Decode/BodyDecoder.cpp
  Decode/Deserializer.cpp
  Decode/Grammar.cpp
  Decode/HeaderDecoder.cpp
  Decode/StringTables.cpp
  Decode/XMLDeserializer.cpp
)

include_items(EXICPP_ENCODE "lib/exi"
  Encode/BodyEncoder.cpp
  Encode/CharacterSet.cpp
  Encode/DTDParser.cpp
  Encode/Event.cpp
  Encode/HeaderEncoder.cpp
  Encode/Grammar.cpp
  Encode/OrderedEncoder.cpp
  Encode/NamespaceContextStack.cpp
  Encode/Serializer.cpp
  Encode/StringTables.cpp
  Encode/XMLSerializer.cpp
)

include_items(EXICPP_GRAMMAR "lib/exi"
  Grammar/BIEventMap.cpp
  Grammar/BIState.cpp
  Grammar/SchemaFactory.cpp
  
  Grammar/Encode/Schema.cpp
  Grammar/Encode/BuiltinSchema.cpp
  Grammar/Decode/Schema.cpp
  Grammar/Decode/BuiltinSchema.cpp
)

include_items(EXICPP_STREAM "lib/exi"
  Stream/Stream.cpp
)

set(EXI_LINK_LIBS exi::core rapidxml::rapidxml dtl::dtl)
function(exi_add_library lib src)
  set(LIBNAME exi-${lib})
  add_library(${LIBNAME} STATIC ${${src}})
  add_library(exi::${lib} ALIAS ${LIBNAME})
  exi_add_target_options(${LIBNAME})
  target_include_directories(${LIBNAME}
    PUBLIC include
    PRIVATE lib/core lib/exi)
  target_link_libraries(${LIBNAME} PUBLIC ${EXI_LINK_LIBS})
endfunction(exi_add_library)

exi_add_library(basic EXICPP_BASIC)
list(APPEND EXI_LINK_LIBS exi::basic)
exi_add_library(stream EXICPP_STREAM)
list(APPEND EXI_LINK_LIBS exi::stream)

exi_add_library(decode  EXICPP_DECODE)
exi_add_library(encode  EXICPP_ENCODE)
exi_add_library(grammar EXICPP_GRAMMAR)

target_link_libraries(exi-decode  PRIVATE exi::encode exi::grammar)
target_link_libraries(exi-encode  PRIVATE exi::decode exi::grammar)
target_link_libraries(exi-grammar PRIVATE exi::decode exi::encode)

add_library(exi-exicpp INTERFACE)
add_library(exi::exicpp ALIAS exi-exicpp)
add_library(exi::exi ALIAS exi-exicpp)

target_link_libraries(exi-exicpp INTERFACE
  exi::basic
  exi::decode
  exi::encode
  exi::grammar
  exi::stream
)

if(DEFINED EXI_CODEGEN_TESTS)
  include(CompileIR)
  message(STATUS "")
  if(NOT EXI_CODEGEN_LEVEL)
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
      message(STATUS "EXI_CODEGEN_TESTS will compile in -O2.")
      set(EXI_CODEGEN_LEVEL OPTLEVEL 2)
    else()
      set(EXI_CODEGEN_LEVEL "")
    endif()
  endif()
  if(NOT EXI_CODEGEN_TYPE)
    set(EXI_CODEGEN_TYPE "")
  endif()

  compile_ir(exi-irgen
    SOURCES ${EXI_CODEGEN_TESTS}
    ROOTS
      ROOT core "lib/core"
        TARGETS exi::core
      ROOT exi "lib/exi"
        TARGETS
          exi::basic
          exi::decode
          exi::encode
          exi::grammar
          exi::stream
    ${EXI_CODEGEN_TYPE}
    ${EXI_CODEGEN_LEVEL})
else()
  add_custom_target(exi-irgen)
endif()

if(EXI_TESTS)
  include(TestSetup.cmake)
  add_subdirectory(unittests)
  add_subdirectory(tests)
endif()

macro(exi_add_driver target)
  exi_add_executable(${target} ${ARGN})
  target_link_libraries(${target} PUBLIC exi::exicpp)
  #exi_minject(exi-driver CLASSIC BACKUP)
endmacro(exi_add_driver)

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  exi_add_driver(exi-driver Driver.cpp DriverTests.cpp ADD_PDB)
  exi_minject(exi-driver CLASSIC BACKUP)
  add_dependencies(exi-exicpp exi-irgen)
endif()

#exi_add_driver(exi-xml-compare XMLCompareDriver.cpp)
#exi_add_driver(exi-xml-print XMLPrintDriver.cpp)
