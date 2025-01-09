# Temporary file, when I remove exip I'll move this into the main file.

set(EXI_INVARIANTS   ${EXICPP_INVARIANTS})
set(EXI_EXCEPTIONS   ${EXICPP_EXCEPTIONS})
set(EXI_DEBUG        ${EXICPP_DEBUG})
set(EXI_ANSI         ${EXICPP_ANSI})
set(EXI_USE_MIMALLOC ${EXICPP_USE_MIMALLOC})

if(WIN32)
  if(CYGWIN)
    set(EXI_ON_WIN32 0)
    set(EXI_ON_UNIX 1)
  else()
    set(EXI_ON_WIN32 1)
    set(EXI_ON_UNIX 0)
  endif()
elseif(FUCHSIA OR UNIX)
  set(EXI_ON_WIN32 0)
  set(EXI_ON_UNIX 1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Generic")
  set(EXI_ON_WIN32 0)
  set(EXI_ON_UNIX 0)
else()
  message(SEND_ERROR "Unable to determine platform")
endif()

include_items(EXICPP_CORE "lib/core"
  Common/SmallVec.cpp
  Common/StringExtras.cpp
  Common/StrRef.cpp
  Common/Twine.cpp

  Config/ABIBreak.cpp

  Support/Alloc.cpp
  Support/Allocator.cpp
  Support/Chrono.cpp
  Support/ConvertUTF.cpp
  Support/ConvertUTFWrappers.cpp
  Support/Error.cpp
  Support/ErrorHandle.cpp
  Support/FmtBuffer.cpp
  Support/MemoryBuffer.cpp
  Support/MD5.cpp
  Support/NativeFormatting.cpp
  Support/SafeAlloc.cpp
  Support/VersionTuple.cpp
  Support/raw_ostream.cpp
)

add_subdirectory(include/core)
include_items(EXICPP_SRC "lib/exi")

add_library(exicpp STATIC ${EXICPP_CORE} ${EXICPP_SRC})
add_library(exicpp::exicpp ALIAS exicpp)
target_include_directories(exicpp PUBLIC include include/core)

target_link_libraries(exicpp PUBLIC fmt::fmt rapidxml::rapidxml)
target_compile_features(exicpp PUBLIC cxx_std_20)

if(EXI_USE_MIMALLOC)
  target_link_libraries(exicpp PUBLIC mimalloc-static)
endif()
if(WIN32)
  target_link_libraries(exicpp PRIVATE
    psapi shell32 ole32 uuid advapi32 ws2_32 ntdll)
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
  target_compile_options(exicpp PRIVATE ${EXICPP_WARNING_FLAGS})
endif()

if(PROJECT_IS_TOP_LEVEL OR EXICPP_DRIVER)
  add_executable(exi-driver Driver.cpp)
  target_link_libraries(exi-driver exicpp::exicpp)
endif()
