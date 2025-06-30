option(EXIP_FORCE_SCOPE  "If the exip namespace should be forced." ON)

set_if_unset(EXICPP_WARNING_FLAGS ${EXI_WARNING_FLAGS})
set_if_unset(EXICPP_INVARIANTS    ${EXI_INVARIANTS})
set_if_unset(EXICPP_EXCEPTIONS    ${EXI_EXCEPTIONS})
set_if_unset(EXICPP_TESTS         ${EXI_TESTS})
set_if_unset(EXICPP_DEBUG         ${EXI_DEBUG})
set_if_unset(EXICPP_ANSI          ${EXI_ANSI})
set_if_unset(EXICPP_USE_MIMALLOC  ${EXI_USE_MIMALLOC})

set(EXICPP_DEBUG_BUILD            ${EXI_DEBUG_BUILD})
set(EXICPP_FAST_DEBUG             ${EXI_FAST_DEBUG})

set(EXIP_USE_MIMALLOC             ${EXI_USE_MIMALLOC})
set(EXIP_DEBUG                    ${EXI_DEBUG})
if(EXI_ANSI)
  set(EXIP_ANSI ON CACHE BOOL "")
endif()
