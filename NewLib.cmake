# Temporary file, when I remove exip I'll move this into the main file.

include_items(EXICPP_CORE "lib/core"
  Common/Option.cpp
  Common/SmallVec.cpp
  Common/StringExtras.cpp
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
  Support/raw_ostream.cpp
)

include_items(EXICPP_SRC "lib/exi"

)

add_library(exicpp STATIC ${EXICPP_CORE} ${EXICPP_SRC})
add_library(exi::exicpp ALIAS exicpp)

target_include_directories(exicpp PUBLIC include include/core)

target_link_libraries(exicpp PUBLIC fmt::fmt rapidxml::rapidxml)
target_compile_features(exicpp PUBLIC cxx_std_20)

if(EXI_USE_MIMALLOC)
  target_link_libraries(exicpp PUBLIC mimalloc)
  #target_link_libraries(exicpp PUBLIC mimalloc-static)
endif()
if(WIN32)
  target_link_libraries(exicpp PRIVATE
    # exi::redirect
    ntdll psapi shell32 ole32 uuid advapi32 ws2_32)
endif()

if(NOT EXI_EXCEPTIONS)
  target_compile_definitions(exicpp PUBLIC
    EXI_NO_EXCEPTIONS=1
    RAPIDXML_NO_EXCEPTIONS=1
  )
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # ...
else()
  target_compile_options(exicpp PRIVATE ${EXI_WARNING_FLAGS})
endif()

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exi::exicpp)
  exi_minject(exi-driver BACKUP)
endif()
