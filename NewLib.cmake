# Temporary file, when I remove exip I'll move this into the main file.

include_items(EXICPP_CORE "lib/core"
  Common/APInt.cpp
  Common/APSInt.cpp
  Common/Option.cpp
  Common/SmallVec.cpp
  Common/StringExtras.cpp
  Common/StringMap.cpp
  Common/StrRef.cpp
  Common/Twine.cpp

  Config/ABIBreak.cpp

  Support/Alloc.cpp
  Support/Allocator.cpp
  Support/AutoConvert.cpp
  Support/Chrono.cpp
  Support/ConvertUTF.cpp
  Support/ConvertUTFWrappers.cpp
  Support/Debug.cpp
  Support/Error.cpp
  Support/ErrorHandle.cpp
  Support/FmtBuffer.cpp
  Support/Format.cpp
  Support/ManagedStatic.cpp
  Support/MD5.cpp
  Support/MemoryBuffer.cpp
  Support/MemoryBufferRef.cpp
  Support/NativeFormatting.cpp
  Support/Path.cpp
  Support/Process.cpp
  Support/Program.cpp
  Support/SafeAlloc.cpp
  Support/Signals.cpp
  Support/StringSaver.cpp
  Support/TokenizeCmd.cpp
  Support/VersionTuple.cpp
  Support/circular_raw_ostream.cpp
  Support/rapidhash.cpp
  Support/raw_ostream.cpp
)

##########################################################################

add_library(exi-core STATIC ${EXICPP_CORE})
add_library(exi::core ALIAS exi-core)

target_include_directories(exi-core PUBLIC include include/core)
target_link_libraries(exi-core PUBLIC fmt::fmt)
target_compile_features(exi-core PUBLIC cxx_std_20)
target_compile_options(exi-core
  PUBLIC ${EXI_FLAGS}
  PRIVATE ${EXI_WARNING_FLAGS})

if(EXI_USE_MIMALLOC)
  target_link_libraries(exi-core PUBLIC mimalloc)
endif()
if(WIN32)
  target_link_libraries(exi-core PRIVATE
    ntdll psapi shell32 ole32 uuid advapi32 ws2_32)
endif()

if(NOT EXI_EXCEPTIONS)
  target_compile_definitions(exi-core PUBLIC
    EXI_NO_EXCEPTIONS=1
    RAPIDXML_NO_EXCEPTIONS=1
  )
endif()

##########################################################################

include_items(EXICPP_SRC "lib/exi"
  Basic/ErrorCodes.cpp
  Basic/NBitInt.cpp

  Stream/BitStreamIn.cpp
  Stream/BitStreamOut.cpp
)

add_library(exicpp STATIC ${EXICPP_SRC})
add_library(exi::exicpp ALIAS exicpp)

target_include_directories(exicpp PUBLIC include)
target_link_libraries(exicpp PUBLIC exi::core rapidxml::rapidxml)
target_compile_options(exicpp PRIVATE ${EXI_WARNING_FLAGS})

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exi::exicpp)
  exi_minject(exi-driver CLASSIC BACKUP)
endif()
