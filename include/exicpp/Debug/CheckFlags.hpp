//===- exicpp/Debug/CheckFlags.hpp ----------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
//     limitations under the License.
//
//===----------------------------------------------------------------===//
//
//  Used to check for consistent debug flags at link time.
//  Based on LLVM's methods.
//
//===----------------------------------------------------------------===//

#ifndef EXICPP_DEBUG_CHECKFLAGS_HPP
#define EXICPP_DEBUG_CHECKFLAGS_HPP

#include <exicpp/Features.hpp>

#if EXICPP_MSVC
# define EXICPP_STR_(s) #s
# define EXICPP_STR(s)  EXICPP_STR_(EXICPP_STR)
# pragma detect_mismatch("EXICPP_DEBUG", LLVM_XSTR(EXICPP_DEBUG))
# undef EXICPP_STR
# undef EXICPP_STR_
#else
# if defined(_WIN32) || defined(__CYGWIN__)
#  define EXICPP_HIDDEN static
#  define EXICPP_WEAK
# elif !(defined(_AIX) && defined(__GNUC__) && !defined(__clang__))
#  define EXICPP_HIDDEN __attribute__((visibility("hidden")))
#  define EXICPP_WEAK __attribute__((weak))
# else
#  define EXICPP_HIDDEN
#  define EXICPP_WEAK __attribute__((weak))
# endif
namespace exi {
# if EXICPP_DEBUG
extern char debugModeEnabled;
EXICPP_HIDDEN EXICPP_WEAK
  char* VerifyDebugModeOnLink = &debugModeEnabled;
# else
extern char debugModeDisabled;
EXICPP_HIDDEN EXICPP_WEAK
  char* VerifyDebugModeOnLink = &debugModeDisabled;
# endif // EXICPP_DEBUG
} // namespace exi
# undef EXICPP_WEAK
# undef EXICPP_HIDDEN
#endif // EXICPP_MSVC

#endif // EXICPP_DEBUG_CHECKFLAGS_HPP
