include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        b514bdc898e2951020cbdca1304b75f5950d1f59 # 1.15.2
)

enable_testing()

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
if(WIN32 AND (NOT CYGWIN) AND (NOT MINGW))
  set(gtest_disable_pthreads ON)
endif()

FetchContent_MakeAvailable(googletest)
set(TEST_LIBS gtest gtest_main)

add_custom_target(exicpp-tests ALL)
set(EXICPP_TESTS "exicpp-tests")

include(GoogleTest)
add_subdirectory(unit)
