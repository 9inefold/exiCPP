include_guard(DIRECTORY)

project(
  exi-redirect
  VERSION 0.1.0
  LANGUAGES CXX C
)

if(NOT WIN32)
  message(SEND_ERROR "Redirect should not be used on non-Windows targets!")
endif()

# target_link_options(hcrt-inc INTERFACE -nostdlib++)

add_library(exi-redirect SHARED Driver.cpp)
add_library(exi::redirect ALIAS exi-redirect)

exi_clear_lib_prefix(exi-redirect)
set_target_properties(exi-redirect PROPERTIES
  OUTPUT_NAME "mimalloc-redirect"
)

target_compile_options(exi-redirect PRIVATE
  ${EXICPP_WARNING_FLAGS}
  -shared
  -fno-threadsafe-statics
  -fno-asynchronous-unwind-tables
  -fno-exceptions -fno-rtti
)

target_include_directories(exi-redirect PRIVATE
  ${EXI_BASE_FOLDER}/include/core/Config
  include
)

target_sources(exi-redirect PRIVATE
  lib/Buf.cpp
  lib/Env.cpp
  lib/Globals.cpp
  lib/Logging.cpp
  lib/NtImports.cpp
  lib/Patches.cpp
  lib/Strings.cpp
  lib/Version.cpp
  lib/VMemcpy.cpp
  lib/VMemset.cpp
)

target_link_options(exi-redirect PRIVATE -nostdlib -nodefaultlibs)
target_link_libraries(exi-redirect PRIVATE ntdll)
