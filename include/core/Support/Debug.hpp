//===- Support/Debug.hpp ---------------------------------------------===//
//
// MODIFIED FOR THE PURPOSES OF THE EXICPP LIBRARY.
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------===//
//
// Copyright (C) 2024 Eightfold
//
// Relicensed under the Apache License, Version 2.0 (the "License");
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
// This file implements a handy way of adding debugging information to your
// code, without it being enabled all of the time, and without having to add
// command line options to enable it.
//
// In particular, just wrap your code with the DEBUG_ONLY() macro, and it will
// be enabled automatically if you specify '-debug' on the command-line.
// DEBUG_ONLY() requires the DEBUG_TYPE macro to be defined. Set it to "foo"
// specify that your debug code belongs to class "foo". Be careful that you only
// do this after including Debug.hpp and not around any #include of headers.
// Headers should define and undef the macro acround the code that needs to use
// the DEBUG_ONLY() macro. Then, on the command line, you can specify
// '-debug-only=foo' to enable JUST the debug information for the foo class.
//
// When compiling without assertions, the -debug-* options and all code in
// DEBUG_ONLY() statements disappears, so it does not affect the runtime of the
// code.
//
//===----------------------------------------------------------------===//

#pragma once

#include <Config/Config.inc>

namespace exi {

class raw_ostream;

#if EXI_DEBUG

/// isCurrentDebugType - Return true if the specified string is the debug type
/// specified on the command line, or if none was specified on the command line
/// with the -debug-only=X option.
///
bool isCurrentDebugType(const char *Type);

/// setCurrentDebugType - Set the current debug type, as if the -debug-only=X
/// option were specified.  Note that DebugFlag also needs to be set to true for
/// debug output to be produced.
///
void setCurrentDebugType(const char *Type);

/// setCurrentDebugTypes - Set the current debug type, as if the
/// -debug-only=X,Y,Z option were specified. Note that DebugFlag
/// also needs to be set to true for debug output to be produced.
///
void setCurrentDebugTypes(const char **Types, unsigned Count);

/// DEBUG_WITH_TYPE macro - This macro should be used by passes to emit debug
/// information.  If the '-debug' option is specified on the commandline, and if
/// this is a debug build, then the code specified as the option to the macro
/// will be executed.  Otherwise it will not be.  Example:
///
/// DEBUG_WITH_TYPE("bitset", dbgs() << "Bitset contains: " << Bitset << "\n");
///
/// This will emit the debug information if -debug is present, and -debug-only
/// is not specified, or is specified as "bitset".
# define DEBUG_WITH_TYPE(TYPE, ...)                                            \
do {                                                                           \
  if (::exi::DebugFlag && ::exi::isCurrentDebugType(TYPE)) {                   \
    __VA_ARGS__;                                                               \
  }                                                                            \
} while (false)

#else
# define isCurrentDebugType(X) (false)
# define setCurrentDebugType(X) do { (void)(X); } while (false)
# define setCurrentDebugTypes(X, N) do { (void)(X); (void)(N); } while (false)
# define DEBUG_WITH_TYPE(TYPE, ...) do { } while (false)
#endif

/// This boolean is set to true if the '-debug' command line option
/// is specified.  This should probably not be referenced directly, instead, use
/// the DEBUG macro below.
///
extern bool DebugFlag;

/// EnableDebugBuffering - This defaults to false.  If true, the debug
/// stream will install signal handlers to dump any buffered debug
/// output.  It allows clients to selectively allow the debug stream
/// to install signal handlers if they are certain there will be no
/// conflict.
///
extern bool EnableDebugBuffering;

/// dbgs() - This returns a reference to a raw_ostream for debugging
/// messages.  If debugging is disabled it returns errs().  Use it
/// like: dbgs() << "foo" << "bar";
raw_ostream &dbgs();

// This macro should be used by passes to emit debug information.
// If the '-debug' option is specified on the commandline, and if this is a
// debug build, then the code specified as the option to the macro will be
// executed.  Otherwise it will not be.  Example:
//
// DEBUG_ONLY(dbgs() << "Bitset contains: " << Bitset << "\n");
//
#define DEBUG_ONLY(...) DEBUG_WITH_TYPE(DEBUG_TYPE, __VA_ARGS__)

} // namespace exi
